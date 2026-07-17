#include "AEExposureGrid.h"

#include "AEHeatmapGrid.h"

namespace AEExposureGridPrivate
{
	constexpr double StateEpsilon = 1.0e-8;
	constexpr double RawRegressionTolerance = 1.0e-6;

	/* Returns one normalized bounded contribution with a validated positive denominator. */
	double Normalize(const double Value, const double Reference)
	{
		const double NormalizedValue = FMath::Clamp(Value / Reference, 0.0, 1.0);
		return NormalizedValue;
	}

	/* Returns true when two deterministic state values differ materially. */
	bool Changed(const double Before, const double After)
	{
		const bool bChanged = !FMath::IsNearlyEqual(Before, After, StateEpsilon);
		return bChanged;
	}
}

/* Validate alignment and allocate contiguous M3 Cell storage. */
bool FAEExposureGrid::Initialize(const FAEHeatmapGridConfig& InConfig)
{
	// Reject invalid dimensions, units, and allocations before mutation.
	if (InConfig.Dimensions.X <= 0
		|| InConfig.Dimensions.Y <= 0
		|| !FMath::IsFinite(InConfig.CellSizeCm)
		|| InConfig.CellSizeCm <= 0.0f)
	{
		return false;
	}
	const int64 CellCount = static_cast<int64>(InConfig.Dimensions.X) * InConfig.Dimensions.Y;
	if (CellCount <= 0 || CellCount > MAX_int32)
	{
		return false;
	}

	// Mirror M1 placement so coordinate and world queries address the same Cell.
	Config = InConfig;
	const FVector2D Extent(
		static_cast<double>(Config.Dimensions.X) * Config.CellSizeCm * 0.5,
		static_cast<double>(Config.Dimensions.Y) * Config.CellSizeCm * 0.5);
	WorldMin = Config.WorldCenter - Extent;
	Cells.SetNum(static_cast<int32>(CellCount));
	ActiveFlags.Init(false, Cells.Num());
	ProcessingFlags.Init(false, Cells.Num());
	Reset();
	return true;
}

/* Clear M3 values and revisions without changing Grid placement. */
void FAEExposureGrid::Reset()
{
	// Restore every Cell and both deterministic bit sets.
	for (FCellState& Cell : Cells)
	{
		Cell = FCellState();
	}
	ActiveFlags.Init(false, Cells.Num());
	ProcessingFlags.Init(false, Cells.Num());
	ActiveCellCount = 0;
	ExposureRevision = 0;
	ResponseRevision = 0;
}

