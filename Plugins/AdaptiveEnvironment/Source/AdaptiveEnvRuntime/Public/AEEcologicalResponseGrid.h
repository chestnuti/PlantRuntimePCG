#pragma once

#include "CoreMinimal.h"
#include "AEM5Types.h"
#include "AdaptiveEnvTypes.h"

/* Owns deterministic per-Cell M5 Damage and Recovery state. */
class ADAPTIVEENVRUNTIME_API FAEEcologicalResponseGrid
{
public:
	/* Allocates M5 storage aligned with the shared runtime Grid. */
	bool Initialize(const FAEHeatmapGridConfig& InConfig);
	/* Clears response state, active tracking, revisions, and counters. */
	void Reset();
	/* Advances version-compatible inputs in row-major order. */
	bool Update(const TArray<FAEM5InputSnapshot>& Inputs, double DeltaSimulationHours, const FAEM5ParameterSet& Parameters, const FAEParameterBundleIdentity& ActiveBundleIdentity);
	/* Builds the stable union of upstream changes and continuing M5 state. */
	void BuildCandidateIndices(const TArray<int32>& M3ChangedIndices, const TArray<int32>& M4ChangedIndices, TArray<int32>& OutIndices) const;
	/* Reads one initialized response by Cell coordinate. */
	bool GetCellSnapshot(const FIntPoint& Coordinate, FAEEcologicalResponseSnapshot& OutSnapshot) const;
	/* Reads one initialized response by half-open world XY position. */
	bool GetCellSnapshotAtWorldLocation(const FVector& Location, FAEEcologicalResponseSnapshot& OutSnapshot) const;
	/* Returns indices changed during the latest successful update. */
	const TArray<int32>& GetLastChangedCellIndices() const { return LastChangedCellIndices; }
	/* Returns the latest global response revision. */
	uint64 GetResponseRevision() const { return ResponseRevision; }
	/* Returns rejected incompatible input count. */
	uint64 GetRejectedInputCount() const { return RejectedInputCount; }

private:
	/* Stores mutable M5 memory and its latest committed public snapshot. */
	struct FCellState
	{
		FAEM5StateMemory StateMemory;
		FAEEcologicalResponseSnapshot Snapshot;
		bool bInitialized = false;
	};
	/* Maps a valid coordinate to row-major storage. */
	bool CellToIndex(const FIntPoint& Coordinate, int32& OutIndex) const;
	/* Maps a half-open world XY location to one Cell. */
	bool WorldToCell(const FVector& Location, FIntPoint& OutCoordinate) const;
	/* Calculates one configured Cell centre. */
	FVector GetCellWorldCenter(const FIntPoint& Coordinate) const;

	FAEHeatmapGridConfig Config;
	FVector2D WorldMin = FVector2D::ZeroVector;
	TArray<FCellState> Cells;
	TBitArray<> ActiveFlags;
	TArray<int32> LastChangedCellIndices;
	uint64 ResponseRevision = 0;
	uint64 RejectedInputCount = 0;
};
