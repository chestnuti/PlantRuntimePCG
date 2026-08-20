#pragma once

#include "CoreMinimal.h"
#include "AEExperimentTypes.generated.h"

class UAEAdaptiveEnvironmentProfile;

UENUM(BlueprintType)
enum class EAEExperimentType : uint8
{
    P00,
    E01,
    E02,
    E03
};

UENUM(BlueprintType)
enum class EAEExperimentState : uint8
{
    Idle,
    Validating,
    Preparing,
    WarmingUp,
    Replaying,
    Recovering,
    Flushing,
    Completed,
    Failed
};

USTRUCT(BlueprintType)
struct ADAPTIVEENVRESEARCHHARNESS_API FAEExperimentCapturePoint
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment")
    FName CaptureId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment", meta = (ClampMin = "0"))
    int64 SimulationStep = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment")
    FTransform CameraTransform = FTransform::Identity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment")
    FString Description;
};

USTRUCT(BlueprintType)
struct ADAPTIVEENVRESEARCHHARNESS_API FAEExperimentRunSpec
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment")
    FString RunId = TEXT("P00_Pilot_S1337_R00");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment")
    EAEExperimentType Experiment = EAEExperimentType::P00;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment")
    FName ConditionId = TEXT("Pilot");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment", meta = (ClampMin = "0"))
    int32 RepeatIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment")
    int32 Seed = 1337;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment")
    FString ReplaySlotName = TEXT("PlayerReplay");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment")
    TObjectPtr<UAEAdaptiveEnvironmentProfile> Profile = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment", meta = (ClampMin = "0.0", Units = "s"))
    float WarmupSeconds = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment", meta = (ClampMin = "0.0", Units = "s"))
    float RecoverySeconds = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment", meta = (ClampMin = "0.01", Units = "s"))
    float SnapshotIntervalSeconds = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment", meta = (ClampMin = "0"))
    int32 TargetFrameRate = 60;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment")
    FName WorkloadLevel = TEXT("L0");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment")
    bool bRuntimeEnabled = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment")
    TArray<FIntPoint> ObservedCells;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment")
    TArray<FAEExperimentCapturePoint> CapturePoints;
};
