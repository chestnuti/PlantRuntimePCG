#include "AEBehaviourTrackerComponent.h"

#include "AdaptiveEnvGameplayTags.h"
#include "AdaptiveEnvSettings.h"
#include "AdaptiveEnvWorldSubsystem.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"

// Disable component Tick because the World Subsystem owns sampling cadence.
UAEBehaviourTrackerComponent::UAEBehaviourTrackerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

// Initialize identity and defer tracker registration through the subsystem.
void UAEBehaviourTrackerComponent::BeginPlay()
{
	Super::BeginPlay();
	EnsureAgentId();

	// Register only when the owning World exposes the adaptive subsystem.
	if (UWorld* World = GetWorld())
	{
		if (UAEAdaptiveEnvWorldSubsystem* Subsystem = World->GetSubsystem<UAEAdaptiveEnvWorldSubsystem>())
		{
			Subsystem->RegisterBehaviourTracker(this);
		}
	}
}

// Remove the tracker before the component or its World is destroyed.
void UAEBehaviourTrackerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Queue safe removal from the subsystem registration list.
	if (UWorld* World = GetWorld())
	{
		if (UAEAdaptiveEnvWorldSubsystem* Subsystem = World->GetSubsystem<UAEAdaptiveEnvWorldSubsystem>())
		{
			Subsystem->UnregisterBehaviourTracker(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

// Build one deterministic movement sample from the current tracked transform.
bool UAEBehaviourTrackerComponent::CaptureBehaviourSample(const double SampleTime, const float StepSeconds, FAEBehaviourSample& OutSample)
{
	// Reject invalid identity or time input before reading actor state.
	EnsureAgentId();
	if (!AgentId.IsValid() || !FMath::IsFinite(SampleTime) || !FMath::IsFinite(StepSeconds) || StepSeconds <= 0.0f)
	{
		return false;
	}

	// Read and validate the current sample position.
	const FVector CurrentLocation = GetTrackedLocation();
	if (CurrentLocation.ContainsNaN())
	{
		return false;
	}

	// Populate shared sample identity, motion, and ordering fields.
	OutSample = FAEBehaviourSample();
	OutSample.AgentId = AgentId;
	OutSample.PreviousWorldLocation = PreviousWorldLocation;
	OutSample.WorldLocation = CurrentLocation;
	OutSample.Velocity = GetTrackedVelocity();
	OutSample.Timestamp = SampleTime;
	OutSample.SequenceNumber = NextSequenceNumber++;
	OutSample.bHasPreviousLocation = bHasPreviousSample;

	// Derive elapsed distance only when a previous position exists.
	if (bHasPreviousSample)
	{
		OutSample.DeltaSeconds = StepSeconds;
		OutSample.TravelDistanceMeters = FVector::Distance(PreviousWorldLocation, CurrentLocation) / 100.0f;
	}

	// Classify movement with dwell hysteresis to avoid threshold flicker.
	const float MovementSpeed = bHasPreviousSample
		? FVector::Distance(PreviousWorldLocation, CurrentLocation) / StepSeconds
		: OutSample.Velocity.Size();
	const UAdaptiveEnvSettings* Settings = GetDefault<UAdaptiveEnvSettings>();
	// Assign the movement tag consumed by grid aggregation.
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

	// Commit state only after a complete valid sample is produced.
	PreviousWorldLocation = CurrentLocation;
	PreviousSampleTime = SampleTime;
	bHasPreviousSample = true;
	return true;
}

// Validate and submit a discrete collection or combat event.
EAEBehaviourSubmitResult UAEBehaviourTrackerComponent::SubmitBehaviourEvent(const FGameplayTag EventTag, const float Intensity)
{
	// Accept only event tags handled by the raw behaviour grid.
	if (EventTag != AdaptiveEnvGameplayTags::Behaviour_Collect.GetTag()
		&& EventTag != AdaptiveEnvGameplayTags::Behaviour_Combat.GetTag())
	{
		return EAEBehaviourSubmitResult::InvalidTag;
	}
	if (!FMath::IsFinite(Intensity) || Intensity < 0.0f)
	{
		return EAEBehaviourSubmitResult::InvalidTimestamp;
	}

	// Resolve the current subsystem and stable agent identity.
	EnsureAgentId();
	UWorld* World = GetWorld();
	UAEAdaptiveEnvWorldSubsystem* Subsystem = World ? World->GetSubsystem<UAEAdaptiveEnvWorldSubsystem>() : nullptr;
	if (Subsystem == nullptr)
	{
		return EAEBehaviourSubmitResult::InvalidAgent;
	}

	// Build an instantaneous event sample at current simulation time.
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

// Restore first-sample behavior and deterministic sequence numbering.
void UAEBehaviourTrackerComponent::ResetSamplingState()
{
	PreviousWorldLocation = FVector::ZeroVector;
	PreviousSampleTime = 0.0;
	NextSequenceNumber = 0;
	bHasPreviousSample = false;
	bIsDwelling = false;
}

// Inject a stable identity and reset dependent sampling state for tests.
void UAEBehaviourTrackerComponent::SetAgentIdForTesting(const FGuid& InAgentId)
{
	AgentId = InAgentId;
	ResetSamplingState();
}

// Prefer an explicit tracked component and fall back to the owner transform.
FVector UAEBehaviourTrackerComponent::GetTrackedLocation() const
{
	if (TrackedComponent != nullptr)
	{
		return TrackedComponent->GetComponentLocation();
	}
	const AActor* Owner = GetOwner();
	return Owner ? Owner->GetActorLocation() : FVector::ZeroVector;
}

// Prefer component velocity and fall back to the owner velocity.
FVector UAEBehaviourTrackerComponent::GetTrackedVelocity() const
{
	if (TrackedComponent != nullptr)
	{
		return TrackedComponent->GetComponentVelocity();
	}
	const AActor* Owner = GetOwner();
	return Owner ? Owner->GetVelocity() : FVector::ZeroVector;
}

// Allocate the agent identity once and preserve it across samples.
void UAEBehaviourTrackerComponent::EnsureAgentId()
{
	if (!AgentId.IsValid())
	{
		AgentId = FGuid::NewGuid();
	}
}
