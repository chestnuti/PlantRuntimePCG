#include "AERepresentativePlantManagerComponent.h"

#include "AdaptiveEnvLog.h"
#include "AdaptiveEnvWorldSubsystem.h"
#include "AELSystemPlantComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

/* Create a manager whose neighborhood and pool transitions use the World fixed step. */
UAERepresentativePlantManagerComponent::UAERepresentativePlantManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

/* Register after the owner enters one playable World. */
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

/* Restore M7 representation before this manager leaves the World. */
void UAERepresentativePlantManagerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UAEAdaptiveEnvWorldSubsystem* Subsystem = World->GetSubsystem<UAEAdaptiveEnvWorldSubsystem>())
		{
			for (TPair<int64, FManagedPlantEntry>& Pair : ManagedPlants)
			{
				Subsystem->SetM7RepresentativeOverride(Pair.Key, false);
				if (UAELSystemPlantComponent* Plant = Pair.Value.PlantComponent.Get())
				{
					Subsystem->UnregisterLSystemPlant(Plant);
				}
				if (AActor* Actor = Pair.Value.Actor.Get())
				{
					if (!Subsystem->ReturnM8PooledActor(Actor))
					{
						// Manager teardown cannot leave an unowned debris-wait actor in a live World.
						Actor->Destroy();
					}
				}
			}
			Subsystem->UnregisterRepresentativePlantManager(this);
		}
	}
	ManagedPlants.Reset();
	Super::EndPlay(EndPlayReason);
}

/* Count only high-detail plants currently representing M7 instances. */
int32 UAERepresentativePlantManagerComponent::GetActiveRepresentativePlantCount() const
{
	int32 Count = 0;
	for (const TPair<int64, FManagedPlantEntry>& Pair : ManagedPlants)
	{
		Count += Pair.Value.State == EAEM8PoolEntryState::Active ? 1 : 0;
	}
	return Count;
}

/* Count retained actors that cannot return until detached branches expire. */
int32 UAERepresentativePlantManagerComponent::GetWaitingForDebrisCount() const
{
	int32 Count = 0;
	for (const TPair<int64, FManagedPlantEntry>& Pair : ManagedPlants)
	{
		Count += Pair.Value.State == EAEM8PoolEntryState::WaitingForDebris ? 1 : 0;
	}
	return Count;
}

