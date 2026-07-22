#include "AEEcologicalResponseGrid.h"

#include "AEM5ParameterService.h"

namespace AEEcologicalResponseGridPrivate
{
	constexpr float ChangeTolerance = 1.0e-6f;
}

/* Validate alignment and allocate contiguous M5 storage. */
bool FAEEcologicalResponseGrid::Initialize(const FAEHeatmapGridConfig& InConfig)
{
	const int64 CellCount = static_cast<int64>(InConfig.Dimensions.X) * InConfig.Dimensions.Y;
	if (InConfig.Dimensions.X <= 0 || InConfig.Dimensions.Y <= 0 || !FMath::IsFinite(InConfig.CellSizeCm)
		|| InConfig.CellSizeCm <= 0.0f || CellCount <= 0 || CellCount > MAX_int32) return false;
	Config = InConfig;
	WorldMin = Config.WorldCenter - FVector2D(Config.Dimensions.X * Config.CellSizeCm * 0.5, Config.Dimensions.Y * Config.CellSizeCm * 0.5);
	Cells.SetNum(static_cast<int32>(CellCount));
	ActiveFlags.Init(false, Cells.Num());
	Reset();
	return true;
}

/* Restore every M5 Cell and diagnostic counter. */
void FAEEcologicalResponseGrid::Reset()
{
	for (FCellState& Cell : Cells) Cell = FCellState();
	ActiveFlags.Init(false, Cells.Num());
	LastChangedCellIndices.Reset();
	ResponseRevision = 0;
	RejectedInputCount = 0;
}

/* Evaluate compatible immutable inputs and commit one revision per changed step. */
bool FAEEcologicalResponseGrid::Update(const TArray<FAEM5InputSnapshot>& Inputs, const double DeltaSimulationHours, const FAEM5ParameterSet& Parameters, const FAEParameterBundleIdentity& ActiveBundleIdentity)
{
	check(IsInGameThread());
	if (Cells.IsEmpty() || !FMath::IsFinite(DeltaSimulationHours) || DeltaSimulationHours < 0.0) return false;
	LastChangedCellIndices.Reset();
	TArray<FAEM5InputSnapshot> Ordered = Inputs;
	Ordered.Sort([this](const FAEM5InputSnapshot& A, const FAEM5InputSnapshot& B)
	{
		int32 AI = INDEX_NONE, BI = INDEX_NONE; CellToIndex(A.Coordinate, AI); CellToIndex(B.Coordinate, BI); return AI < BI;
	});

	// Reject mixed bundle identities and regressions before mutating a Cell.
	for (const FAEM5InputSnapshot& Input : Ordered)
	{
		int32 Index = INDEX_NONE;
		if (!CellToIndex(Input.Coordinate, Index)
			|| Input.BundleIdentity.BundleId != ActiveBundleIdentity.BundleId
			|| Input.BundleIdentity.ContentHash != ActiveBundleIdentity.ContentHash
			|| Input.ExposureMaximum <= 0.0)
		{
			++RejectedInputCount;
			continue;
		}
		FCellState& Cell = Cells[Index];
		if (Cell.bInitialized && (Input.ExposureRevision < static_cast<uint64>(Cell.Snapshot.SourceExposureRevision)
			|| Input.ConstraintRevision < static_cast<uint64>(Cell.Snapshot.SourceConstraintRevision)
			|| Input.SimulationStep < static_cast<uint64>(Cell.Snapshot.SimulationStep)))
		{
			++RejectedInputCount;
			continue;
		}
		const FAEEcologicalResponseSnapshot Previous = Cell.Snapshot;
		FAEEcologicalResponseSnapshot Next = FAEM5ParameterService::EvaluateResponse(Input.Exposure, Input.ExposureMaximum,
			Input.ConstraintPressureRatio, Input.HabitatSuitabilityRatio, DeltaSimulationHours, Parameters, Cell.StateMemory);
		Next.Coordinate = Input.Coordinate;
		Next.WorldCenter = GetCellWorldCenter(Input.Coordinate);
		Next.SourceExposureRevision = static_cast<int64>(Input.ExposureRevision);
		Next.SourceConstraintRevision = static_cast<int64>(Input.ConstraintRevision);
		Next.SimulationStep = static_cast<int64>(Input.SimulationStep);
		const bool bChanged = !Cell.bInitialized
			|| !FMath::IsNearlyEqual(Previous.DamageRatio, Next.DamageRatio, AEEcologicalResponseGridPrivate::ChangeTolerance)
			|| !FMath::IsNearlyEqual(Previous.EffectiveImpactRatio, Next.EffectiveImpactRatio, AEEcologicalResponseGridPrivate::ChangeTolerance)
			|| Next.RecoveryRatePerSimulationHour > 0.0f;
		Cell.Snapshot = Next;
		Cell.bInitialized = true;
		ActiveFlags[Index] = Cell.StateMemory.DamageRatio > UE_DOUBLE_SMALL_NUMBER
			|| Cell.StateMemory.LowExposureDurationSimulationHours > 0.0
			|| Next.EffectiveImpactRatio > 0.0f;
		if (bChanged) LastChangedCellIndices.Add(Index);
	}
	if (!LastChangedCellIndices.IsEmpty())
	{
		++ResponseRevision;
		for (const int32 Index : LastChangedCellIndices) Cells[Index].Snapshot.ResponseRevision = static_cast<int64>(ResponseRevision);
	}
	return true;
}

