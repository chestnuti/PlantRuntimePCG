#pragma once

#include "CoreMinimal.h"
#include "AEExposureGrid.h"
#include "AEEnvironmentConstraintGrid.h"
#include "AEEcologicalResponseGrid.h"
#include "AEHeatmapGrid.h"
#include "AEPathHeatmapGrid.h"
#include "AEVegetationPatchStateService.h"
#include "AEM5ConsumerTypes.h"
#include "AEM4Types.h"
#include "Subsystems/WorldSubsystem.h"
#include "AdaptiveEnvWorldSubsystem.generated.h"

class UAEBehaviourTrackerComponent;
class UAEHeatmapRendererComponent;
class UAEMoistureSourceComponent;
class UAEPathHeatmapRendererComponent;
class UAEVegetationPatchComponent;
class UAEPublishedParameterBundleAsset;

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
	/* Queues one M4 moisture source for safe registration. */
	void RegisterMoistureSource(UAEMoistureSourceComponent* Source);
	/* Queues one M4 moisture source for safe removal. */
	void UnregisterMoistureSource(UAEMoistureSourceComponent* Source);
	/* Queues one M6 renderer for safe registration. */
	void RegisterPathHeatmapRenderer(UAEPathHeatmapRendererComponent* Renderer);
	/* Queues one M6 renderer for safe removal. */
	void UnregisterPathHeatmapRenderer(UAEPathHeatmapRendererComponent* Renderer);
	/* Queues one M7 Patch for safe registration. */
	void RegisterVegetationPatch(UAEVegetationPatchComponent* Patch);
	/* Queues one M7 Patch for safe removal. */
	void UnregisterVegetationPatch(UAEVegetationPatchComponent* Patch);
	/* Queues one active M7 Patch for safe removal and rebuilt registration. */
	void RefreshVegetationPatch(UAEVegetationPatchComponent* Patch);

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

	/* Returns whether M3 has a complete validated parameter snapshot. */
	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|M3")
	bool IsM3Enabled() const { return bM3Enabled; }

	/* Returns whether M4 has a complete validated parameter snapshot. */
	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|M4")
	bool IsM4Enabled() const { return bM4Enabled; }

	/* Returns whether M5 has a complete validated parameter snapshot. */
	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|M5")
	bool IsM5Enabled() const { return bM5Enabled; }

	/* Validates and atomically applies one complete M3/M4/M5 published parameter bundle. */
	UFUNCTION(BlueprintCallable, Category = "Adaptive Environment|Parameters")
	bool ApplyParameterBundle(UAEPublishedParameterBundleAsset* Bundle, FString& OutError);

	/* Reads one M3 Cell by integer XY coordinate. */
	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|M3")
	bool GetM3Cell(const FIntPoint& Coordinate, FAEM3CellSnapshot& OutSnapshot) const;

	/* Reads one M3 Cell containing a world position in centimetres. */
	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|M3")
	bool GetM3CellAtWorldLocation(const FVector& Location, FAEM3CellSnapshot& OutSnapshot) const;

	/* Returns the latest global M3 Exposure revision. */
	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|M3")
	int64 GetExposureRevision() const { return static_cast<int64>(ExposureGrid.GetExposureRevision()); }

	/* Reads one committed M4 Cell by integer XY coordinate. */
	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|M4")
	bool GetM4Cell(const FIntPoint& Coordinate, FAEEnvironmentConstraintSnapshot& OutSnapshot) const;
	/* Reads one committed M4 Cell containing a world position. */
	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|M4")
	bool GetM4CellAtWorldLocation(const FVector& Location, FAEEnvironmentConstraintSnapshot& OutSnapshot) const;
	/* Returns the latest committed M4 revision. */
	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|M4")
	int64 GetConstraintRevision() const { return static_cast<int64>(ConstraintGrid.GetConstraintRevision()); }

	/* Reads one committed M5 Cell by integer XY coordinate. */
	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|M5")
	bool GetM5Cell(const FIntPoint& Coordinate, FAEEcologicalResponseSnapshot& OutSnapshot) const;
	/* Reads one committed M5 Cell containing a world position. */
	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|M5")
	bool GetM5CellAtWorldLocation(const FVector& Location, FAEEcologicalResponseSnapshot& OutSnapshot) const;
	/* Returns the latest committed M5 revision. */
	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|M5")
	int64 GetResponseRevision() const { return static_cast<int64>(ResponseGrid.GetResponseRevision()); }

	/* Returns whether M6 path visual state is enabled. */
	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|M6")
	bool IsM6Enabled() const { return bM6Enabled; }
	/* Reads one committed M6 Cell by integer XY coordinate. */
	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|M6")
	bool GetM6Cell(const FIntPoint& Coordinate, FAEPathHeatmapSnapshot& OutSnapshot) const;
	/* Reads one committed M6 Cell containing a world position. */
	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|M6")
	bool GetM6CellAtWorldLocation(const FVector& Location, FAEPathHeatmapSnapshot& OutSnapshot) const;
	/* Returns the latest committed M6 visual-state revision. */
	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|M6")
	int64 GetPathVisualRevision() const { return static_cast<int64>(PathHeatmapGrid.GetPathVisualRevision()); }

	/* Returns whether M7 Patch response is enabled. */
	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|M7")
	bool IsM7Enabled() const { return bM7Enabled; }
	/* Reads one committed M7 Patch by stable identity. */
	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|M7")
	bool GetM7Patch(const FGuid& PatchId, FAEVegetationPatchSnapshot& OutSnapshot) const;
	/* Returns the latest committed M7 Patch-state revision. */
	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|M7")
	int64 GetVegetationPatchRevision() const
	{
		/* Convert the internal unsigned revision to the reflected signed contract. */
		return static_cast<int64>(VegetationPatchService.GetPatchStateRevision());
	}

	/* Collects non-empty cells around a world position for debug drawing. */
	void GetDebugCells(const FVector& Location, float RadiusCm, int32 MaxCells, TArray<FAEBehaviourCellSnapshot>& OutCells) const;
	/* Collects active M3 cells around a world position for read-only debug drawing. */
	void GetM3DebugCells(const FVector& Location, float RadiusCm, int32 MaxCells, TArray<FAEM3CellSnapshot>& OutCells) const;
	/* Collects committed M4 cells around recent activity for debug drawing. */
	void GetM4DebugCells(const FVector& Location, float RadiusCm, int32 MaxCells, TArray<FAEEnvironmentConstraintSnapshot>& OutCells) const;
	/* Collects committed M5 cells around recent activity for debug drawing. */
	void GetM5DebugCells(const FVector& Location, float RadiusCm, int32 MaxCells, TArray<FAEEcologicalResponseSnapshot>& OutCells) const;
	/* Returns the parameter-derived default colour maximum for one M3 debug mode. */
	float GetM3DebugMaximumValue(EAEHeatmapDebugMode Mode) const;

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
	/* Advances Exposure and ecological response after raw aggregation for one fixed step. */
	void UpdateM3(float StepSeconds);
	/* Samples World constraints and commits M4 after M3 for one fixed step. */
	void UpdateM4(float StepSeconds);
	/* Freezes compatible M3/M4 inputs and commits M5 after M4. */
	void UpdateM5(float StepSeconds);
	/* Builds one shared M5 view and advances parallel M6/M7 consumers. */
	void UpdateM6AndM7(float StepSeconds);
	/* Freezes the minimal committed M5 fields once for downstream consumers. */
	void BuildM5ConsumerWorkset(const TArray<int32>& CellIndices, TArray<FAEM5ConsumerCellView>& OutWorkset) const;
	/* Advances complete M6 path visual state from the shared M5 workset. */
	void UpdateM6(float StepSeconds, const TArray<int32>& CandidateIndices, TConstArrayView<FAEM5ConsumerCellView> Workset);
	/* Advances M7 Patch state from the same shared M5 workset. */
	void UpdateM7(float StepSeconds, TConstArrayView<FAEM5ConsumerCellView> Workset);
	/* Applies queued M6 commands through registered renderers at a bounded rate. */
	void UpdateM6VisualRenderers(float DeltaTime);
	/* Applies queued M7 commands through registered Patch components at bounded rates. */
	void UpdateM7VisualAdapters(float DeltaTime);
	/* Queues one full M6 texture reconstruction for a renderer. */
	void QueueFullM6VisualRebuild(UAEPathHeatmapRendererComponent& Renderer) const;
	/* Rebuilds M3 once from all current raw Cell totals after a parameter-package switch. */
	void RebuildM3FromCurrentRawGrid();
	/* Accumulates raw Cells changed since the previous completed debug refresh. */
	void AccumulateDebugActiveCells();
	/* Builds one stable nearest-first Cell coordinate window around recent activity. */
	void BuildDebugCellCoordinates(const FVector& Location, float RadiusCm, int32 MaxCells, TArray<FIntPoint>& OutCoordinates) const;
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
	/* Owns derived M3 Exposure state aligned with the raw Grid. */
	FAEExposureGrid ExposureGrid;
	/* Owns sampled M4 constraints and state decisions aligned with M1. */
	FAEEnvironmentConstraintGrid ConstraintGrid;
	/* Owns fused M5 ecological response state aligned with M1. */
	FAEEcologicalResponseGrid ResponseGrid;
	/* Owns complete M6 path visual state aligned with M1-M5. */
	FAEPathHeatmapGrid PathHeatmapGrid;
	/* Owns deterministic M7 Patch aggregation and health state. */
	FAEVegetationPatchStateService VegetationPatchService;
	/* Stores the atomically committed bundle identity and grouped M3/M4 parameter values. */
	FAEActiveParameterSnapshot ActiveParameters;
	/* Controls M3 updates independently from the valid M1 runtime pipeline. */
	bool bM3Enabled = false;
	/* Controls M4 decisions independently while sharing the active bundle snapshot. */
	bool bM4Enabled = false;
	/* Controls World-level M5 response updates. */
	bool bM5Enabled = false;
	/* Controls World-level M6 state updates and registered visual outputs. */
	bool bM6Enabled = false;
	/* Controls World-level M7 state updates and registered instance outputs. */
	bool bM7Enabled = false;
	/* Stores the validated effective M6 parameter snapshot for this World. */
	FAEM6ParameterSet M6Parameters;
	/* Counts failed M4 World samples retained by fail-closed submission. */
	uint64 M4InvalidSampleCount = 0;
	/* Converts one real second into simulated hours for M3 integration. */
	double SimulationHoursPerRealSecond = 0.0;
	/* Accumulates render time awaiting fixed behaviour steps. */
	double BehaviourAccumulator = 0.0;
	/* Stores elapsed fixed-step behaviour time in seconds. */
	double BehaviourTimeSeconds = 0.0;
	/* Accumulates render time awaiting a debug refresh. */
	double DebugAccumulator = 0.0;
	/* Accumulates render time awaiting a bounded M6 texture refresh. */
	double M6VisualAccumulator = 0.0;
	/* Accumulates render time awaiting one bounded M7 instance refresh. */
	double M7VisualAccumulator = 0.0;
	/* Stores unique raw Cell indices changed since the previous debug refresh. */
	TSet<int32> PendingDebugActiveCellIndices;
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
	/* Stores active non-owning M6 renderer registrations. */
	TArray<TWeakObjectPtr<UAEPathHeatmapRendererComponent>> RegisteredPathHeatmapRenderers;
	/* Stores M6 renderers awaiting safe registration. */
	TArray<TWeakObjectPtr<UAEPathHeatmapRendererComponent>> PendingPathHeatmapRendererAdds;
	/* Stores M6 renderers awaiting safe removal. */
	TArray<TWeakObjectPtr<UAEPathHeatmapRendererComponent>> PendingPathHeatmapRendererRemoves;
	/* Coalesces latest fixed-step visual commands until the next visual refresh. */
	TMap<int32, FAEPathHeatmapVisualCommand> PendingM6VisualCommands;
	/* Stores active registered M7 Patch components. */
	TArray<TWeakObjectPtr<UAEVegetationPatchComponent>> RegisteredVegetationPatches;
	/* Stores M7 Patches awaiting safe registration. */
	TArray<TWeakObjectPtr<UAEVegetationPatchComponent>> PendingVegetationPatchAdds;
	/* Stores M7 Patches awaiting safe removal. */
	TArray<TWeakObjectPtr<UAEVegetationPatchComponent>> PendingVegetationPatchRemoves;
	/* Coalesces the newest visual command per stable Patch identity. */
	TMap<FGuid, FAEVegetationPatchVisualCommand> PendingM7VisualCommands;
	/* Stores active registered M4 moisture sources. */
	TArray<TWeakObjectPtr<UAEMoistureSourceComponent>> RegisteredMoistureSources;
	/* Stores M4 moisture sources awaiting safe registration. */
	TArray<TWeakObjectPtr<UAEMoistureSourceComponent>> PendingMoistureSourceAdds;
	/* Stores M4 moisture sources awaiting safe removal. */
	TArray<TWeakObjectPtr<UAEMoistureSourceComponent>> PendingMoistureSourceRemoves;
	/* Collects samples submitted during the current producer phase. */
	TArray<FAEBehaviourSample> PendingSamples;
	/* Owns the stable sample batch currently being sorted and processed. */
	TArray<FAEBehaviourSample> ProcessingSamples;
	/* Tracks the latest queued sequence for each agent. */
	TMap<FGuid, int64> LastQueuedSequenceByAgent;
	/* Tracks the latest queued timestamp for each agent in seconds. */
	TMap<FGuid, double> LastQueuedTimestampByAgent;
};