/* Advance M3 state from stable M1 dirty indices and the shared fixed simulation clock. */
bool FAEExposureGrid::Update(
	const FAEHeatmapGrid& BehaviourGrid,
	const TArray<int32>& DirtyBehaviourCellIndices,
	const double SimulationTimeHours,
	const double DeltaSimulationHours,
	const uint64 BehaviourRevision,
	const FAEM3ParameterSet& Parameters)
{
	check(IsInGameThread());

	// Reject invalid clocks or misaligned grids without mutating state.
	if (Cells.IsEmpty()
		|| BehaviourGrid.GetConfig().Dimensions != Config.Dimensions
		|| !FMath::IsFinite(SimulationTimeHours)
		|| !FMath::IsFinite(DeltaSimulationHours)
		|| SimulationTimeHours < 0.0
		|| DeltaSimulationHours < 0.0)
	{
		return false;
	}

	// Build one stable row-major processing set from active and newly dirty Cells.
	ProcessingFlags = ActiveFlags;
	for (const int32 Index : DirtyBehaviourCellIndices)
	{
		if (ProcessingFlags.IsValidIndex(Index))
		{
			ProcessingFlags[Index] = true;
		}
	}

	bool bAnyExposureChanged = false;
	bool bAnyResponseChanged = false;
	TArray<int32> ExposureChangedIndices;
	TArray<int32> ResponseChangedIndices;

	// Process indices in ascending row-major order regardless of dirty insertion order.
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (!ProcessingFlags[Index])
		{
			continue;
		}

		FCellState& Cell = Cells[Index];
		const bool bWasActive = ActiveFlags[Index];
		const bool bDirty = DirtyBehaviourCellIndices.Contains(Index);
		const double PreviousExposure = Cell.CurrentExposure;
		const double PreviousDamage = Cell.EcologicalDamageRatio;
		const double PreviousDamageRate = Cell.DamageRatePerSimulationHour;
		const double PreviousRecoveryRate = Cell.RecoveryRatePerSimulationHour;
		const double PreviousLowDuration = Cell.LowExposureDurationSimulationHours;

		// Decay accumulated Exposure using simulated hours only.
		if (Cell.CurrentExposure > 0.0 && DeltaSimulationHours > 0.0)
		{
			Cell.CurrentExposure *= FMath::Pow(2.0, -DeltaSimulationHours / Parameters.ExposureHalfLifeSimulationHours);
			if (Cell.CurrentExposure <= AEExposureGridPrivate::StateEpsilon)
			{
				Cell.CurrentExposure = 0.0;
			}
		}

		// Consume each cumulative raw contribution exactly once for dirty Cells.
		if (bDirty)
		{
			const FIntPoint Coordinate(Index % Config.Dimensions.X, Index / Config.Dimensions.X);
			FAEBehaviourCellSnapshot RawSnapshot;
			if (!BehaviourGrid.GetCellSnapshot(Coordinate, RawSnapshot))
			{
				return false;
			}
			const FAERawBehaviourTotals CurrentTotals = MakeRawTotals(RawSnapshot);
			if (HasRawRegression(CurrentTotals, Cell.LastConsumedRawTotals))
			{
				// Rebase the complete raw vector so partial regressions cannot create mixed deltas.
				Cell.LastConsumedRawTotals = CurrentTotals;
				Cell.PassExposure = 0.0;
				Cell.TravelExposure = 0.0;
				Cell.DwellExposure = 0.0;
				Cell.SprintExposure = 0.0;
				Cell.CollectExposure = 0.0;
				Cell.CombatExposure = 0.0;
			}
			else
			{
				// Calculate independent non-negative deltas and remove Sprint from ordinary Travel.
				const double DeltaPass = CurrentTotals.PassCount - Cell.LastConsumedRawTotals.PassCount;
				const double DeltaTravel = CurrentTotals.TravelDistanceMeters - Cell.LastConsumedRawTotals.TravelDistanceMeters;
				const double DeltaDwell = CurrentTotals.DwellSeconds - Cell.LastConsumedRawTotals.DwellSeconds;
				const double DeltaSprint = CurrentTotals.SprintDistanceMeters - Cell.LastConsumedRawTotals.SprintDistanceMeters;
				const double DeltaCollect = CurrentTotals.CollectEventCount - Cell.LastConsumedRawTotals.CollectEventCount;
				const double DeltaCombat = CurrentTotals.CombatEventCount - Cell.LastConsumedRawTotals.CombatEventCount;
				const double DeltaNormalTravel = FMath::Max(DeltaTravel - DeltaSprint, 0.0);

				// Normalize six interpretable components before applying explicit M2 weights.
				Cell.PassExposure = AEExposureGridPrivate::Normalize(DeltaPass, Parameters.ExposurePassReferenceCount);
				Cell.TravelExposure = AEExposureGridPrivate::Normalize(DeltaNormalTravel, Parameters.ExposureTravelDistanceReferenceMeters);
				Cell.DwellExposure = AEExposureGridPrivate::Normalize(DeltaDwell, Parameters.ExposureDwellReferenceSeconds);
				Cell.SprintExposure = AEExposureGridPrivate::Normalize(DeltaSprint, Parameters.ExposureSprintDistanceReferenceMeters);
				Cell.CollectExposure = AEExposureGridPrivate::Normalize(DeltaCollect, Parameters.ExposureCollectEventReferenceCount);
				Cell.CombatExposure = AEExposureGridPrivate::Normalize(DeltaCombat, Parameters.ExposureCombatEventReferenceCount);
				const double Increment =
					Cell.PassExposure * Parameters.ExposurePassWeight
					+ Cell.TravelExposure * Parameters.ExposureTravelDistanceWeight
					+ Cell.DwellExposure * Parameters.ExposureDwellWeight
					+ Cell.SprintExposure * Parameters.ExposureSprintWeight
					+ Cell.CollectExposure * Parameters.ExposureCollectEventWeight
					+ Cell.CombatExposure * Parameters.ExposureCombatEventWeight;
				Cell.CurrentExposure = FMath::Clamp(Cell.CurrentExposure + Increment, 0.0, Parameters.ExposureMaximum);
				Cell.LastConsumedRawTotals = CurrentTotals;
			}
			Cell.SourceBehaviourRevision = BehaviourRevision;
		}

		// Evaluate mutually exclusive Damage, Recovery, and neutral-band behavior.
		Cell.DamageRatePerSimulationHour = EvaluateDamageRate(Cell.CurrentExposure, Parameters);
		Cell.RecoveryRatePerSimulationHour = 0.0;
		if (Cell.DamageRatePerSimulationHour > 0.0)
		{
			Cell.LowExposureDurationSimulationHours = 0.0;
		}
		else if (Cell.CurrentExposure < Parameters.RecoveryActivationExposure)
		{
			Cell.LowExposureDurationSimulationHours += DeltaSimulationHours;
			if (Cell.LowExposureDurationSimulationHours + UE_DOUBLE_SMALL_NUMBER >= Parameters.RecoveryDelaySimulationHours)
			{
				Cell.RecoveryRatePerSimulationHour = Parameters.RecoveryBaseRatePerSimulationHour;
			}
		}
		else
		{
			Cell.LowExposureDurationSimulationHours = 0.0;
		}

		// Integrate one bounded logical Damage state with the shared simulation step.
		Cell.EcologicalDamageRatio = FMath::Clamp(
			Cell.EcologicalDamageRatio
			+ Cell.DamageRatePerSimulationHour * DeltaSimulationHours
			- Cell.RecoveryRatePerSimulationHour * DeltaSimulationHours,
			0.0,
			1.0);

		const bool bExposureChanged = AEExposureGridPrivate::Changed(PreviousExposure, Cell.CurrentExposure) || bDirty;
		const bool bResponseChanged = AEExposureGridPrivate::Changed(PreviousDamage, Cell.EcologicalDamageRatio)
			|| AEExposureGridPrivate::Changed(PreviousDamageRate, Cell.DamageRatePerSimulationHour)
			|| AEExposureGridPrivate::Changed(PreviousRecoveryRate, Cell.RecoveryRatePerSimulationHour)
			|| AEExposureGridPrivate::Changed(PreviousLowDuration, Cell.LowExposureDurationSimulationHours);
		if (bExposureChanged)
		{
			bAnyExposureChanged = true;
			ExposureChangedIndices.Add(Index);
		}
		if (bResponseChanged)
		{
			bAnyResponseChanged = true;
			ResponseChangedIndices.Add(Index);
		}

		// Continue only while decay, Damage, or Recovery can change logical state.
		const bool bShouldRemainActive = Cell.CurrentExposure > 0.0
			|| Cell.EcologicalDamageRatio > 0.0
			|| Cell.DamageRatePerSimulationHour > 0.0;
		ActiveFlags[Index] = bShouldRemainActive;
		if (bWasActive != bShouldRemainActive)
		{
			ActiveCellCount += bShouldRemainActive ? 1 : -1;
		}
	}

	// Commit revisions once per fixed step and stamp every changed Cell consistently.
	if (bAnyExposureChanged)
	{
		++ExposureRevision;
		for (const int32 Index : ExposureChangedIndices)
		{
			Cells[Index].ExposureRevision = ExposureRevision;
		}
	}
	if (bAnyResponseChanged)
	{
		++ResponseRevision;
		for (const int32 Index : ResponseChangedIndices)
		{
			Cells[Index].ResponseRevision = ResponseRevision;
		}
	}
	return true;
}

