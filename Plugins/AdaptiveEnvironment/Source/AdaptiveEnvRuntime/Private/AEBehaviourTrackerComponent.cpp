#include "AEBehaviourTrackerComponent.h"

#include "AdaptiveEnvGameplayTags.h"
#include "AdaptiveEnvSettings.h"
#include "AdaptiveEnvWorldSubsystem.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"

UAEBehaviourTrackerComponent::UAEBehaviourTrackerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UAEBehaviourTrackerComponent::BeginPlay()
{
	Super::BeginPlay();
	EnsureAgentId();

	if (UWorld* World = GetWorld())
	{
		if (UAEAdaptiveEnvWorldSubsystem* Subsystem = World->GetSubsystem<UAEAdaptiveEnvWorldSubsystem>())
		{
			Subsystem->RegisterBehaviourTracker(this);
		}
	}
}

void UAEBehaviourTrackerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UAEAdaptiveEnvWorldSubsystem* Subsystem = World->GetSubsystem<UAEAdaptiveEnvWorldSubsystem>())
		{
			Subsystem->UnregisterBehaviourTracker(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

bool UAEBehaviourTrackerComponent::CaptureBehaviourSample(const double SampleTime, const float StepSeconds, FAEBehaviourSample& OutSample)
{
	EnsureAgentId();
	if (!AgentId.IsValid() || !FMath::IsFinite(SampleTime) || !FMath::IsFinite(StepSeconds) || StepSeconds <= 0.0f)
	{
		return false;
	}

	const FVector CurrentLocation = GetTrackedLocation();
	if (CurrentLocation.ContainsNaN())
	{
		return false;
	}

	OutSample = FAEBehaviourSample();
	OutSample.AgentId = AgentId;
	OutSample.PreviousWorldLocation = PreviousWorldLocation;
	OutSample.WorldLocation = CurrentLocation;
	OutSample.Velocity = GetTrackedVelocity();
	OutSample.Timestamp = SampleTime;
	OutSample.SequenceNumber = NextSequenceNumber++;
	OutSample.bHasPreviousLocation = bHasPreviousSample;

	if (bHasPreviousSample)
	{
		OutSample.DeltaSeconds = StepSeconds;
		OutSample.TravelDistanceMeters = FVector::Distance(PreviousWorldLocation, CurrentLocation) / 100.0f;
	}

	const float MovementSpeed = bHasPreviousSample
		? FVector::Distance(PreviousWorldLocation, CurrentLocation) / StepSeconds
		: OutSample.Velocity.Size();
	const UAdaptiveEnvSettings* Settings = GetDefault<UAdaptiveEnvSettings>();
	if (bIsDwelling)
	{
		bIsDwelling = MovementSpeed < Settings->DwellExitSpeedCmPerSecond;
	}
	else
	{
		bIsDwelling = MovementSpeed <= Settings->DwellEnterSpeedCmPerSecond;
	}

	if (bIsDwelling)
	{
		OutSample.BehaviourTag = AdaptiveEnvGameplayTags::Behaviour_Dwell.GetTag();
	}
	else if (MovementSpeed >= Settings->SprintSpeedCmPerSecond)
	{
		OutSample.BehaviourTag = AdaptiveEnvGameplayTags::Behaviour_Sprint.GetTag();
	}
	else
	{
		OutSample.BehaviourTag = AdaptiveEnvGameplayTags::Behaviour_Move.GetTag();
	}

	PreviousWorldLocation = CurrentLocation;
	PreviousSampleTime = SampleTime;
	bHasPreviousSample = true;
	return true;
}

EAEBehaviourSubmitResult UAEBehaviourTrackerComponent::SubmitBehaviourEvent(const FGameplayTag EventTag, const float Intensity)
{
	if (EventTag != AdaptiveEnvGameplayTags::Behaviour_Collect.GetTag()
		&& EventTag != AdaptiveEnvGameplayTags::Behaviour_Combat.GetTag())
	{
		return EAEBehaviourSubmitResult::InvalidTag;
	}
	if (!FMath::IsFinite(Intensity) || Intensity < 0.0f)
	{
		return EAEBehaviourSubmitResult::InvalidTimestamp;
	}

	EnsureAgentId();
	UWorld* World = GetWorld();
	UAEAdaptiveEnvWorldSubsystem* Subsystem = World ? World->GetSubsystem<UAEAdaptiveEnvWorldSubsystem>() : nullptr;
	if (Subsystem == nullptr)
	{
		return EAEBehaviourSubmitResult::InvalidAgent;
	}

	FAEBehaviourSample Sample;
	Sample.AgentId = AgentId;
	Sample.WorldLocation = GetTrackedLocation();
	Sample.Velocity = GetTrackedVelocity();
	Sample.Timestamp = Subsystem->GetBehaviourTimeSeconds();
	Sample.EventIntensity = Intensity;
	Sample.SequenceNumber = NextSequenceNumber++;
	Sample.BehaviourTag = EventTag;
	return Subsystem->SubmitBehaviourSample(Sample);
}

void UAEBehaviourTrackerComponent::ResetSamplingState()
{
	PreviousWorldLocation = FVector::ZeroVector;
	PreviousSampleTime = 0.0;
	NextSequenceNumber = 0;
	bHasPreviousSample = false;
	bIsDwelling = false;
}

void UAEBehaviourTrackerComponent::SetAgentIdForTesting(const FGuid& InAgentId)
{
	AgentId = InAgentId;
	ResetSamplingState();
}

FVector UAEBehaviourTrackerComponent::GetTrackedLocation() const
{
	if (TrackedComponent != nullptr)
	{
		return TrackedComponent->GetComponentLocation();
	}
	const AActor* Owner = GetOwner();
	return Owner ? Owner->GetActorLocation() : FVector::ZeroVector;
}

FVector UAEBehaviourTrackerComponent::GetTrackedVelocity() const
{
	if (TrackedComponent != nullptr)
	{
		return TrackedComponent->GetComponentVelocity();
	}
	const AActor* Owner = GetOwner();
	return Owner ? Owner->GetVelocity() : FVector::ZeroVector;
}

void UAEBehaviourTrackerComponent::EnsureAgentId()
{
	if (!AgentId.IsValid())
	{
		AgentId = FGuid::NewGuid();
	}
}
