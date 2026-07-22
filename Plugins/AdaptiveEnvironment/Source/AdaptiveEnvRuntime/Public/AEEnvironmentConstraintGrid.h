#pragma once

#include "CoreMinimal.h"
#include "AEWorldConstraintProvider.h"
#include "AEM4Types.h"
#include "AdaptiveEnvTypes.h"

/* Owns committed per-Cell M4 environment observations and temporal state. */
class ADAPTIVEENVRUNTIME_API FAEEnvironmentConstraintGrid
{
public:
	/* Allocates M4 storage aligned with the M1 Grid configuration. */
	bool Initialize(const FAEHeatmapGridConfig& InConfig);
	/* Clears snapshots, state memories, active flags, and revisions. */
	void Reset();
	/* Applies valid observations in row-major order for one fixed simulation step. */
	bool Update(const TArray<FAEWorldConstraintObservation>& Observations, double DeltaSimulationHours, uint64 SimulationStep, const FAEM4ParameterSet& Parameters);
	/* Builds the stable set requiring first sampling or continued state timing. */
	void BuildCandidateIndices(const TArray<int32>& NewlyRelevantIndices, TArray<int32>& OutIndices) const;
	/* Reads one committed snapshot by Cell coordinate. */
	bool GetCellSnapshot(const FIntPoint& Coordinate, FAEEnvironmentConstraintSnapshot& OutSnapshot) const;
	/* Reads one committed snapshot by half-open world XY position. */
	bool GetCellSnapshotAtWorldLocation(const FVector& Location, FAEEnvironmentConstraintSnapshot& OutSnapshot) const;
	/* Returns indices changed by the latest successful update. */
	const TArray<int32>& GetLastChangedCellIndices() const { return LastChangedCellIndices; }
	/* Returns the latest global M4 revision. */
	uint64 GetConstraintRevision() const { return ConstraintRevision; }
	/* Returns the configured world centre for one Cell. */
	FVector GetCellWorldCenter(const FIntPoint& Coordinate) const;

private:
	/* Stores mutable state and the last committed public result for one Cell. */
	struct FCellState
	{
		FAEM4StateMemory StateMemory;
		FAEEnvironmentConstraintSnapshot Snapshot;
		bool bInitialized = false;
	};
	/* Maps a valid coordinate to a row-major index. */
	bool CellToIndex(const FIntPoint& Coordinate, int32& OutIndex) const;
	/* Maps a half-open world position to a Cell coordinate. */
	bool WorldToCell(const FVector& Location, FIntPoint& OutCoordinate) const;

	FAEHeatmapGridConfig Config;
	FVector2D WorldMin = FVector2D::ZeroVector;
	TArray<FCellState> Cells;
	TBitArray<> ActiveFlags;
	TArray<int32> LastChangedCellIndices;
	uint64 ConstraintRevision = 0;
};
