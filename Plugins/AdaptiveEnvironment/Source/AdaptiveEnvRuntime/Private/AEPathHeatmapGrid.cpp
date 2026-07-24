#include "AEPathHeatmapGrid.h"

/* Validate alignment and allocate contiguous M6 storage. */
bool FAEPathHeatmapGrid::Initialize(const FAEHeatmapGridConfig& InConfig)
{
	const int64 CellCount = static_cast<int64>(InConfig.Dimensions.X) * InConfig.Dimensions.Y;
	if (InConfig.Dimensions.X <= 0 || InConfig.Dimensions.Y <= 0
		|| !FMath::IsFinite(InConfig.CellSizeCm) || InConfig.CellSizeCm <= 0.0f
		|| CellCount <= 0 || CellCount > MAX_int32)
	{
		return false;
	}

	Config = InConfig;
	WorldMin = Config.WorldCenter - FVector2D(
		Config.Dimensions.X * Config.CellSizeCm * 0.5,
		Config.Dimensions.Y * Config.CellSizeCm * 0.5);
	Cells.SetNum(static_cast<int32>(CellCount));
	ActiveTransitionFlags.Init(false, Cells.Num());
	LastEncodedValues.Init(FColor(128, 128, 0, 0), Cells.Num());
	Reset();
	return true;
}

/* Restore every M6 Cell and diagnostic counter. */
void FAEPathHeatmapGrid::Reset()
{
	for (FCell& Cell : Cells)
	{
		Cell = FCell();
	}
	ActiveTransitionFlags.Init(false, Cells.Num());
	LastEncodedValues.Init(FColor(128, 128, 0, 0), Cells.Num());
	LastChangedCellIndices.Reset();
	VisualCommands.Reset();
	PathVisualRevision = 0;
	RejectedInputCount = 0;
}

/* Advance validated M6 inputs and commit one revision per changed fixed step. */
bool FAEPathHeatmapGrid::Update(
	const TArray<FAEM6InputSnapshot>& Inputs,
	const double DeltaSimulationHours,
	const FAEM6ParameterSet& Parameters)
{
	check(IsInGameThread());
	if (Cells.IsEmpty() || !FMath::IsFinite(DeltaSimulationHours)
		|| DeltaSimulationHours < 0.0 || !ValidateParameters(Parameters))
	{
		return false;
	}

	LastChangedCellIndices.Reset();
	VisualCommands.Reset();
	TArray<FAEM6InputSnapshot> Ordered = Inputs;
	Ordered.Sort([this](const FAEM6InputSnapshot& A, const FAEM6InputSnapshot& B)
	{
		int32 AIndex = INDEX_NONE;
		int32 BIndex = INDEX_NONE;
		CellToIndex(A.Coordinate, AIndex);
		CellToIndex(B.Coordinate, BIndex);
		return AIndex < BIndex;
	});

	// Validate version and value contracts before mutating each Cell.
	for (const FAEM6InputSnapshot& Input : Ordered)
	{
		int32 Index = INDEX_NONE;
		if (!CellToIndex(Input.Coordinate, Index)
			|| !FMath::IsFinite(Input.FlowDirection.X)
			|| !FMath::IsFinite(Input.FlowDirection.Y)
			|| !FMath::IsFinite(Input.FlowMagnitude)
			|| Input.FlowMagnitude < 0.0 || Input.FlowMagnitude > 1.0
			|| !FMath::IsFinite(Input.DamageRatio)
			|| Input.DamageRatio < 0.0 || Input.DamageRatio > 1.0)
		{
			++RejectedInputCount;
			continue;
		}

		FCell& Cell = Cells[Index];
		if (Cell.bInitialized
			&& (Input.SourceBehaviourRevision < Cell.State.SourceBehaviourRevision
				|| Input.SourceResponseRevision < Cell.State.SourceResponseRevision
				|| Input.CurrentSimulationStep <= Cell.State.SimulationStep))
		{
			++RejectedInputCount;
			continue;
		}

		// Derive the new target and advance the independent visual transition.
		const FAEPathHeatmapCellState Previous = Cell.State;
		FAEPathHeatmapCellState Next = Previous;
		Next.FlowVector = (Input.FlowDirection.GetSafeNormal() * Input.FlowMagnitude).ClampAxes(-1.0, 1.0);
		Next.TargetPathIntensity = CalculateTargetIntensity(Input.DamageRatio, Parameters);
		const double Rate = Next.TargetPathIntensity >= Previous.PathIntensity
			? Parameters.FormationRatePerSimulationHour
			: Parameters.FadeRatePerSimulationHour;
		Next.PathIntensity = FMath::Clamp(
			FMath::FInterpConstantTo(
				Previous.PathIntensity,
				Next.TargetPathIntensity,
				DeltaSimulationHours,
				Rate),
			0.0,
			1.0);
		Next.SourceResponseRevision = Input.SourceResponseRevision;
		Next.SourceBehaviourRevision = Input.SourceBehaviourRevision;
		Next.SimulationStep = Input.CurrentSimulationStep;

		const bool bChanged = !Cell.bInitialized
			|| !Previous.FlowVector.Equals(Next.FlowVector, 1.0 / 255.0)
			|| !FMath::IsNearlyEqual(Previous.PathIntensity, Next.PathIntensity, Parameters.DirtyIntensityEpsilon)
			|| !FMath::IsNearlyEqual(Previous.TargetPathIntensity, Next.TargetPathIntensity, Parameters.DirtyIntensityEpsilon)
			|| Previous.SourceBehaviourRevision != Next.SourceBehaviourRevision
			|| Previous.SourceResponseRevision != Next.SourceResponseRevision;
		Cell.State = Next;
		Cell.bInitialized = true;
		ActiveTransitionFlags[Index] = !FMath::IsNearlyEqual(
			Next.PathIntensity,
			Next.TargetPathIntensity,
			Parameters.DirtyIntensityEpsilon);
		if (bChanged)
		{
			LastChangedCellIndices.Add(Index);
		}
	}

	// Commit one stable revision and emit only texture-observable changes.
	if (!LastChangedCellIndices.IsEmpty())
	{
		++PathVisualRevision;
		for (const int32 Index : LastChangedCellIndices)
		{
			FCell& Cell = Cells[Index];
			Cell.State.PathVisualRevision = PathVisualRevision;
			const FColor EncodedValue = EncodeVisualValue(
				Cell.State.FlowVector,
				Cell.State.PathIntensity);
			if (LastEncodedValues[Index] != EncodedValue)
			{
				LastEncodedValues[Index] = EncodedValue;
				VisualCommands.Add({
					Index,
					FIntPoint(Index % Config.Dimensions.X, Index / Config.Dimensions.X),
					EncodedValue,
					PathVisualRevision});
			}
		}
	}
	return true;
}