/* Merge upstream changes with Cells whose temporal response must continue. */
void FAEEcologicalResponseGrid::BuildCandidateIndices(const TArray<int32>& M3ChangedIndices, const TArray<int32>& M4ChangedIndices, TArray<int32>& OutIndices) const
{
	TBitArray<> Flags = ActiveFlags;
	for (const int32 Index : M3ChangedIndices) if (Flags.IsValidIndex(Index)) Flags[Index] = true;
	for (const int32 Index : M4ChangedIndices) if (Flags.IsValidIndex(Index)) Flags[Index] = true;
	OutIndices.Reset();
	for (TConstSetBitIterator<> It(Flags); It; ++It) OutIndices.Add(It.GetIndex());
}

/* Read one initialized M5 Cell. */
bool FAEEcologicalResponseGrid::GetCellSnapshot(const FIntPoint& Coordinate, FAEEcologicalResponseSnapshot& OutSnapshot) const
{
	int32 Index = INDEX_NONE;
	if (!CellToIndex(Coordinate, Index) || !Cells[Index].bInitialized) return false;
	OutSnapshot = Cells[Index].Snapshot;
	return true;
}

/* Resolve a world position and read its initialized M5 Cell. */
bool FAEEcologicalResponseGrid::GetCellSnapshotAtWorldLocation(const FVector& Location, FAEEcologicalResponseSnapshot& OutSnapshot) const
{
	FIntPoint Coordinate;
	return WorldToCell(Location, Coordinate) && GetCellSnapshot(Coordinate, OutSnapshot);
}

/* Map one coordinate to row-major storage. */
bool FAEEcologicalResponseGrid::CellToIndex(const FIntPoint& Coordinate, int32& OutIndex) const
{
	if (Coordinate.X < 0 || Coordinate.Y < 0 || Coordinate.X >= Config.Dimensions.X || Coordinate.Y >= Config.Dimensions.Y) return false;
	OutIndex = Coordinate.Y * Config.Dimensions.X + Coordinate.X;
	return Cells.IsValidIndex(OutIndex);
}

/* Map one half-open world XY position to a coordinate. */
bool FAEEcologicalResponseGrid::WorldToCell(const FVector& Location, FIntPoint& OutCoordinate) const
{
	if (Cells.IsEmpty() || Location.ContainsNaN()) return false;
	const FIntPoint Candidate(FMath::FloorToInt((Location.X - WorldMin.X) / Config.CellSizeCm), FMath::FloorToInt((Location.Y - WorldMin.Y) / Config.CellSizeCm));
	int32 Index = INDEX_NONE;
	if (!CellToIndex(Candidate, Index)) return false;
	OutCoordinate = Candidate;
	return true;
}

/* Calculate one Cell centre in world centimetres. */
FVector FAEEcologicalResponseGrid::GetCellWorldCenter(const FIntPoint& Coordinate) const
{
	return FVector(WorldMin.X + (Coordinate.X + 0.5) * Config.CellSizeCm, WorldMin.Y + (Coordinate.Y + 0.5) * Config.CellSizeCm, 0.0);
}
