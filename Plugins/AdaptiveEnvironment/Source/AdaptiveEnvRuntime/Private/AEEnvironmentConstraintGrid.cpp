#include "AEEnvironmentConstraintGrid.h"

#include "AEM4ParameterService.h"

namespace AEEnvironmentConstraintGridPrivate
{
	constexpr float ChangeTolerance = 1.0e-6f;
}

/* Validate alignment and allocate contiguous M4 Cell storage. */
bool FAEEnvironmentConstraintGrid::Initialize(const FAEHeatmapGridConfig& InConfig)
{
	const int64 CellCount = static_cast<int64>(InConfig.Dimensions.X) * InConfig.Dimensions.Y;
	if (InConfig.Dimensions.X <= 0 || InConfig.Dimensions.Y <= 0 || !FMath::IsFinite(InConfig.CellSizeCm)
		|| InConfig.CellSizeCm <= 0.0f || CellCount <= 0 || CellCount > MAX_int32)
	{
		return false;
	}
	Config = InConfig;
	WorldMin = Config.WorldCenter - FVector2D(Config.Dimensions.X * Config.CellSizeCm * 0.5, Config.Dimensions.Y * Config.CellSizeCm * 0.5);
	Cells.SetNum(static_cast<int32>(CellCount));
	ActiveFlags.Init(false, Cells.Num());
	Reset();
	return true;
}

/* Restore every M4 Cell to an uninitialized state. */
void FAEEnvironmentConstraintGrid::Reset()
{
	for (FCellState& Cell : Cells) Cell = FCellState();
	ActiveFlags.Init(false, Cells.Num());
	LastChangedCellIndices.Reset();
	ConstraintRevision = 0;
}

/* Evaluate and atomically stamp one stable observation batch. */
bool FAEEnvironmentConstraintGrid::Update(const TArray<FAEWorldConstraintObservation>& Observations, const double DeltaSimulationHours, const uint64 SimulationStep, const FAEM4ParameterSet& Parameters)
{
	check(IsInGameThread());
	if (Cells.IsEmpty() || !FMath::IsFinite(DeltaSimulationHours) || DeltaSimulationHours < 0.0) return false;
	LastChangedCellIndices.Reset();
	TArray<FAEWorldConstraintObservation> Ordered = Observations;
	Ordered.Sort([this](const FAEWorldConstraintObservation& A, const FAEWorldConstraintObservation& B)
	{
		int32 AI = INDEX_NONE, BI = INDEX_NONE;
		CellToIndex(A.Coordinate, AI); CellToIndex(B.Coordinate, BI);
		return AI < BI;
	});

	// Fail closed per Cell by ignoring invalid observations and preserving committed state.
	for (const FAEWorldConstraintObservation& Observation : Ordered)
	{
		int32 Index = INDEX_NONE;
		if (!Observation.bValid || !CellToIndex(Observation.Coordinate, Index)) continue;
		FCellState& Cell = Cells[Index];
		const FAEEnvironmentConstraintSnapshot Previous = Cell.Snapshot;
		FAEEnvironmentConstraintSnapshot Next = FAEM4ParameterService::EvaluateEnvironment(
			Observation.SlopeDegrees, Observation.MoistureRatio, DeltaSimulationHours, Parameters, Cell.StateMemory);
		Next.Coordinate = Observation.Coordinate;
		Next.WorldCenter = Observation.WorldCenter;
		Next.SlopeDegrees = static_cast<float>(Observation.SlopeDegrees);
		Next.MoistureRatio = static_cast<float>(Observation.MoistureRatio);
		Next.SimulationStep = static_cast<int64>(SimulationStep);
		const bool bChanged = !Cell.bInitialized
			|| !FMath::IsNearlyEqual(Previous.ConstraintPressureRatio, Next.ConstraintPressureRatio, AEEnvironmentConstraintGridPrivate::ChangeTolerance)
			|| Previous.State != Next.State;
		Cell.Snapshot = Next;
		Cell.bInitialized = true;
		ActiveFlags[Index] = Cell.StateMemory.CandidateState != Cell.StateMemory.CurrentState;
		if (bChanged) LastChangedCellIndices.Add(Index);
	}
	if (!LastChangedCellIndices.IsEmpty())
	{
		++ConstraintRevision;
		for (const int32 Index : LastChangedCellIndices) Cells[Index].Snapshot.ConstraintRevision = static_cast<int64>(ConstraintRevision);
	}
	return true;
}

/* Merge newly relevant and temporal Cells into one row-major candidate set. */
void FAEEnvironmentConstraintGrid::BuildCandidateIndices(const TArray<int32>& NewlyRelevantIndices, TArray<int32>& OutIndices) const
{
	TBitArray<> Flags = ActiveFlags;
	// Resample every initialized Cell so moving terrain or moisture sources invalidate cached observations.
	for (int32 Index = 0; Index < Cells.Num(); ++Index) if (Cells[Index].bInitialized) Flags[Index] = true;
	for (const int32 Index : NewlyRelevantIndices) if (Flags.IsValidIndex(Index)) Flags[Index] = true;
	OutIndices.Reset();
	for (TConstSetBitIterator<> It(Flags); It; ++It) OutIndices.Add(It.GetIndex());
}

/* Read one initialized M4 Cell snapshot. */
bool FAEEnvironmentConstraintGrid::GetCellSnapshot(const FIntPoint& Coordinate, FAEEnvironmentConstraintSnapshot& OutSnapshot) const
{
	int32 Index = INDEX_NONE;
	if (!CellToIndex(Coordinate, Index) || !Cells[Index].bInitialized) return false;
	OutSnapshot = Cells[Index].Snapshot;
	return true;
}

/* Resolve a world position and read its initialized M4 Cell. */
bool FAEEnvironmentConstraintGrid::GetCellSnapshotAtWorldLocation(const FVector& Location, FAEEnvironmentConstraintSnapshot& OutSnapshot) const
{
	FIntPoint Coordinate;
	return WorldToCell(Location, Coordinate) && GetCellSnapshot(Coordinate, OutSnapshot);
}

/* Map a coordinate to row-major storage. */
bool FAEEnvironmentConstraintGrid::CellToIndex(const FIntPoint& Coordinate, int32& OutIndex) const
{
	if (Coordinate.X < 0 || Coordinate.Y < 0 || Coordinate.X >= Config.Dimensions.X || Coordinate.Y >= Config.Dimensions.Y) return false;
	OutIndex = Coordinate.Y * Config.Dimensions.X + Coordinate.X;
	return Cells.IsValidIndex(OutIndex);
}

/* Map one half-open world XY location to a Cell. */
bool FAEEnvironmentConstraintGrid::WorldToCell(const FVector& Location, FIntPoint& OutCoordinate) const
{
	if (Cells.IsEmpty() || Location.ContainsNaN()) return false;
	const FIntPoint Candidate(FMath::FloorToInt((Location.X - WorldMin.X) / Config.CellSizeCm), FMath::FloorToInt((Location.Y - WorldMin.Y) / Config.CellSizeCm));
	int32 Index = INDEX_NONE;
	if (!CellToIndex(Candidate, Index)) return false;
	OutCoordinate = Candidate;
	return true;
}

/* Calculate one Cell centre at the configured trace origin height. */
FVector FAEEnvironmentConstraintGrid::GetCellWorldCenter(const FIntPoint& Coordinate) const
{
	return FVector(WorldMin.X + (Coordinate.X + 0.5) * Config.CellSizeCm, WorldMin.Y + (Coordinate.Y + 0.5) * Config.CellSizeCm, 0.0);
}
