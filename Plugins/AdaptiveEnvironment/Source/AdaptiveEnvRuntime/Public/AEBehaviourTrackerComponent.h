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
	UAEBehaviourTrackerComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	bool CaptureBehaviourSample(double SampleTime, float StepSeconds, FAEBehaviourSample& OutSample);

	UFUNCTION(BlueprintCallable, Category = "Adaptive Environment|Behaviour")
	EAEBehaviourSubmitResult SubmitBehaviourEvent(FGameplayTag EventTag, float Intensity = 1.0f);

	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|Behaviour")
	FGuid GetAgentId() const { return AgentId; }

	UFUNCTION(BlueprintCallable, Category = "Adaptive Environment|Behaviour")
	void ResetSamplingState();

	void SetAgentIdForTesting(const FGuid& InAgentId);

	/** Uses this component for sampled location when set. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour")
	TObjectPtr<USceneComponent> TrackedComponent;

private:
	FVector GetTrackedLocation() const;
	FVector GetTrackedVelocity() const;
	void EnsureAgentId();

	FGuid AgentId;
	FVector PreviousWorldLocation = FVector::ZeroVector;
	double PreviousSampleTime = 0.0;
	int64 NextSequenceNumber = 0;
	bool bHasPreviousSample = false;
	bool bIsDwelling = false;
};