/* Advance debris expiry, release hysteresis, and bounded nearest activation. */
void UAERepresentativePlantManagerComponent::AdvanceNeighborhoodActivation(
	UAEAdaptiveEnvWorldSubsystem& Subsystem,
	const float StepSeconds)
{
	UWorld* World = GetWorld();
	const APlayerController* PlayerController = World != nullptr ? World->GetFirstPlayerController() : nullptr;
	const APawn* PlayerPawn = PlayerController != nullptr ? PlayerController->GetPawn() : nullptr;
	if (PlayerPawn == nullptr)
	{
		return;
	}
	const FVector PlayerLocation = PlayerPawn->GetActorLocation();
	const double CurrentTimeSeconds = Subsystem.GetBehaviourTimeSeconds();

	// Submit deterministic class prewarm requests once after Blueprint configuration is available.
	if (!bPoolPrewarmed)
	{
		TSet<TObjectKey<UClass>> SubmittedClasses;
		for (const FAEM8RepresentativePlantBinding& Binding : PlantBindings)
		{
			if (Binding.bEnabled && Binding.RepresentativePlantActorClass != nullptr
				&& !SubmittedClasses.Contains(TObjectKey<UClass>(Binding.RepresentativePlantActorClass.Get())))
			{
				Subsystem.EnsureM8PoolPrewarmed(
					Binding.RepresentativePlantActorClass,
					Binding.PrewarmCount,
					Binding.PoolCapacity,
					GetOwner());
				SubmittedClasses.Add(TObjectKey<UClass>(Binding.RepresentativePlantActorClass.Get()));
			}
		}
		bPoolPrewarmed = true;
	}

	TArray<FAEPlantInstanceSnapshot> Snapshots;
	Subsystem.GetM7PlantInstanceStates(Snapshots);
	TMap<int64, const FAEPlantInstanceSnapshot*> SnapshotById;
	for (const FAEPlantInstanceSnapshot& Snapshot : Snapshots)
	{
		SnapshotById.Add(Snapshot.StablePointId, &Snapshot);
	}

	// Expire detached geometry every fixed step, independent from the slower selection interval.
	int32 CleanupBudget = FMath::Max(MaxDebrisCleanupsPerStep, 1);
	TArray<int64> ManagedIds;
	ManagedPlants.GetKeys(ManagedIds);
	ManagedIds.Sort();
	for (const int64 StablePointId : ManagedIds)
	{
		FManagedPlantEntry* Entry = ManagedPlants.Find(StablePointId);
		if (Entry != nullptr && Entry->PlantComponent.IsValid() && CleanupBudget > 0)
		{
			CleanupBudget -= Entry->PlantComponent->ExpireDetachedBranches(CurrentTimeSeconds, CleanupBudget);
		}
	}

	SelectionAccumulatorSeconds += FMath::Max(static_cast<double>(StepSeconds), 0.0);
	const double SafeInterval = FMath::Max(static_cast<double>(SelectionIntervalSeconds), 0.01);
	if (SelectionAccumulatorSeconds + UE_DOUBLE_SMALL_NUMBER < SafeInterval)
	{
		return;
	}
	SelectionAccumulatorSeconds = FMath::Fmod(SelectionAccumulatorSeconds, SafeInterval);

	// Resolve waiting actors before new releases so re-entry always reuses the original shell.
	TArray<int64> RemoveIds;
	for (const int64 StablePointId : ManagedIds)
	{
		FManagedPlantEntry* Entry = ManagedPlants.Find(StablePointId);
		if (Entry == nullptr || Entry->State != EAEM8PoolEntryState::WaitingForDebris || !Entry->PlantComponent.IsValid())
		{
			continue;
		}
		const FAEPlantInstanceSnapshot* const* SnapshotPtr = SnapshotById.Find(StablePointId);
		const bool bInsideActivation = SnapshotPtr != nullptr && *SnapshotPtr != nullptr && (*SnapshotPtr)->bVisible && FVector::DistSquared(PlayerLocation, (*SnapshotPtr)->WorldLocation) <= FMath::Square(static_cast<double>(FMath::Max(ActivationRadiusCm, 0.0f)));
		if (bInsideActivation)
		{
			Entry->PlantComponent->CancelDebrisReleaseWait();
			Subsystem.RegisterLSystemPlant(Entry->PlantComponent.Get());
			Subsystem.SetM7RepresentativeOverride(StablePointId, true);
			Entry->State = EAEM8PoolEntryState::Active;
			Entry->ActivationTimeSeconds = CurrentTimeSeconds;
		}
		else if (!Entry->PlantComponent->HasLiveDetachedBranches())
		{
			Entry->State = EAEM8PoolEntryState::Returning;
			if (Subsystem.ReturnM8PooledActor(Entry->Actor.Get()))
			{
				RemoveIds.Add(StablePointId);
			}
		}
	}
	for (const int64 StablePointId : RemoveIds)
	{
		ManagedPlants.Remove(StablePointId);
	}

	// Request bounded release for active actors beyond the outer hysteresis radius.
	int32 ReleaseBudget = FMath::Max(MaxReleaseRequestsPerPass, 1);
	ManagedIds.Reset();
	ManagedPlants.GetKeys(ManagedIds);
	ManagedIds.Sort();
	for (const int64 StablePointId : ManagedIds)
	{
		FManagedPlantEntry* Entry = ManagedPlants.Find(StablePointId);
		if (Entry == nullptr || Entry->State != EAEM8PoolEntryState::Active || ReleaseBudget <= 0)
		{
			continue;
		}
		const FAEPlantInstanceSnapshot* const* SnapshotPtr = SnapshotById.Find(StablePointId);
		const bool bOutsideRelease = SnapshotPtr == nullptr || *SnapshotPtr == nullptr || !(*SnapshotPtr)->bVisible || FVector::DistSquared(PlayerLocation, (*SnapshotPtr)->WorldLocation) > FMath::Square(static_cast<double>(FMath::Max(DeactivationRadiusCm, ActivationRadiusCm)));
		if (bOutsideRelease && CurrentTimeSeconds - Entry->ActivationTimeSeconds >= FMath::Max(static_cast<double>(MinimumActiveSeconds), 0.0))
		{
			RequestRelease(Subsystem, StablePointId, *Entry);
			--ReleaseBudget;
			if (Entry->State == EAEM8PoolEntryState::Available)
			{
				ManagedPlants.Remove(StablePointId);
			}
		}
	}

	// Build nearest deterministic candidates that have one enabled species binding.
	struct FCandidate
	{
		const FAEPlantInstanceSnapshot* Snapshot = nullptr;
		const FAEM8RepresentativePlantBinding* Binding = nullptr;
		double DistanceSquared = 0.0;
	};
	TArray<FCandidate> Candidates;
	const double ActivationRadiusSquared = FMath::Square(static_cast<double>(FMath::Max(ActivationRadiusCm, 0.0f)));
	for (const FAEPlantInstanceSnapshot& Snapshot : Snapshots)
	{
		const FAEM8RepresentativePlantBinding* Binding = FindBinding(Snapshot.SpeciesId);
		if (!Snapshot.bVisible || Snapshot.StablePointId <= 0 || ManagedPlants.Contains(Snapshot.StablePointId)
			|| Binding == nullptr || Binding->MaxActivePlants <= CountActiveSpecies(Snapshot.SpeciesId))
		{
			continue;
		}
		const double DistanceSquared = FVector::DistSquared(PlayerLocation, Snapshot.WorldLocation);
		if (DistanceSquared <= ActivationRadiusSquared)
		{
			Candidates.Add({&Snapshot, Binding, DistanceSquared});
		}
	}
	Candidates.Sort([](const FCandidate& A, const FCandidate& B)
	{
		if (!FMath::IsNearlyEqual(A.DistanceSquared, B.DistanceSquared)) return A.DistanceSquared < B.DistanceSquared;
		return A.Snapshot->StablePointId < B.Snapshot->StablePointId;
	});

	int32 ActivationBudget = FMath::Min(
		FMath::Max(MaxActivationsPerPass, 1),
		// Waiting actors reserve their slots so a re-entering damaged plant never exceeds the cap.
		FMath::Max(MaxActivePlants - ManagedPlants.Num(), 0));
	for (const FCandidate& Candidate : Candidates)
	{
		if (ActivationBudget <= 0) break;
		if (Candidate.Binding->MaxActivePlants <= CountActiveSpecies(Candidate.Snapshot->SpeciesId)) continue;
		if (ActivateSnapshot(Subsystem, *Candidate.Snapshot, *Candidate.Binding)) --ActivationBudget;
	}
}

