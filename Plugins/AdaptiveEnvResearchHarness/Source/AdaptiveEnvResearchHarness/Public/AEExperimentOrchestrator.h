#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AEExperimentTypes.h"
#include "AEExperimentOrchestrator.generated.h"

class APlayerReplayManager;
class UAEAdaptiveEnvWorldSubsystem;

UCLASS(BlueprintType, Blueprintable)
class ADAPTIVEENVRESEARCHHARNESS_API AAEExperimentOrchestrator final : public AActor
{
    GENERATED_BODY()

public:
    AAEExperimentOrchestrator();

    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment")
    FAEExperimentRunSpec RunSpec;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment")
    bool bAutoStartOnBeginPlay = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Experiment")
    EAEExperimentState State = EAEExperimentState::Idle;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Experiment")
    FString FailureReason;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Experiment")
    FString OutputDirectory;

    UFUNCTION(BlueprintCallable, Category = "Experiment")
    bool StartExperiment();

    UFUNCTION(BlueprintCallable, Category = "Experiment")
    void AbortExperiment(const FString& Reason);

    UFUNCTION(BlueprintPure, Category = "Experiment|Identity")
    static FString BuildRunId(EAEExperimentType Experiment, FName ConditionId, int32 Seed, int32 RepeatIndex);

    UFUNCTION(BlueprintPure, Category = "Experiment|Identity")
    static bool IsValidRunId(const FString& RunId);

protected:
    virtual void BeginPlay() override;

private:
    bool ValidateAndPrepare();
    void AdvanceState(float DeltaSeconds);
    void CollectSnapshot(float DeltaSeconds);
    void CollectCells();
    void CollectPlantsAndLSystems();
    void CompleteExperiment();
    void FailExperiment(const FString& Reason);
    bool FlushResults(const FString& Status, FString& OutError);
    bool ApplyExperimentSeed(FString& OutError);
    void AppendEvent(const FString& Event, const FString& Detail = FString());
    FString MakeManifestJson(const FString& Status) const;
    static FString CsvEscape(const FString& Value);

    UPROPERTY(Transient)
    TObjectPtr<APlayerReplayManager> ReplayManager;

    UPROPERTY(Transient)
    TObjectPtr<UAEAdaptiveEnvWorldSubsystem> Environment;

    double StateElapsedSeconds = 0.0;
    double SnapshotAccumulatorSeconds = 0.0;
    double RunElapsedSeconds = 0.0;
    int64 LastCollectedStep = INDEX_NONE;
    int64 SampleIndex = 0;
    bool bReplayStarted = false;
    bool bPreviousReplayState = false;
    bool bPreviousObservationPaused = false;
    int32 AppliedM7DistributionCount = 0;
    int32 AppliedM8PlantCount = 0;
    float PreviousMaxFps = 0.0f;
    TSet<FName> EmittedCapturePoints;
    TArray<FString> CellRows;
    TArray<FString> PlantRows;
    TArray<FString> LSystemRows;
    TArray<FString> PerformanceRows;
    TArray<FString> EventRows;
};
