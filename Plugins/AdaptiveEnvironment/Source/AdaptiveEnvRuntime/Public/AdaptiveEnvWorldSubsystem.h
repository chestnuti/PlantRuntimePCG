#pragma once

#include "CoreMinimal.h"
#include "AEHeatmapGrid.h"
#include "Subsystems/WorldSubsystem.h"
#include "AdaptiveEnvWorldSubsystem.generated.h"

class UAEBehaviourTrackerComponent;
class UAEHeatmapRendererComponent;

UCLASS()
class ADAPTIVEENVRUNTIME_API UAEAdaptiveEnvWorldSubsystem final : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/* Initializes runtime clocks, settings, and the behaviour grid for one World. */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	/* Clears registrations, queues, and grid state before World teardown. */
	virtual void Deinitialize() override;
	/* Advances the ordered fixed-step behaviour pipeline. */
	virtual void Tick(float DeltaTime) override;
	/* Returns whether runtime updates are enabled and initialized. */
	virtual bool IsTickable() const override;
	/* Exposes this subsystem to Unreal cycle statistics. */
	virtual TStatId GetStatId() const override;

	/* Returns the unique identity of this World subsystem instance. */
	UFUNCTION(BlueprintPure, Category = "Adaptive Environment")
	FGuid GetInstanceId() const { return InstanceId; }

	/* Returns the number of render ticks observed by this subsystem. */
	UFUNCTION(BlueprintPure, Category = "Adaptive Environment")
	int64 GetTickCount() const { return TickCount; }

	/* Returns elapsed fixed-step behaviour time in seconds. */
	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|Behaviour")
	double GetBehaviourTimeSeconds() const { return BehaviourTimeSeconds; }

	/* Returns the number of completed behaviour simulation steps. */
	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|Behaviour")
	int64 GetProcessedBehaviourStepCount() const { return ProcessedBehaviourStepCount; }

	/* Returns the number of frames that exceeded the substep budget. */
	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|Behaviour")
	int64 GetSchedulerOverrunCount() const { return SchedulerOverrunCount; }

	/* Validates and queues one behaviour sample for deterministic processing. */
	UFUNCTION(BlueprintCallable, Category = "Adaptive Environment|Behaviour")
	EAEBehaviourSubmitResult SubmitBehaviourSample(const FAEBehaviourSample& Sample);

	/* Queues a tracker for registration at the next safe tick boundary. */
	void RegisterBehaviourTracker(UAEBehaviourTrackerComponent* Tracker);
	/* Queues a tracker for removal at the next safe tick boundary. */
	void UnregisterBehaviourTracker(UAEBehaviourTrackerComponent* Tracker);
	/* Queues a debug renderer for registration. */
	void RegisterHeatmapRenderer(UAEHeatmapRendererComponent* Renderer);
	/* Queues a debug renderer for removal. */
	void UnregisterHeatmapRenderer(UAEHeatmapRendererComponent* Renderer);

	/* Reads the cell containing a world position in centimetres. */
	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|Heatmap")
	bool GetBehaviourCellAtWorldLocation(const FVector& Location, FAEBehaviourCellSnapshot& OutSnapshot) const;

	/* Reads a cell by integer XY coordinate. */
	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|Heatmap")
	bool GetBehaviourCell(const FIntPoint& Coordinate, FAEBehaviourCellSnapshot& OutSnapshot) const;

	/* Returns grid width and height in cells. */
	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|Heatmap")
	FIntPoint GetGridDimensions() const;

	/* Returns the half-open grid bounds in world XY centimetres. */
	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|Heatmap")
	FBox2D GetGridWorldBounds() const;

	/* Returns the revision incremented by accepted grid changes. */
	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|Heatmap")
	int64 GetBehaviourRevision() const;

	/* Returns the number of cells changed during the current tick. */
	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|Heatmap")
	int32 GetDirtyCellCount() const;

	/* Returns aggregate sample acceptance and rejection statistics. */
	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|Heatmap")
	FAEBehaviourGridStats GetBehaviourGridStats() const;

	/* Clears behaviour data, clocks, queue guards, and tracker sampling state. */
	UFUNCTION(BlueprintCallable, Category = "Adaptive Environment|Heatmap")
	void ResetBehaviourGrid();

	/* Collects non-empty cells around a world position for debug drawing. */
	void GetDebugCells(const FVector& Location, float RadiusCm, int32 MaxCells, TArray<FAEBehaviourCellSnapshot>& OutCells) const;