/* Merge M5 changes with Cells whose M6 transition must continue. */
void FAEPathHeatmapGrid::BuildCandidateIndices(
	const TArray<int32>& M1ChangedIndices,
	const TArray<int32>& M5ChangedIndices,
	TArray<int32>& OutIndices) const
{
	TBitArray<> Flags = ActiveTransitionFlags;
	for (const int32 Index : M1ChangedIndices)
	{
		if (Flags.IsValidIndex(Index))
		{
			Flags[Index] = true;
		}
	}
	for (const int32 Index : M5ChangedIndices)
	{
		if (Flags.IsValidIndex(Index))
		{
			Flags[Index] = true;
		}
	}
	OutIndices.Reset();
	for (TConstSetBitIterator<> Iterator(Flags); Iterator; ++Iterator)
	{
		OutIndices.Add(Iterator.GetIndex());
	}
}

/* Read one initialized M6 Cell. */
bool FAEPathHeatmapGrid::GetCellSnapshot(
	const FIntPoint& Coordinate,
	FAEPathHeatmapSnapshot& OutSnapshot) const
{
	int32 Index = INDEX_NONE;
	if (!CellToIndex(Coordinate, Index) || !Cells[Index].bInitialized)
	{
		return false;
	}
	const FAEPathHeatmapCellState& State = Cells[Index].State;
	OutSnapshot.Coordinate = Coordinate;
	OutSnapshot.WorldCenter = GetCellWorldCenter(Coordinate);
	OutSnapshot.FlowVector = State.FlowVector;
	OutSnapshot.PathIntensity = static_cast<float>(State.PathIntensity);
	OutSnapshot.TargetPathIntensity = static_cast<float>(State.TargetPathIntensity);
	OutSnapshot.SourceBehaviourRevision = static_cast<int64>(State.SourceBehaviourRevision);
	OutSnapshot.SourceResponseRevision = static_cast<int64>(State.SourceResponseRevision);
	OutSnapshot.PathVisualRevision = static_cast<int64>(State.PathVisualRevision);
	OutSnapshot.SimulationStep = static_cast<int64>(State.SimulationStep);
	return true;
}

/* Resolve a world position and read its initialized M6 Cell. */
bool FAEPathHeatmapGrid::GetCellSnapshotAtWorldLocation(
	const FVector& Location,
	FAEPathHeatmapSnapshot& OutSnapshot) const
{
	FIntPoint Coordinate;
	return WorldToCell(Location, Coordinate)
		&& GetCellSnapshot(Coordinate, OutSnapshot);
}

