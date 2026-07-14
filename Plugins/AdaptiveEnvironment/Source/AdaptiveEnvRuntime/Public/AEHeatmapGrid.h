#pragma once

#include "CoreMinimal.h"
#include "AdaptiveEnvTypes.h"

/** Stores mutable raw behaviour accumulators for one grid cell. */
struct FAEBehaviourCellData
{
	/** Counts agent entries into the cell. */
	float PassCount = 0.0f;
	/** Accumulates movement distance in metres. */
	float TravelDistanceMeters = 0.0f;
	/** Accumulates dwelling time in seconds. */
	float DwellSeconds = 0.0f;
	/** Accumulates sprint movement distance in metres. */
	float SprintDistanceMeters = 0.0f;
	/** Accumulates weighted collection events. */
	float CollectEventCount = 0.0f;
	/** Accumulates weighted combat events. */
	float CombatEventCount = 0.0f;
	/** Accumulates distance-weighted horizontal directions. */
	FVector2f FlowSum = FVector2f::ZeroVector;
	/** Accumulates distance used to normalize flow. */
	float FlowWeight = 0.0f;
	/** Stores Gaussian-smoothed raw activity. */
	float SmoothedActivity = 0.0f;
	/** Stores the latest contributing simulation timestamp in seconds. */
	double LastUpdatedTime = 0.0;
};

/** Maps world positions to cells and aggregates deterministic behaviour samples. */
class ADAPTIVEENVRUNTIME_API FAEHeatmapGrid
{
public:
	/** Validates configuration and allocates a zeroed grid. */
	bool Initialize(const FAEHeatmapGridConfig& InConfig);
	/** Clears all cell, agent, revision, and statistics state. */
	void Reset();

	/** Returns whether the grid contains allocated cells. */
	bool IsValid() const { return Cells.Num() > 0; }
	/** Returns the validated grid configuration. */
	const FAEHeatmapGridConfig& GetConfig() const { return Config; }
	/** Returns half-open world XY bounds in centimetres. */
	FBox2D GetWorldBounds() const;

	/** Maps a world position in centimetres to a cell coordinate. */
	bool WorldToCell(const FVector& WorldLocation, FIntPoint& OutCoordinate) const;
	/** Maps a valid XY cell coordinate to a flat array index. */
	bool CellToIndex(const FIntPoint& Coordinate, int32& OutIndex) const;
	/** Returns the world-space centre of a cell in centimetres. */
	FVector GetCellWorldCenter(const FIntPoint& Coordinate) const;

	/** Validates and aggregates one movement or event sample. */
	EAEBehaviourSubmitResult AccumulateSample(const FAEBehaviourSample& Sample);
	/** Adds one queue-level rejection to grid statistics. */
	void RecordRejectedSample(EAEBehaviourSubmitResult Result);
	/** Copies one cell into an immutable public snapshot. */
	bool GetCellSnapshot(const FIntPoint& Coordinate, FAEBehaviourCellSnapshot& OutSnapshot) const;
	/** Reads the cell containing a world position in centimetres. */
	bool GetCellSnapshotAtWorldLocation(const FVector& WorldLocation, FAEBehaviourCellSnapshot& OutSnapshot) const;
	/** Collects non-empty cells within a world-space radius in centimetres. */
	void GetNonEmptyCellsInRadius(const FVector& WorldLocation, float RadiusCm, int32 MaxCells, TArray<FAEBehaviourCellSnapshot>& OutCells) const;

	/** Returns flat indices changed since the last clear. */
	const TArray<int32>& GetDirtyCellIndices() const { return DirtyCellIndices; }
	/** Clears per-tick dirty tracking without changing cell data. */
	void ClearDirtyCells();
	/** Returns the revision incremented for accepted mutations. */
	uint64 GetBehaviourRevision() const { return BehaviourRevision; }
	/** Returns aggregate acceptance and rejection statistics. */
	const FAEBehaviourGridStats& GetStats() const { return Stats; }

private:
	/** Tracks sequence and last-cell state for one agent. */
	struct FAgentState
	{
		/** Stores the latest accepted sequence number. */
		int64 LastSequenceNumber = INDEX_NONE;
		/** Stores the latest accepted timestamp in seconds. */
		double LastTimestamp = -DBL_MAX;
		/** Stores the last mapped cell coordinate. */
		FIntPoint LastCell = FIntPoint::ZeroValue;
		/** Indicates whether sequence and timestamp guards are initialized. */
		bool bHasSequence = false;
		/** Indicates whether the agent previously occupied the grid. */
		bool bInsideGrid = false;
	};

	/** Validates sample identity, values, ordering, and tag. */
	bool ValidateSample(const FAEBehaviourSample& Sample, EAEBehaviourSubmitResult& OutResult) const;
	/** Returns whether a tag contributes movement distance. */
	bool IsMovementTag(const FGameplayTag& Tag) const;
	/** Returns whether a tag contributes directional flow. */
	bool IsFlowTag(const FGameplayTag& Tag) const;
	/** Returns whether a tag represents a discrete event. */
	bool IsEventTag(const FGameplayTag& Tag) const;
	/** Marks one valid flat cell index as changed once per tick. */
	void MarkDirty(int32 Index);
	/** Distributes one activity contribution through the Gaussian kernel. */
	void AddSmoothedActivity(const FIntPoint& Coordinate, float Contribution);
	/** Applies raw metric deltas to one valid cell. */
	void AddRawContribution(
		const FIntPoint& Coordinate,
		float PassDelta,
		float DistanceDelta,
		float DwellDelta,
		float SprintDistanceDelta,
		float CollectDelta,
		float CombatDelta,
		const FVector2f& FlowDelta,
		float FlowWeightDelta,
		double Timestamp);
	/** Builds a public snapshot from one internal cell. */
	FAEBehaviourCellSnapshot MakeSnapshot(const FIntPoint& Coordinate, int32 Index) const;

	/** Stores the validated grid configuration. */
	FAEHeatmapGridConfig Config;
	/** Stores the lower-left grid origin in world XY centimetres. */
	FVector2D WorldMin = FVector2D::ZeroVector;
	/** Stores all cells in row-major order. */
	TArray<FAEBehaviourCellData> Cells;
	/** Stores unique flat indices changed during the current tick. */
	TArray<int32> DirtyCellIndices;
	/** Prevents duplicate dirty indices. */
	TBitArray<> DirtyFlags;
	/** Stores ordering and pass state for each agent. */
	TMap<FGuid, FAgentState> AgentStates;
	/** Increments after each accepted sample mutation. */
	uint64 BehaviourRevision = 0;
	/** Stores aggregate sample processing statistics. */
	FAEBehaviourGridStats Stats;
};