/* Read one public M3 snapshot by integer Cell coordinate. */
bool FAEExposureGrid::GetCellSnapshot(const FIntPoint& Coordinate, FAEM3CellSnapshot& OutSnapshot) const
{
	int32 Index = INDEX_NONE;
	if (!CellToIndex(Coordinate, Index))
	{
		return false;
	}
	OutSnapshot = MakeSnapshot(Coordinate, Index);
	return true;
}

/* Resolve one half-open world XY position and read its M3 snapshot. */
bool FAEExposureGrid::GetCellSnapshotAtWorldLocation(const FVector& WorldLocation, FAEM3CellSnapshot& OutSnapshot) const
{
	FIntPoint Coordinate;
	return WorldToCell(WorldLocation, Coordinate) && GetCellSnapshot(Coordinate, OutSnapshot);
}

/* Flatten one valid XY coordinate into row-major storage. */
bool FAEExposureGrid::CellToIndex(const FIntPoint& Coordinate, int32& OutIndex) const
{
	if (Coordinate.X < 0 || Coordinate.Y < 0 || Coordinate.X >= Config.Dimensions.X || Coordinate.Y >= Config.Dimensions.Y)
	{
		return false;
	}
	OutIndex = Coordinate.Y * Config.Dimensions.X + Coordinate.X;
	return Cells.IsValidIndex(OutIndex);
}

/* Map one world XY position to a valid Cell under half-open Grid bounds. */
bool FAEExposureGrid::WorldToCell(const FVector& WorldLocation, FIntPoint& OutCoordinate) const
{
	if (Cells.IsEmpty() || WorldLocation.ContainsNaN())
	{
		return false;
	}
	const FIntPoint Coordinate(
		FMath::FloorToInt((WorldLocation.X - WorldMin.X) / Config.CellSizeCm),
		FMath::FloorToInt((WorldLocation.Y - WorldMin.Y) / Config.CellSizeCm));
	int32 UnusedIndex = INDEX_NONE;
	if (!CellToIndex(Coordinate, UnusedIndex))
	{
		return false;
	}
	OutCoordinate = Coordinate;
	return true;
}

