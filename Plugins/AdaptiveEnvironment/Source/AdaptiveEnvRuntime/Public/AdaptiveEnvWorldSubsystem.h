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
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;

	UFUNCTION(BlueprintPure, Category = "Adaptive Environment")
	FGuid GetInstanceId() const { return InstanceId; }

	UFUNCTION(BlueprintPure, Category = "Adaptive Environment")
	int64 GetTickCount() const { return TickCount; }

	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|Behaviour")
	double GetBehaviourTimeSeconds() const { return BehaviourTimeSeconds; }

	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|Behaviour")
	int64 GetProcessedBehaviourStepCount() const { return ProcessedBehaviourStepCount; }

	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|Behaviour")
	int64 GetSchedulerOverrunCount() const { return SchedulerOverrunCount; }

	UFUNCTION(BlueprintCallable, Category = "Adaptive Environment|Behaviour")
	EAEBehaviourSubmitResult SubmitBehaviourSample(const FAEBehaviourSample& Sample);

	void RegisterBehaviourTracker(UAEBehaviourTrackerComponent* Tracker);
	void UnregisterBehaviourTracker(UAEBehaviourTrackerComponent* Tracker);
	void RegisterHeatmapRenderer(UAEHeatmapRendererComponent* Renderer);
	void UnregisterHeatmapRenderer(UAEHeatmapRendererComponent* Renderer);

	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|Heatmap")
	bool GetBehaviourCellAtWorldLocation(const FVector& Location, FAEBehaviourCellSnapshot& OutSnapshot) const;

	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|Heatmap")
	bool GetBehaviourCell(const FIntPoint& Coordinate, FAEBehaviourCellSnapshot& OutSnapshot) const;

	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|Heatmap")
	FIntPoint GetGridDimensions() const;

	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|Heatmap")
	FBox2D GetGridWorldBounds() const;

	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|Heatmap")
	int64 GetBehaviourRevision() const;

	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|Heatmap")
	int32 GetDirtyCellCount() const;

	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|Heatmap")
	FAEBehaviourGridStats GetBehaviourGridStats() const;

	UFUNCTION(BlueprintCallable, Category = "Adaptive Environment|Heatmap")
	void ResetBehaviourGrid();

	void GetDebugCells(const FVector& Location, float RadiusCm, int32 MaxCells, TArray<FAEBehaviourCellSnapshot>& OutCells) const;

protected:
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

private:
	void ApplyPendingRegistrations();
	void SampleRegisteredTrackers(float StepSeconds);
	void ProcessPendingSamples();
	void UpdateDebugRenderers(float DeltaTime);
	bool ValidateQueuedSample(const FAEBehaviourSample& Sample, EAEBehaviourSubmitResult& OutResult) const;

	FGuid InstanceId;
	int64 TickCount = 0;
	bool bRuntimeEnabled = false;
	FAEHeatmapGrid BehaviourGrid;
	double BehaviourAccumulator = 0.0;
	double BehaviourTimeSeconds = 0.0;
	double DebugAccumulator = 0.0;
	float BehaviourStepSeconds = 0.1f;
	int32 MaxBehaviourSubstepsPerFrame = 3;
	int64 ProcessedBehaviourStepCount = 0;
	int64 SchedulerOverrunCount = 0;
	TArray<TWeakObjectPtr<UAEBehaviourTrackerComponent>> RegisteredTrackers;
	TArray<TWeakObjectPtr<UAEBehaviourTrackerComponent>> PendingTrackerAdds;
	TArray<TWeakObjectPtr<UAEBehaviourTrackerComponent>> PendingTrackerRemoves;
	TArray<TWeakObjectPtr<UAEHeatmapRendererComponent>> RegisteredRenderers;
	TArray<TWeakObjectPtr<UAEHeatmapRendererComponent>> PendingRendererAdds;
	TArray<TWeakObjectPtr<UAEHeatmapRendererComponent>> PendingRendererRemoves;
	TArray<FAEBehaviourSample> PendingSamples;
	TArray<FAEBehaviourSample> ProcessingSamples;
	TMap<FGuid, int64> LastQueuedSequenceByAgent;
	TMap<FGuid, double> LastQueuedTimestampByAgent;
};