protected:
	/* Restricts the subsystem to playable World types. */
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

private:
	/* Applies deferred tracker and renderer registration changes. */
	void ApplyPendingRegistrations();
	/* Pulls one sample from every valid tracker for a fixed step. */
	void SampleRegisteredTrackers(float StepSeconds);
	/* Sorts and aggregates the currently queued sample batch. */
	void ProcessPendingSamples();
	/* Refreshes registered debug renderers at the configured rate. */
	void UpdateDebugRenderers(float DeltaTime);
	/* Validates sample identity, values, and behaviour tag before queuing. */
	bool ValidateQueuedSample(const FAEBehaviourSample& Sample, EAEBehaviourSubmitResult& OutResult) const;

	/* Uniquely identifies this subsystem instance. */
	FGuid InstanceId;
	/* Counts render ticks received by the subsystem. */
	int64 TickCount = 0;
	/* Controls whether the ordered runtime pipeline can tick. */
	bool bRuntimeEnabled = false;
	/* Owns raw two-dimensional behaviour aggregation. */
	FAEHeatmapGrid BehaviourGrid;
	/* Accumulates render time awaiting fixed behaviour steps. */
	double BehaviourAccumulator = 0.0;
	/* Stores elapsed fixed-step behaviour time in seconds. */
	double BehaviourTimeSeconds = 0.0;
	/* Accumulates render time awaiting a debug refresh. */
	double DebugAccumulator = 0.0;
	/* Stores one fixed behaviour step duration in seconds. */
	float BehaviourStepSeconds = 0.1f;
	/* Caps fixed behaviour steps executed during one render frame. */
	int32 MaxBehaviourSubstepsPerFrame = 3;
	/* Counts completed fixed behaviour steps. */
	int64 ProcessedBehaviourStepCount = 0;
	/* Counts frames that exceed the fixed-step catch-up budget. */
	int64 SchedulerOverrunCount = 0;
	/* Stores active non-owning tracker registrations. */
	TArray<TWeakObjectPtr<UAEBehaviourTrackerComponent>> RegisteredTrackers;
	/* Stores trackers awaiting safe registration. */
	TArray<TWeakObjectPtr<UAEBehaviourTrackerComponent>> PendingTrackerAdds;
	/* Stores trackers awaiting safe removal. */
	TArray<TWeakObjectPtr<UAEBehaviourTrackerComponent>> PendingTrackerRemoves;
	/* Stores active non-owning debug renderer registrations. */
	TArray<TWeakObjectPtr<UAEHeatmapRendererComponent>> RegisteredRenderers;
	/* Stores renderers awaiting safe registration. */
	TArray<TWeakObjectPtr<UAEHeatmapRendererComponent>> PendingRendererAdds;
	/* Stores renderers awaiting safe removal. */
	TArray<TWeakObjectPtr<UAEHeatmapRendererComponent>> PendingRendererRemoves;
	/* Collects samples submitted during the current producer phase. */
	TArray<FAEBehaviourSample> PendingSamples;
	/* Owns the stable sample batch currently being sorted and processed. */
	TArray<FAEBehaviourSample> ProcessingSamples;
	/* Tracks the latest queued sequence for each agent. */
	TMap<FGuid, int64> LastQueuedSequenceByAgent;
	/* Tracks the latest queued timestamp for each agent in seconds. */
	TMap<FGuid, double> LastQueuedTimestampByAgent;
};