/* Acquire, bind, restore persistent damage, and publish one M8 representation. */
bool UAERepresentativePlantManagerComponent::ActivateSnapshot(
	UAEAdaptiveEnvWorldSubsystem& Subsystem,
	const FAEPlantInstanceSnapshot& Snapshot,
	const FAEM8RepresentativePlantBinding& Binding)
{
	FString Error;
	AActor* Actor = Subsystem.AcquireM8PooledActor(
		Binding.RepresentativePlantActorClass,
		Binding.PoolCapacity,
		GetOwner(),
		Error);
	if (Actor == nullptr)
	{
		UE_LOG(LogAdaptiveEnv, Verbose, TEXT("M8 activation deferred. StablePointId=%lld Species=%s Error=%s"), Snapshot.StablePointId, *Snapshot.SpeciesId.ToString(), *Error);
		return false;
	}
	UAELSystemPlantComponent* Plant = Actor->FindComponentByClass<UAELSystemPlantComponent>();
	if (Plant == nullptr)
	{
		UE_LOG(LogAdaptiveEnv, Warning, TEXT("M8 pooled actor lost its plant component. Class=%s"), *GetNameSafe(Actor->GetClass()));
		Actor->Destroy();
		return false;
	}

	Actor->SetActorTransform(FTransform(FRotator::ZeroRotator, Snapshot.WorldLocation));
	Actor->SetActorHiddenInGame(false);
	Actor->SetActorEnableCollision(false);
	if (!Plant->BindToM7PlantSnapshot(Snapshot, true, Error))
	{
		UE_LOG(LogAdaptiveEnv, Warning, TEXT("M8 pooled activation failed. StablePointId=%lld Class=%s Error=%s"), Snapshot.StablePointId, *GetNameSafe(Actor->GetClass()), *Error);
		Subsystem.ReturnM8PooledActor(Actor);
		return false;
	}
	FAEM8PersistentPlantState PersistentState;
	if (Subsystem.GetM8PersistentPlantState(Snapshot.StablePointId, PersistentState))
	{
		FString RestoreError;
		if (!Plant->ApplyPersistentStructuralState(PersistentState, RestoreError))
		{
			UE_LOG(LogAdaptiveEnv, Warning, TEXT("M8 persistent state skipped. StablePointId=%lld Error=%s"), Snapshot.StablePointId, *RestoreError);
		}
	}
	Actor->SetActorEnableCollision(true);
	Subsystem.RegisterLSystemPlant(Plant);
	Subsystem.SetM7RepresentativeOverride(Snapshot.StablePointId, true);

	FManagedPlantEntry& Entry = ManagedPlants.Add(Snapshot.StablePointId);
	Entry.Actor = Actor;
	Entry.PlantComponent = Plant;
	Entry.SpeciesId = Snapshot.SpeciesId;
	Entry.ActorClass = Binding.RepresentativePlantActorClass;
	Entry.State = EAEM8PoolEntryState::Active;
	Entry.ActivationTimeSeconds = Subsystem.GetBehaviourTimeSeconds();
	return true;
}

