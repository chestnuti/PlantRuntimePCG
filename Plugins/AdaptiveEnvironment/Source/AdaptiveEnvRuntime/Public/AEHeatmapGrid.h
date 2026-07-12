#pragma once

#include "CoreMinimal.h"
#include "AdaptiveEnvTypes.h"

struct FAEBehaviourCellData
{
	float PassCount = 0.0f;
	float TravelDistanceMeters = 0.0f;
	float DwellSeconds = 0.0f;
	float SprintDistanceMeters = 0.0f;
	float CollectEventCount = 0.0f;
	float CombatEventCount = 0.0f;
	FVector2f FlowSum = FVector2f::ZeroVector;
	float FlowWeight = 0.0f;
	float SmoothedActivity = 0.0f;
	double LastUpdatedTime = 0.0;
};

class ADAPTIVEENVRUNTIME_API FAEHeatmapGrid
{
public:
	bool Initialize(const FAEHeatmapGridConfig& InConfig);
	void Reset();

	bool IsValid() const { return Cells.Num() > 0; }
	const FAEHeatmapGridConfig& GetConfig() const { return Config; }
	FBox2D GetWorldBounds() const;

	bool WorldToCell(const FVector& WorldLocation, FIntPoint& OutCoordinate) const;
	bool CellToIndex(const FIntPoint& Coordinate, int32& OutIndex) const;
	FVector GetCellWorldCenter(const FIntPoint& Coordinate) const;

	EAEBehaviourSubmitResult AccumulateSample(const FAEBehaviourSample& Sample);
	void RecordRejectedSample(EAEBehaviourSubmitResult Result);
	bool GetCellSnapshot(const FIntPoint& Coordinate, FAEBehaviourCellSnapshot& OutSnapshot) const;
	bool GetCellSnapshotAtWorldLocation(const FVector& WorldLocation, FAEBehaviourCellSnapshot& OutSnapshot) const;
	void GetNonEmptyCellsInRadius(const FVector& WorldLocation, float RadiusCm, int32 MaxCells, TArray<FAEBehaviourCellSnapshot>& OutCells) const;

	const TArray<int32>& GetDirtyCellIndices() const { return DirtyCellIndices; }
	void ClearDirtyCells();
	uint64 GetBehaviourRevision() const { return BehaviourRevision; }
	const FAEBehaviourGridStats& GetStats() const { return Stats; }

private:
	struct FAgentState
	{
		int64 LastSequenceNumber = INDEX_NONE;
		double LastTimestamp = -DBL_MAX;
		FIntPoint LastCell = FIntPoint::ZeroValue;
		bool bHasSequence = false;
		bool bInsideGrid = false;
	};

	bool ValidateSample(const FAEBehaviourSample& Sample, EAEBehaviourSubmitResult& OutResult) const;
	bool IsMovementTag(const FGameplayTag& Tag) const;
	bool IsFlowTag(const FGameplayTag& Tag) const;
	bool IsEventTag(const FGameplayTag& Tag) const;
	void MarkDirty(int32 Index);
	void AddSmoothedActivity(const FIntPoint& Coordinate, float Contribution);
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
	FAEBehaviourCellSnapshot MakeSnapshot(const FIntPoint& Coordinate, int32 Index) const;

	FAEHeatmapGridConfig Config;
	FVector2D WorldMin = FVector2D::ZeroVector;
	TArray<FAEBehaviourCellData> Cells;
	TArray<int32> DirtyCellIndices;
	TBitArray<> DirtyFlags;
	TMap<FGuid, FAgentState> AgentStates;
	uint64 BehaviourRevision = 0;
	FAEBehaviourGridStats Stats;
};