/* Build a deterministic full texture reconstruction. */
void FAEPathHeatmapGrid::BuildFullVisualCommands(
	TArray<FAEPathHeatmapVisualCommand>& OutCommands) const
{
	OutCommands.Reset();
	OutCommands.Reserve(Cells.Num());
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (!Cells[Index].bInitialized)
		{
			continue;
		}
		OutCommands.Add({
			Index,
			FIntPoint(Index % Config.Dimensions.X, Index / Config.Dimensions.X),
			LastEncodedValues[Index],
			PathVisualRevision});
	}
}

/* Map one coordinate to row-major storage. */
bool FAEPathHeatmapGrid::CellToIndex(
	const FIntPoint& Coordinate,
	int32& OutIndex) const
{
	if (Coordinate.X < 0 || Coordinate.Y < 0
		|| Coordinate.X >= Config.Dimensions.X
		|| Coordinate.Y >= Config.Dimensions.Y)
	{
		return false;
	}
	OutIndex = Coordinate.Y * Config.Dimensions.X + Coordinate.X;
	return Cells.IsValidIndex(OutIndex);
}

/* Map one half-open world XY position to a coordinate. */
bool FAEPathHeatmapGrid::WorldToCell(
	const FVector& Location,
	FIntPoint& OutCoordinate) const
{
	if (Cells.IsEmpty() || Location.ContainsNaN())
	{
		return false;
	}
	const FIntPoint Candidate(
		FMath::FloorToInt((Location.X - WorldMin.X) / Config.CellSizeCm),
		FMath::FloorToInt((Location.Y - WorldMin.Y) / Config.CellSizeCm));
	int32 Index = INDEX_NONE;
	if (!CellToIndex(Candidate, Index))
	{
		return false;
	}
	OutCoordinate = Candidate;
	return true;
}

/* Calculate one Cell centre in world centimetres. */
FVector FAEPathHeatmapGrid::GetCellWorldCenter(
	const FIntPoint& Coordinate) const
{
	return FVector(
		WorldMin.X + (Coordinate.X + 0.5) * Config.CellSizeCm,
		WorldMin.Y + (Coordinate.Y + 0.5) * Config.CellSizeCm,
		0.0);
}

/* Validate the complete M6 parameter contract. */
bool FAEPathHeatmapGrid::ValidateParameters(
	const FAEM6ParameterSet& Parameters)
{
	// Reject non-finite values and contracts that cannot produce a stable normalized intensity.
	return FMath::IsFinite(Parameters.VisibleDamageThresholdRatio)
		// Validate every remaining floating-point input before comparing its range.
		&& FMath::IsFinite(Parameters.FullPathDamageThresholdRatio)
		// Formation must remain finite for deterministic fixed-step integration.
		&& FMath::IsFinite(Parameters.FormationRatePerSimulationHour)
		// Fade must remain finite for deterministic fixed-step integration.
		&& FMath::IsFinite(Parameters.FadeRatePerSimulationHour)
		// Dirty epsilon must remain finite before command emission uses it.
		&& FMath::IsFinite(Parameters.DirtyIntensityEpsilon)
		&& Parameters.VisibleDamageThresholdRatio >= 0.0
		&& Parameters.VisibleDamageThresholdRatio < Parameters.FullPathDamageThresholdRatio
		&& Parameters.FullPathDamageThresholdRatio <= 1.0
		&& Parameters.FormationRatePerSimulationHour >= 0.0
		&& Parameters.FadeRatePerSimulationHour >= 0.0
		&& Parameters.DirtyIntensityEpsilon > 0.0
		&& Parameters.DirtyIntensityEpsilon <= 1.0;
}

/* Convert one bounded M5 Damage value into the M6 target range. */
double FAEPathHeatmapGrid::CalculateTargetIntensity(
	const double DamageRatio,
	const FAEM6ParameterSet& Parameters)
{
	const double Range = Parameters.FullPathDamageThresholdRatio
		- Parameters.VisibleDamageThresholdRatio;
	// Normalize the configured damage interval into the material-facing zero-to-one range.
	return FMath::Clamp(
		(DamageRatio - Parameters.VisibleDamageThresholdRatio) / Range,
		0.0,
		1.0);
}

/* Encode one complete material-facing Cell value into RGBA8. */
FColor FAEPathHeatmapGrid::EncodeVisualValue(
	const FVector2D& FlowVector,
	const double PathIntensity)
{
	// Map signed Flow components to unsigned texture channels and retain path intensity in alpha.
	const FVector2D BoundedFlow = FlowVector.ClampAxes(-1.0, 1.0);
	return FColor(
		static_cast<uint8>(FMath::Clamp(FMath::RoundToInt((BoundedFlow.X * 0.5 + 0.5) * 255.0), 0, 255)),
		static_cast<uint8>(FMath::Clamp(FMath::RoundToInt((BoundedFlow.Y * 0.5 + 0.5) * 255.0), 0, 255)),
		0,
		static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(PathIntensity * 255.0), 0, 255)));
}