/* Restore M7 immediately, then delay pool return only while detached geometry survives. */
void UAERepresentativePlantManagerComponent::RequestRelease(
	UAEAdaptiveEnvWorldSubsystem& Subsystem,
	const int64 StablePointId,
	FManagedPlantEntry& Entry)
{
	Subsystem.SetM7RepresentativeOverride(StablePointId, false);
	if (!Entry.PlantComponent.IsValid())
	{
		Entry.State = EAEM8PoolEntryState::Available;
		return;
	}
	Subsystem.UnregisterLSystemPlant(Entry.PlantComponent.Get());
	if (Entry.PlantComponent->HasLiveDetachedBranches())
	{
		Entry.PlantComponent->EnterDebrisReleaseWait();
		Entry.State = EAEM8PoolEntryState::WaitingForDebris;
		return;
	}
	Entry.State = EAEM8PoolEntryState::Returning;
	if (Subsystem.ReturnM8PooledActor(Entry.Actor.Get()))
	{
		Entry.State = EAEM8PoolEntryState::Available;
	}
}

/* Find one unambiguous enabled species binding in authored array order. */
const FAEM8RepresentativePlantBinding* UAERepresentativePlantManagerComponent::FindBinding(const FName SpeciesId) const
{
	for (const FAEM8RepresentativePlantBinding& Binding : PlantBindings)
	{
		if (Binding.bEnabled && Binding.M7SpeciesId == SpeciesId && Binding.RepresentativePlantActorClass != nullptr)
		{
			return &Binding;
		}
	}
	return nullptr;
}

/* Count active high-detail representatives for one species quota. */
int32 UAERepresentativePlantManagerComponent::CountActiveSpecies(const FName SpeciesId) const
{
	int32 Count = 0;
	for (const TPair<int64, FManagedPlantEntry>& Pair : ManagedPlants)
	{
		Count += Pair.Value.State == EAEM8PoolEntryState::Active && Pair.Value.SpeciesId == SpeciesId ? 1 : 0;
	}
	return Count;
}
