#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AdaptiveEnvTypes.h"
#include "AEBehaviourTrackerComponent.generated.h"

class USceneComponent;

UCLASS(ClassGroup = (AdaptiveEnvironment), meta = (BlueprintSpawnableComponent))
class ADAPTIVEENVRUNTIME_API UAEBehaviourTrackerComponent final : public UActorComponent
{
	GENERATED_BODY()

public:
	/** Creates a tracker driven by the World Subsystem fixed-step scheduler. */
	UAEBehaviourTrackerComponent();

	/** Registers this tracker with the World Subsystem. */
	virtual void BeginPlay() override;
	/** Unregisters this tracker before its owner leaves play. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Captures one movement sample at the supplied simulation time and step. */
	bool CaptureBehaviourSample(double SampleTime, float StepSeconds, FAEBehaviourSample& OutSample);

	/** Submits one discrete tagged event with a non-negative intensity. */
	UFUNCTION(BlueprintCallable, Category = "Adaptive Environment|Behaviour")
	EAEBehaviourSubmitResult SubmitBehaviourEvent(FGameplayTag EventTag, float Intensity = 1.0f);

	/** Returns the stable identity assigned to this tracked agent. */
	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|Behaviour")
	FGuid GetAgentId() const { return AgentId; }

	/** Clears previous-position, sequence, and dwell classification state. */
	UFUNCTION(BlueprintCallable, Category = "Adaptive Environment|Behaviour")
	void ResetSamplingState();

	/** Replaces the agent identity to support deterministic tests. */
	void SetAgentIdForTesting(const FGuid& InAgentId);

	/** Uses this component for sampled location when set. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour")
	TObjectPtr<USceneComponent> TrackedComponent;

private:
	/** Returns the tracked component location or owner location in centimetres. */
	FVector GetTrackedLocation() const;
	/** Returns the owner velocity in centimetres per second. */
	FVector GetTrackedVelocity() const;
	/** Creates a stable agent identity when none exists. */
	void EnsureAgentId();

	/** Uniquely identifies this tracked agent. */
	FGuid AgentId;
	/** Stores the previous sampled world position in centimetres. */
	FVector PreviousWorldLocation = FVector::ZeroVector;
	/** Stores the previous simulation sample time in seconds. */
	double PreviousSampleTime = 0.0;
	/** Provides the next monotonically increasing sample sequence. */
	int64 NextSequenceNumber = 0;
	/** Indicates whether the previous sample state is valid. */
	bool bHasPreviousSample = false;
	/** Stores the current hysteresis-based dwell classification. */
	bool bIsDwelling = false;
};
