#pragma once

#include "CoreMinimal.h"
#include "AEExposureGrid.h"
#include "AEEnvironmentConstraintGrid.h"
#include "AEEcologicalResponseGrid.h"
#include "AEHeatmapGrid.h"
#include "AEPathHeatmapGrid.h"
#include "AEActiveEnvironmentConfig.h"
#include "AEM4Types.h"
#include "AEM7Types.h"
#include "AEM8Types.h"
#include "Subsystems/WorldSubsystem.h"
#include "AdaptiveEnvWorldSubsystem.generated.h"

class UAEBehaviourTrackerComponent;
class UAEHeatmapRendererComponent;
class UAELSystemPlantComponent;
class UAERepresentativePlantManagerComponent;
class UAEMoistureSourceComponent;
class UAEMoistureTextureAsset;
class UAEPathHeatmapRendererComponent;
class UAEVegetationDistributionComponent;
class UAEAdaptiveEnvironmentProfile;
class AActor;

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

	/* Pauses the ordered runtime pipeline for replay observation without pausing the World. */
	UFUNCTION(BlueprintCallable, Category = "Adaptive Environment|Runtime")
	void SetObservationPaused(bool bPaused);

	/* Returns whether replay observation currently freezes the ordered runtime pipeline. */
	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|Runtime")
	bool IsObservationPaused() const { return bObservationPaused; }

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
	/* Queues one self-owned M7 vegetation distribution for safe registration. */
	void RegisterVegetationDistribution(UAEVegetationDistributionComponent* Distribution);
	/* Queues one M7 vegetation distribution for safe removal. */
	void UnregisterVegetationDistribution(UAEVegetationDistributionComponent* Distribution);
	/* Queues one M8 representative plant for safe registration. */
	void RegisterLSystemPlant(UAELSystemPlantComponent* Plant);
	/* Queues one M8 representative plant for safe removal. */
	void UnregisterLSystemPlant(UAELSystemPlantComponent* Plant);
	/* Queues one player-neighborhood M8 activation manager for safe registration. */
	void RegisterRepresentativePlantManager(UAERepresentativePlantManagerComponent* Manager);
	/* Queues one player-neighborhood M8 activation manager for safe removal. */
	void UnregisterRepresentativePlantManager(UAERepresentativePlantManagerComponent* Manager);
	/* Prewarms bounded actor shells for one representative Blueprint class. */
	void EnsureM8PoolPrewarmed(TSubclassOf<AActor> ActorClass, int32 PrewarmCount, int32 PoolCapacity, AActor* Owner);
	/* Acquires one available or newly spawned actor shell within the class capacity. */
	AActor* AcquireM8PooledActor(TSubclassOf<AActor> ActorClass, int32 PoolCapacity, AActor* Owner, FString& OutError);
	/* Returns one fully expired actor shell to its class-specific available pool. */
	bool ReturnM8PooledActor(AActor* Actor);
	/* Stores one persistent broken-module fact before physics side effects. */
	void RecordM8BrokenBranch(int64 StablePointId, FName SpeciesId, int64 RuleContentHash, int32 GenerationSeed, int64 BranchModuleId);
	/* Stores one persistent dead-wood fact independent from recoverable health. */
	void RecordM8DeadWoodBranch(int64 StablePointId, FName SpeciesId, int64 RuleContentHash, int32 GenerationSeed, int64 BranchModuleId);
	/* Reads World-lifetime structural state for one stable representative plant. */
	bool GetM8PersistentPlantState(int64 StablePointId, FAEM8PersistentPlantState& OutState) const;
	/* Hides or restores the M7 HISM representation for one M8-owned stable plant. */
	bool SetM7RepresentativeOverride(int64 StablePointId, bool bM8Active);
	/* Queues occupied M7 Cells for one authoritative M4/M5 baseline pass. */
	void RequestM7BaselineInitialization(const UAEVegetationDistributionComponent* Distribution);

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

	/* Validates and atomically applies one complete M3/M4/M5 product profile. */
	UFUNCTION(BlueprintCallable, Category = "Adaptive Environment|Configuration")
	bool ApplyEnvironmentProfile(UAEAdaptiveEnvironmentProfile* Profile, FString& OutError);

	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|Configuration")
	FName GetActiveEnvironmentProfileId() const { return ActiveEnvironmentConfig.ProfileId; }

	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|Configuration")
	int32 GetEnvironmentConfigVersion() const { return ActiveEnvironmentConfig.ConfigVersion; }

	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|Configuration")
	int64 GetEnvironmentConfigRevision() const { return static_cast<int64>(ActiveEnvironmentConfig.RuntimeRevision); }

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

	/* Returns whether M7 vegetation distribution is enabled. */
	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|M7")
	bool IsM7Enabled() const { return bM7Enabled; }
	/* Returns whether registered M8 representative plants are enabled. */
	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|M8")
	bool IsM8Enabled() const { return bM8Enabled; }
	/* Provides M8 and gameplay systems with immutable per-plant state. */
	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|M7")
	bool GetM7PlantInstanceState(int64 StablePointId, FAEPlantInstanceSnapshot& OutSnapshot) const;
	/* Collects every registered M7 snapshot for deterministic neighborhood selection. */
	void GetM7PlantInstanceStates(TArray<FAEPlantInstanceSnapshot>& OutSnapshots) const;

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
	/* Advances complete M6 path visual state after M5 for one fixed step. */
	void UpdateM6(float StepSeconds);
	/* Derives M7 from the same committed M5 work set without consuming M6 output. */
	void UpdateM7(float StepSeconds);
	/* Activates bounded player-neighborhood M8 representatives after M7 commits. */
	void UpdateM8NeighborhoodActivation(float StepSeconds);
	/* Resolves M8 visual state after M7 commits without rebuilding fixed geometry. */
	void UpdateM8();
	/* Applies bounded per-instance custom-data and transform changes. */
	void UpdateM7VisualAdapters();
	/* Applies queued M6 commands through registered renderers at a bounded rate. */
	void UpdateM6VisualRenderers(float DeltaTime);
	/* Queues one full M6 texture reconstruction for a renderer. */
	void QueueFullM6VisualRebuild(UAEPathHeatmapRendererComponent& Renderer) const;
	/* Rebuilds M3 once from all current raw Cell totals after a profile switch. */
	void RebuildM3FromCurrentRawGrid();
	/* Accumulates raw Cells changed since the previous completed debug refresh. */
	void AccumulateDebugActiveCells();
	/* Builds one stable nearest-first Cell coordinate window around recent activity. */
	void BuildDebugCellCoordinates(const FVector& Location, float RadiusCm, int32 MaxCells, TArray<FIntPoint>& OutCoordinates) const;
	/* Refreshes registered debug renderers at the configured rate. */
	void UpdateDebugRenderers(float DeltaTime);
	/* Validates sample identity, values, and behaviour tag before queuing. */
	bool ValidateQueuedSample(const FAEBehaviourSample& Sample, EAEBehaviourSubmitResult& OutResult) const;
	/* Creates one clean Blueprint actor shell after Construction Script components exist. */
	AActor* SpawnM8PoolActor(TSubclassOf<AActor> ActorClass, AActor* Owner, FString& OutError);

	/* Uniquely identifies this subsystem instance. */
	FGuid InstanceId;
	/* Counts render ticks received by the subsystem. */
	int64 TickCount = 0;
	/* Controls whether the ordered runtime pipeline can tick. */
	bool bRuntimeEnabled = false;
	/* Freezes fixed-step and visual advancement while replay observation is active. */
	bool bObservationPaused = false;
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
	/* Stores the atomically committed product configuration for M3 through M5. */
	FAEActiveEnvironmentConfig ActiveEnvironmentConfig;
	/* Controls M3 updates independently from the valid M1 runtime pipeline. */
	bool bM3Enabled = false;
	/* Controls M4 decisions while sharing the active product configuration. */
	bool bM4Enabled = false;
	/* Controls World-level M5 response updates. */
	bool bM5Enabled = false;
	/* Controls World-level M6 state updates and registered visual outputs. */
	bool bM6Enabled = false;
	/* Controls self-owned M7 distributions when M4 and M5 are available. */
	bool bM7Enabled = false;
	/* Controls registered M8 representative plants independently from M7 availability. */
	bool bM8Enabled = false;
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
	/* Stores unique raw Cell indices changed since the previous debug refresh. */
	TSet<int32> PendingDebugActiveCellIndices;
	/* Stores the Cell selection used by the latest debug refresh. */
	TSet<int32> LastRenderedDebugCellIndices;
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
	/* Stores active non-owning M7 distribution registrations. */
	TArray<TWeakObjectPtr<UAEVegetationDistributionComponent>> RegisteredVegetationDistributions;
	TArray<TWeakObjectPtr<UAEVegetationDistributionComponent>> PendingVegetationDistributionAdds;
	TArray<TWeakObjectPtr<UAEVegetationDistributionComponent>> PendingVegetationDistributionRemoves;
	/* Stores active non-owning M8 representative plant registrations. */
	TArray<TWeakObjectPtr<UAELSystemPlantComponent>> RegisteredLSystemPlants;
	/* Stores M8 plants awaiting safe registration. */
	TArray<TWeakObjectPtr<UAELSystemPlantComponent>> PendingLSystemPlantAdds;
	/* Stores M8 plants awaiting safe removal. */
	TArray<TWeakObjectPtr<UAELSystemPlantComponent>> PendingLSystemPlantRemoves;
	/* Stores active non-owning player-neighborhood activation managers. */
	TArray<TWeakObjectPtr<UAERepresentativePlantManagerComponent>> RegisteredRepresentativePlantManagers;
	TArray<TWeakObjectPtr<UAERepresentativePlantManagerComponent>> PendingRepresentativePlantManagerAdds;
	TArray<TWeakObjectPtr<UAERepresentativePlantManagerComponent>> PendingRepresentativePlantManagerRemoves;
	/* Stores the next registered M8 plant index scheduled for fixed-step advancement. */
	int32 M8UpdateCursor = 0;
	/* Keeps every subsystem-owned pool actor reachable until World teardown. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> M8ManagedPoolActors;
	/* Stores clean available actors grouped by exact Blueprint class. */
	TMap<TObjectKey<UClass>, TArray<TWeakObjectPtr<AActor>>> M8AvailablePoolActorsByClass;
	/* Stores persistent structural facts independently from recyclable actor shells. */
	TMap<int64, FAEM8PersistentPlantState> M8PersistentPlantStates;
	/* Stores unique row-major M7 Cells awaiting their first authoritative M4 sample. */
	TSet<int32> PendingM7BaselineCellIndices;
	/* Stores active registered M4 moisture sources. */
	TArray<TWeakObjectPtr<UAEMoistureSourceComponent>> RegisteredMoistureSources;
	/* Keeps the atomically applied M4 moisture field reachable during runtime sampling. */
	UPROPERTY(Transient)
	TObjectPtr<UAEMoistureTextureAsset> ActiveMoistureTexture;
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
