#include "AERepresentativePlantManagerComponent.h"

#include "AdaptiveEnvLog.h"
#include "AdaptiveEnvWorldSubsystem.h"
#include "AELSystemPlantComponent.h"
#include "AEM8Types.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

/* Create a manager whose work remains ordered by the World subsystem. */
UAERepresentativePlantManagerComponent::UAERepresentativePlantManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

/* Register after the owner enters a playable World. */
void UAERepresentativePlantManagerComponent::BeginPlay()
{
	Super::BeginPlay();
	SelectionAccumulatorSeconds = FMath::Max(static_cast<double>(SelectionIntervalSeconds), 0.01);
	if (UWorld* World = GetWorld())
	{
		if (UAEAdaptiveEnvWorldSubsystem* Subsystem = World->GetSubsystem<UAEAdaptiveEnvWorldSubsystem>())
		{
			Subsystem->RegisterRepresentativePlantManager(this);
		}
	}
}

/* Remove subsystem ownership and clean up manager-owned Actors at World teardown. */
void UAERepresentativePlantManagerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UAEAdaptiveEnvWorldSubsystem* Subsystem = World->GetSubsystem<UAEAdaptiveEnvWorldSubsystem>())
		{
			Subsystem->UnregisterRepresentativePlantManager(this);
		}
	}
	for (AActor* PlantActor : ActiveRepresentativePlants)
	{
		if (IsValid(PlantActor))
		{
			PlantActor->Destroy();
		}
	}
	ActiveRepresentativePlants.Reset();
	ActiveStablePointIds.Reset();
	Super::EndPlay(EndPlayReason);
}

/* Report only live activated representatives. */
int32 UAERepresentativePlantManagerComponent::GetActiveRepresentativePlantCount() const
{
	return ActiveRepresentativePlants.Num();
}

/* Select and activate a bounded nearest-first M7 batch around player zero. */
void UAERepresentativePlantManagerComponent::AdvanceNeighborhoodActivation(
	const UAEAdaptiveEnvWorldSubsystem& Subsystem,
	const float StepSeconds)
{
	if (RepresentativePlantActorClass == nullptr || MaxActivePlants <= ActiveRepresentativePlants.Num())
	{
		return;
	}
	SelectionAccumulatorSeconds += FMath::Max(static_cast<double>(StepSeconds), 0.0);
	const double SafeInterval = FMath::Max(static_cast<double>(SelectionIntervalSeconds), 0.01);
	if (SelectionAccumulatorSeconds + UE_DOUBLE_SMALL_NUMBER < SafeInterval)
	{
		return;
	}
	SelectionAccumulatorSeconds = FMath::Fmod(SelectionAccumulatorSeconds, SafeInterval);

	const UWorld* World = GetWorld();
	const APlayerController* PlayerController = World != nullptr ? World->GetFirstPlayerController() : nullptr;
	const APawn* PlayerPawn = PlayerController != nullptr ? PlayerController->GetPawn() : nullptr;
	if (PlayerPawn == nullptr)
	{
		return;
	}

	TArray<FAEPlantInstanceSnapshot> Snapshots;
	Subsystem.GetM7PlantInstanceStates(Snapshots);
	const int32 RemainingCapacity = FMath::Max(MaxActivePlants - ActiveRepresentativePlants.Num(), 0);
	const int32 PassBudget = FMath::Min(FMath::Max(MaxActivationsPerPass, 1), RemainingCapacity);
	TArray<FAEPlantInstanceSnapshot> SelectedSnapshots;
	FAEM8NeighborhoodSelector::SelectNearest(
		Snapshots,
		PlayerPawn->GetActorLocation(),
		FMath::Max(ActivationRadiusCm, 0.0f),
		PassBudget,
		ActiveStablePointIds,
		SelectedSnapshots);
	for (const FAEPlantInstanceSnapshot& Snapshot : SelectedSnapshots)
	{
		ActivateSnapshot(Snapshot);
	}
}

/* Create one Actor at the exact M7 snapshot position and bind its stable identity internally. */
bool UAERepresentativePlantManagerComponent::ActivateSnapshot(const FAEPlantInstanceSnapshot& Snapshot)
{
	UWorld* World = GetWorld();
	if (World == nullptr || RepresentativePlantActorClass == nullptr || ActiveStablePointIds.Contains(Snapshot.StablePointId))
	{
		return false;
	}
	const FTransform SpawnTransform(FRotator::ZeroRotator, Snapshot.WorldLocation);
	AActor* PlantActor = World->SpawnActorDeferred<AActor>(
		RepresentativePlantActorClass,
		SpawnTransform,
		GetOwner(),
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	UAELSystemPlantComponent* PlantComponent = PlantActor != nullptr
		? PlantActor->FindComponentByClass<UAELSystemPlantComponent>()
		: nullptr;
	FString Error;
	if (PlantComponent == nullptr || !PlantComponent->BindToM7PlantSnapshot(Snapshot, false, Error))
	{
		UE_LOG(
			LogAdaptiveEnv,
			Warning,
			TEXT("M8 neighborhood activation failed. StablePointId=%lld Class=%s Error=%s"),
			Snapshot.StablePointId,
			*GetNameSafe(RepresentativePlantActorClass),
			PlantComponent == nullptr ? TEXT("Actor class has no L-System plant component.") : *Error);
		if (PlantActor != nullptr)
		{
			PlantActor->FinishSpawning(SpawnTransform);
			PlantActor->Destroy();
		}
		return false;
	}
	PlantActor->FinishSpawning(SpawnTransform);
	if (!PlantComponent->IsPlantGenerated() && !PlantComponent->GeneratePreview(Error))
	{
		UE_LOG(
			LogAdaptiveEnv,
			Warning,
			TEXT("M8 neighborhood generation failed. StablePointId=%lld Class=%s Error=%s"),
			Snapshot.StablePointId,
			*GetNameSafe(RepresentativePlantActorClass),
			*Error);
		PlantActor->Destroy();
		return false;
	}
	ActiveRepresentativePlants.Add(PlantActor);
	ActiveStablePointIds.Add(Snapshot.StablePointId);
	return true;
}