/* Calculate the world-space centre of one valid Cell. */
FVector FAEExposureGrid::GetCellWorldCenter(const FIntPoint& Coordinate) const
{
	return FVector(
		WorldMin.X + (static_cast<double>(Coordinate.X) + 0.5) * Config.CellSizeCm,
		WorldMin.Y + (static_cast<double>(Coordinate.Y) + 0.5) * Config.CellSizeCm,
		0.0);
}

/* Copy cumulative M1 facts into double-precision M3 consumption storage. */
FAERawBehaviourTotals FAEExposureGrid::MakeRawTotals(const FAEBehaviourCellSnapshot& Snapshot)
{
	FAERawBehaviourTotals Totals;
	Totals.PassCount = Snapshot.PassCount;
	Totals.TravelDistanceMeters = Snapshot.TravelDistanceMeters;
	Totals.DwellSeconds = Snapshot.DwellSeconds;
	Totals.SprintDistanceMeters = Snapshot.SprintDistanceMeters;
	Totals.CollectEventCount = Snapshot.CollectEventCount;
	Totals.CombatEventCount = Snapshot.CombatEventCount;
	return Totals;
}

/* Detect any reset or cumulative counter regression across the complete raw vector. */
bool FAEExposureGrid::HasRawRegression(const FAERawBehaviourTotals& Current, const FAERawBehaviourTotals& Previous)
{
	using namespace AEExposureGridPrivate;
	return Current.PassCount + RawRegressionTolerance < Previous.PassCount
		|| Current.TravelDistanceMeters + RawRegressionTolerance < Previous.TravelDistanceMeters
		|| Current.DwellSeconds + RawRegressionTolerance < Previous.DwellSeconds
		|| Current.SprintDistanceMeters + RawRegressionTolerance < Previous.SprintDistanceMeters
		|| Current.CollectEventCount + RawRegressionTolerance < Previous.CollectEventCount
		|| Current.CombatEventCount + RawRegressionTolerance < Previous.CombatEventCount;
}

/* Evaluate the zero-at-activation piecewise-linear first-version Damage curve. */
double FAEExposureGrid::EvaluateDamageRate(const double Exposure, const FAEM3ParameterSet& Parameters)
{
	if (Exposure <= Parameters.DamageActivationExposure)
	{
		return 0.0;
	}
	if (Exposure >= Parameters.DamageSaturationExposure)
	{
		return Parameters.DamageMaximumRatePerSimulationHour;
	}
	const double Alpha = (Exposure - Parameters.DamageActivationExposure)
		/ (Parameters.DamageSaturationExposure - Parameters.DamageActivationExposure);
	return Alpha * Parameters.DamageMaximumRatePerSimulationHour;
}

/* Copy internal state into one immutable Blueprint-readable snapshot. */
FAEM3CellSnapshot FAEExposureGrid::MakeSnapshot(const FIntPoint& Coordinate, const int32 Index) const
{
	const FCellState& Cell = Cells[Index];
	FAEM3CellSnapshot Snapshot;
	Snapshot.Coordinate = Coordinate;
	Snapshot.WorldCenter = GetCellWorldCenter(Coordinate);
	Snapshot.PassExposure = static_cast<float>(Cell.PassExposure);
	Snapshot.TravelExposure = static_cast<float>(Cell.TravelExposure);
	Snapshot.DwellExposure = static_cast<float>(Cell.DwellExposure);
	Snapshot.SprintExposure = static_cast<float>(Cell.SprintExposure);
	Snapshot.CollectExposure = static_cast<float>(Cell.CollectExposure);
	Snapshot.CombatExposure = static_cast<float>(Cell.CombatExposure);
	Snapshot.CurrentExposure = static_cast<float>(Cell.CurrentExposure);
	Snapshot.EcologicalDamageRatio = static_cast<float>(Cell.EcologicalDamageRatio);
	Snapshot.DamageRatePerSimulationHour = static_cast<float>(Cell.DamageRatePerSimulationHour);
	Snapshot.RecoveryRatePerSimulationHour = static_cast<float>(Cell.RecoveryRatePerSimulationHour);
	Snapshot.LowExposureDurationSimulationHours = static_cast<float>(Cell.LowExposureDurationSimulationHours);
	Snapshot.SourceBehaviourRevision = static_cast<int64>(Cell.SourceBehaviourRevision);
	Snapshot.ExposureRevision = static_cast<int64>(Cell.ExposureRevision);
	Snapshot.ResponseRevision = static_cast<int64>(Cell.ResponseRevision);
	return Snapshot;
}
