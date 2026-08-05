#include "AdaptiveEnvWorldSubsystem.h"

#include "AEBehaviourTrackerComponent.h"
#include "AEHeatmapRendererComponent.h"
#include "AEPathHeatmapRendererComponent.h"
#include "AERepresentativePlantManagerComponent.h"
#include "AEVegetationDistributionComponent.h"
#include "AELSystemPlantComponent.h"
#include "AEM8Types.h"
#include "AEMoistureSourceComponent.h"
#include "AEWorldConstraintProvider.h"
#include "AEAdaptiveEnvironmentProfile.h"
#include "AEM2ConfigService.h"
#include "AdaptiveEnvGameplayTags.h"
#include "AdaptiveEnvLog.h"
#include "AdaptiveEnvSettings.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

// Initialize one ordered runtime pipeline for the current World.
void UAEAdaptiveEnvWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Load scheduler controls and establish a unique World instance identity.
	InstanceId = FGuid::NewGuid();
	TickCount = 0;
	M8UpdateCursor = 0;
	const UAdaptiveEnvSettings* Settings = GetDefault<UAdaptiveEnvSettings>();
	bRuntimeEnabled = Settings->bEnableRuntime;
	BehaviourStepSeconds = 1.0f / FMath::Max(Settings->BehaviourSampleRateHz, 1.0f);
	MaxBehaviourSubstepsPerFrame = FMath::Max(Settings->MaxBehaviourSubstepsPerFrame, 1);
	SimulationHoursPerRealSecond = FMath::Max(static_cast<double>(Settings->SimulationHoursPerRealSecond), 0.0);

	// Translate project settings into the grid initialization contract.
	FAEHeatmapGridConfig GridConfig;
	GridConfig.WorldCenter = Settings->GridWorldCenter;
	GridConfig.Dimensions = FIntPoint(Settings->GridWidth, Settings->GridHeight);
	GridConfig.CellSizeCm = Settings->CellSizeCm;
	GridConfig.KernelRadiusCells = Settings->ActivityKernelRadiusCells;
	GridConfig.KernelSigma = Settings->ActivityKernelSigma;
	// Disable runtime updates when the grid configuration cannot be allocated.
	if (!BehaviourGrid.Initialize(GridConfig))
	{
		bRuntimeEnabled = false;
		UE_LOG(LogAdaptiveEnv, Error, TEXT("Behaviour grid initialization failed."));
	}
	if (!ExposureGrid.Initialize(GridConfig))
	{
		bRuntimeEnabled = false;
		UE_LOG(LogAdaptiveEnv, Error, TEXT("M3 grid initialization failed."));
	}
	if (!ConstraintGrid.Initialize(GridConfig) || !ResponseGrid.Initialize(GridConfig))
	{
		bRuntimeEnabled = false;
		UE_LOG(LogAdaptiveEnv, Error, TEXT("M4/M5 grid initialization failed."));
	}
	if (!PathHeatmapGrid.Initialize(GridConfig))
	{
		bRuntimeEnabled = false;
		UE_LOG(LogAdaptiveEnv, Error, TEXT("M6 grid initialization failed."));
	}

	// Freeze user-configurable M6 values into one validated World parameter snapshot.
	M6Parameters.VisibleDamageThresholdRatio = Settings->M6VisibleDamageThresholdRatio;
	M6Parameters.FullPathDamageThresholdRatio = Settings->M6FullPathDamageThresholdRatio;
	M6Parameters.FormationRatePerSimulationHour = Settings->M6FormationRatePerSimulationHour;
	M6Parameters.FadeRatePerSimulationHour = Settings->M6FadeRatePerSimulationHour;
	M6Parameters.DirtyIntensityEpsilon = Settings->M6DirtyIntensityEpsilon;

	// Load one atomic M3/M4/M5 profile without disabling the valid M1 pipeline when absent.
	bM3Enabled = false;
	bM4Enabled = false;
	bM5Enabled = false;
	bM6Enabled = false;
	bM7Enabled = false;
	bM8Enabled = bRuntimeEnabled && Settings->bEnableM8;
	if (bRuntimeEnabled && Settings->bEnableAdaptiveEcology)
	{
		UAEAdaptiveEnvironmentProfile* Profile = Settings->EnvironmentProfile.LoadSynchronous();
		if (Profile != nullptr)
		{
			FString Error;
			if (!ApplyEnvironmentProfile(Profile, Error))
			{
				UE_LOG(LogAdaptiveEnv, Error, TEXT("Environment profile initialization failed. World=%s Error=%s"), *GetNameSafe(GetWorld()), *Error);
			}
		}
		else
		{
			UE_LOG(LogAdaptiveEnv, Error, TEXT("Adaptive ecology disabled because no environment profile is configured. World=%s"), *GetNameSafe(GetWorld()));
		}
	}

	UE_LOG(
		LogAdaptiveEnv,
		Log,
		TEXT("World subsystem initialized. World=%s Instance=%s"),
		*GetNameSafe(GetWorld()),
		*InstanceId.ToString(EGuidFormats::DigitsWithHyphens));
}

// Release all World-owned runtime state in a deterministic order.
void UAEAdaptiveEnvWorldSubsystem::Deinitialize()
{
	// Record final lifecycle diagnostics before state is cleared.
	UE_LOG(
		LogAdaptiveEnv,
		Log,
		TEXT("World subsystem deinitialized. World=%s Instance=%s Ticks=%lld"),
		*GetNameSafe(GetWorld()),
		*InstanceId.ToString(EGuidFormats::DigitsWithHyphens),
		TickCount);

	// Stop ticking and clear registrations, queues, order guards, and grid data.
	bRuntimeEnabled = false;
	RegisteredTrackers.Reset();
	PendingTrackerAdds.Reset();
	PendingTrackerRemoves.Reset();
	RegisteredRenderers.Reset();
	PendingRendererAdds.Reset();
	PendingRendererRemoves.Reset();
	for (const TWeakObjectPtr<UAEPathHeatmapRendererComponent>& Renderer : RegisteredPathHeatmapRenderers)
	{
		if (Renderer.IsValid())
		{
			Renderer->ResetVisualOutput();
		}
	}
	RegisteredPathHeatmapRenderers.Reset();
	PendingPathHeatmapRendererAdds.Reset();
	PendingPathHeatmapRendererRemoves.Reset();
	PendingM6VisualCommands.Reset();
	RegisteredVegetationDistributions.Reset();
	PendingVegetationDistributionAdds.Reset();
	PendingVegetationDistributionRemoves.Reset();
	RegisteredLSystemPlants.Reset();
	PendingLSystemPlantAdds.Reset();
	PendingLSystemPlantRemoves.Reset();
	RegisteredRepresentativePlantManagers.Reset();
	PendingRepresentativePlantManagerAdds.Reset();
	PendingRepresentativePlantManagerRemoves.Reset();
	for (AActor* PoolActor : M8ManagedPoolActors)
	{
		if (IsValid(PoolActor))
		{
			PoolActor->Destroy();
		}
	}
	M8ManagedPoolActors.Reset();
	M8AvailablePoolActorsByClass.Reset();
	M8PersistentPlantStates.Reset();
	M8UpdateCursor = 0;
	RegisteredMoistureSources.Reset();
	ActiveMoistureTexture = nullptr;
	PendingMoistureSourceAdds.Reset();
	PendingMoistureSourceRemoves.Reset();
	PendingSamples.Reset();
	ProcessingSamples.Reset();
	PendingDebugActiveCellIndices.Reset();
	LastQueuedSequenceByAgent.Reset();
	LastQueuedTimestampByAgent.Reset();
	BehaviourGrid.Reset();
	ExposureGrid.Reset();
	ConstraintGrid.Reset();
	ResponseGrid.Reset();
	PathHeatmapGrid.Reset();
	PendingM7BaselineCellIndices.Reset();
	bM3Enabled = false;
	bM4Enabled = false;
	bM5Enabled = false;
	bM6Enabled = false;
	bM7Enabled = false;
	bM8Enabled = false;
	Super::Deinitialize();
}

// Advance registration, fixed-step sampling, aggregation, and debug output.
void UAEAdaptiveEnvWorldSubsystem::Tick(float DeltaTime)
{
	// Apply deferred registrations before any service iterates active arrays.
	++TickCount;
	ApplyPendingRegistrations();

	// Convert render time into a bounded number of fixed behaviour steps.
	const double SafeDeltaTime = FMath::Max(static_cast<double>(DeltaTime), 0.0);
	BehaviourAccumulator += SafeDeltaTime;
	const int32 AvailableSteps = FMath::FloorToInt((BehaviourAccumulator + UE_DOUBLE_SMALL_NUMBER) / BehaviourStepSeconds);
	const int32 StepsToRun = FMath::Min(AvailableSteps, MaxBehaviourSubstepsPerFrame);
	// Drop excess whole steps after an overrun while preserving fractional time.
	if (AvailableSteps > MaxBehaviourSubstepsPerFrame)
	{
		++SchedulerOverrunCount;
		BehaviourAccumulator = FMath::Fmod(BehaviourAccumulator, static_cast<double>(BehaviourStepSeconds));
	}
	else
	{
		BehaviourAccumulator -= static_cast<double>(StepsToRun) * BehaviourStepSeconds;
	}

	// Sample and aggregate each fixed step before visual consumers run.
	for (int32 StepIndex = 0; StepIndex < StepsToRun; ++StepIndex)
	{
		BehaviourTimeSeconds += BehaviourStepSeconds;
		SampleRegisteredTrackers(BehaviourStepSeconds);
		ProcessPendingSamples();
		UpdateM3(BehaviourStepSeconds);
		UpdateM4(BehaviourStepSeconds);
		UpdateM5(BehaviourStepSeconds);
		UpdateM6(BehaviourStepSeconds);
		UpdateM7(BehaviourStepSeconds);
		UpdateM8NeighborhoodActivation(BehaviourStepSeconds);
		UpdateM8();
		AccumulateDebugActiveCells();
		++ProcessedBehaviourStepCount;
	}

	// Apply final visual state after all fixed substeps, then draw debug output.
	UpdateM6VisualRenderers(DeltaTime);
	UpdateM7VisualAdapters();
	UpdateDebugRenderers(DeltaTime);
	BehaviourGrid.ClearDirtyCells();
}

// Tick only after successful initialization when runtime is enabled.
bool UAEAdaptiveEnvWorldSubsystem::IsTickable() const
{
	return bRuntimeEnabled && IsInitialized();
}

// Register this tickable object with Unreal performance statistics.
TStatId UAEAdaptiveEnvWorldSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UAEAdaptiveEnvWorldSubsystem, STATGROUP_Tickables);
}

// Create the subsystem only for playable World types.
bool UAEAdaptiveEnvWorldSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game
		|| WorldType == EWorldType::PIE
		|| WorldType == EWorldType::GamePreview;
}

// Validate ordering and append one sample to the producer queue.
EAEBehaviourSubmitResult UAEAdaptiveEnvWorldSubsystem::SubmitBehaviourSample(const FAEBehaviourSample& Sample)
{
	// All UObject access and queue mutation remain on the Game Thread.
	check(IsInGameThread());

	// Reject malformed samples before touching per-agent ordering state.
	EAEBehaviourSubmitResult Result = EAEBehaviourSubmitResult::Accepted;
	if (!ValidateQueuedSample(Sample, Result))
	{
		BehaviourGrid.RecordRejectedSample(Result);
		return Result;
	}

	// Reject duplicate or decreasing sequence numbers within the pending batch.
	if (const int64* LastSequence = LastQueuedSequenceByAgent.Find(Sample.AgentId))
	{
		if (Sample.SequenceNumber == *LastSequence)
		{
			BehaviourGrid.RecordRejectedSample(EAEBehaviourSubmitResult::DuplicateSequence);
			return EAEBehaviourSubmitResult::DuplicateSequence;
		}
		if (Sample.SequenceNumber < *LastSequence)
		{
			BehaviourGrid.RecordRejectedSample(EAEBehaviourSubmitResult::OutOfOrder);
			return EAEBehaviourSubmitResult::OutOfOrder;
		}
	}
	// Reject timestamps older than the latest queued sample for this agent.
	if (const double* LastTimestamp = LastQueuedTimestampByAgent.Find(Sample.AgentId))
	{
		if (Sample.Timestamp < *LastTimestamp)
		{
			BehaviourGrid.RecordRejectedSample(EAEBehaviourSubmitResult::OutOfOrder);
			return EAEBehaviourSubmitResult::OutOfOrder;
		}
	}

	// Commit ordering guards and queue the validated sample.
	LastQueuedSequenceByAgent.Add(Sample.AgentId, Sample.SequenceNumber);
	LastQueuedTimestampByAgent.Add(Sample.AgentId, Sample.Timestamp);
	PendingSamples.Add(Sample);
	return EAEBehaviourSubmitResult::Accepted;
}

// Defer tracker registration until the next safe tick boundary.
void UAEAdaptiveEnvWorldSubsystem::RegisterBehaviourTracker(UAEBehaviourTrackerComponent* Tracker)
{
	if (IsValid(Tracker))
	{
		PendingTrackerAdds.AddUnique(Tracker);
	}
}

// Defer tracker removal until the next safe tick boundary.
void UAEAdaptiveEnvWorldSubsystem::UnregisterBehaviourTracker(UAEBehaviourTrackerComponent* Tracker)
{
	if (Tracker != nullptr)
	{
		PendingTrackerRemoves.AddUnique(Tracker);
	}
}

// Defer renderer registration until the next safe tick boundary.
void UAEAdaptiveEnvWorldSubsystem::RegisterHeatmapRenderer(UAEHeatmapRendererComponent* Renderer)
{
	if (IsValid(Renderer))
	{
		PendingRendererAdds.AddUnique(Renderer);
	}
}

// Defer renderer removal until the next safe tick boundary.
void UAEAdaptiveEnvWorldSubsystem::UnregisterHeatmapRenderer(UAEHeatmapRendererComponent* Renderer)
{
	if (Renderer != nullptr)
	{
		PendingRendererRemoves.AddUnique(Renderer);
	}
}

/* Defer moisture-source registration until the next safe pipeline boundary. */
void UAEAdaptiveEnvWorldSubsystem::RegisterMoistureSource(UAEMoistureSourceComponent* Source)
{
	if (IsValid(Source)) PendingMoistureSourceAdds.AddUnique(Source);
}

/* Defer moisture-source removal until the next safe pipeline boundary. */
void UAEAdaptiveEnvWorldSubsystem::UnregisterMoistureSource(UAEMoistureSourceComponent* Source)
{
	if (Source != nullptr) PendingMoistureSourceRemoves.AddUnique(Source);
}

/* Defer M6 renderer registration until the next safe pipeline boundary. */
void UAEAdaptiveEnvWorldSubsystem::RegisterPathHeatmapRenderer(
	UAEPathHeatmapRendererComponent* Renderer)
{
	if (IsValid(Renderer))
	{
		PendingPathHeatmapRendererAdds.AddUnique(Renderer);
	}
}

/* Defer M6 renderer removal until the next safe pipeline boundary. */
void UAEAdaptiveEnvWorldSubsystem::UnregisterPathHeatmapRenderer(
	UAEPathHeatmapRendererComponent* Renderer)
{
	if (Renderer != nullptr)
	{
		PendingPathHeatmapRendererRemoves.AddUnique(Renderer);
	}
}

/* Defer one M7 distribution registration until the next safe pipeline boundary. */
void UAEAdaptiveEnvWorldSubsystem::RegisterVegetationDistribution(
	UAEVegetationDistributionComponent* Distribution)
{
	if (IsValid(Distribution))
	{
		PendingVegetationDistributionAdds.AddUnique(Distribution);
	}
}

/* Defer one M7 distribution removal until the next safe pipeline boundary. */
void UAEAdaptiveEnvWorldSubsystem::UnregisterVegetationDistribution(
	UAEVegetationDistributionComponent* Distribution)
{
	if (Distribution != nullptr)
	{
		PendingVegetationDistributionRemoves.AddUnique(Distribution);
	}
}

/* Queue one M8 representative plant for registration at the next safe boundary. */
void UAEAdaptiveEnvWorldSubsystem::RegisterLSystemPlant(UAELSystemPlantComponent* Plant)
{
	if (IsValid(Plant))
	{
		PendingLSystemPlantAdds.AddUnique(Plant);
		PendingLSystemPlantRemoves.Remove(Plant);
	}
}

/* Queue one M8 representative plant for removal at the next safe boundary. */
void UAEAdaptiveEnvWorldSubsystem::UnregisterLSystemPlant(UAELSystemPlantComponent* Plant)
{
	if (Plant != nullptr)
	{
		PendingLSystemPlantRemoves.AddUnique(Plant);
		PendingLSystemPlantAdds.Remove(Plant);
	}
}

/* Queue one M8 neighborhood manager for registration at the next safe boundary. */
void UAEAdaptiveEnvWorldSubsystem::RegisterRepresentativePlantManager(
	UAERepresentativePlantManagerComponent* Manager)
{
	if (IsValid(Manager))
	{
		PendingRepresentativePlantManagerAdds.AddUnique(Manager);
		PendingRepresentativePlantManagerRemoves.Remove(Manager);
	}
}

/* Queue one M8 neighborhood manager for removal at the next safe boundary. */
void UAEAdaptiveEnvWorldSubsystem::UnregisterRepresentativePlantManager(
	UAERepresentativePlantManagerComponent* Manager)
{
	if (Manager != nullptr)
	{
		PendingRepresentativePlantManagerRemoves.AddUnique(Manager);
		PendingRepresentativePlantManagerAdds.Remove(Manager);
	}
}

/* Create clean available shells until one class reaches its requested prewarm count. */
void UAEAdaptiveEnvWorldSubsystem::EnsureM8PoolPrewarmed(
	const TSubclassOf<AActor> ActorClass,
	const int32 PrewarmCount,
	const int32 PoolCapacity,
	AActor* Owner)
{
	if (ActorClass == nullptr || PrewarmCount <= 0 || PoolCapacity <= 0)
	{
		return;
	}
	const int32 TargetCount = FMath::Min(PrewarmCount, PoolCapacity);
	int32 ResidentCount = 0;
	for (AActor* Actor : M8ManagedPoolActors)
	{
		ResidentCount += IsValid(Actor) && Actor->GetClass() == ActorClass.Get() ? 1 : 0;
	}
	while (ResidentCount < TargetCount)
	{
		FString Error;
		AActor* Actor = SpawnM8PoolActor(ActorClass, Owner, Error);
		if (Actor == nullptr)
		{
			UE_LOG(LogAdaptiveEnv, Warning, TEXT("M8 pool prewarm failed. Class=%s Error=%s"), *GetNameSafe(ActorClass), *Error);
			break;
		}
		M8AvailablePoolActorsByClass.FindOrAdd(TObjectKey<UClass>(ActorClass.Get())).Add(Actor);
		++ResidentCount;
	}
}

/* Acquire one clean actor shell without exceeding the exact-class resident cap. */
AActor* UAEAdaptiveEnvWorldSubsystem::AcquireM8PooledActor(
	const TSubclassOf<AActor> ActorClass,
	const int32 PoolCapacity,
	AActor* Owner,
	FString& OutError)
{
	OutError.Reset();
	if (ActorClass == nullptr || PoolCapacity <= 0)
	{
		OutError = TEXT("M8 pool requires an actor class and positive capacity.");
		return nullptr;
	}
	TArray<TWeakObjectPtr<AActor>>& Available = M8AvailablePoolActorsByClass.FindOrAdd(TObjectKey<UClass>(ActorClass.Get()));
	while (!Available.IsEmpty())
	{
		if (AActor* Actor = Available.Pop(EAllowShrinking::No).Get())
		{
			return Actor;
		}
	}

	int32 ResidentCount = 0;
	for (AActor* Actor : M8ManagedPoolActors)
	{
		ResidentCount += IsValid(Actor) && Actor->GetClass() == ActorClass.Get() ? 1 : 0;
	}
	if (ResidentCount >= PoolCapacity)
	{
		OutError = TEXT("M8 pool capacity is occupied by active or debris-waiting actors.");
		return nullptr;
	}
	return SpawnM8PoolActor(ActorClass, Owner, OutError);
}

/* Return one clean actor shell only after its detached branch geometry has expired. */
bool UAEAdaptiveEnvWorldSubsystem::ReturnM8PooledActor(AActor* Actor)
{
	if (!IsValid(Actor) || !M8ManagedPoolActors.Contains(Actor))
	{
		return false;
	}
	UAELSystemPlantComponent* Plant = Actor->FindComponentByClass<UAELSystemPlantComponent>();
	if (Plant == nullptr || !Plant->PrepareForPool())
	{
		return false;
	}
	UnregisterLSystemPlant(Plant);
	M8AvailablePoolActorsByClass.FindOrAdd(TObjectKey<UClass>(Actor->GetClass())).AddUnique(Actor);
	return true;
}

/* Persist one broken module before its actor-owned physics presentation changes. */
void UAEAdaptiveEnvWorldSubsystem::RecordM8BrokenBranch(
	const int64 StablePointId,
	const FName SpeciesId,
	const int64 RuleContentHash,
	const int32 GenerationSeed,
	const int64 BranchModuleId)
{
	if (StablePointId <= 0 || BranchModuleId <= 0)
	{
		return;
	}
	FAEM8PersistentPlantState& State = M8PersistentPlantStates.FindOrAdd(StablePointId);
	State.StablePointId = StablePointId;
	State.SpeciesId = SpeciesId;
	State.RuleContentHash = RuleContentHash;
	State.GenerationSeed = GenerationSeed;
	State.BrokenBranchModuleIds.AddUnique(BranchModuleId);
	State.BrokenBranchModuleIds.Sort();
}

/* Persist one irreversible dead-wood module independently from M7 recovery. */
void UAEAdaptiveEnvWorldSubsystem::RecordM8DeadWoodBranch(
	const int64 StablePointId,
	const FName SpeciesId,
	const int64 RuleContentHash,
	const int32 GenerationSeed,
	const int64 BranchModuleId)
{
	if (StablePointId <= 0 || BranchModuleId < 0)
	{
		return;
	}
	FAEM8PersistentPlantState& State = M8PersistentPlantStates.FindOrAdd(StablePointId);
	State.StablePointId = StablePointId;
	State.SpeciesId = SpeciesId;
	State.RuleContentHash = RuleContentHash;
	State.GenerationSeed = GenerationSeed;
	State.DeadWoodBranchModuleIds.AddUnique(BranchModuleId);
	State.DeadWoodBranchModuleIds.Sort();
}

/* Copy one stable plant's structural state for silent restoration. */
bool UAEAdaptiveEnvWorldSubsystem::GetM8PersistentPlantState(
	const int64 StablePointId,
	FAEM8PersistentPlantState& OutState) const
{
	if (const FAEM8PersistentPlantState* State = M8PersistentPlantStates.Find(StablePointId))
	{
		OutState = *State;
		return true;
	}
	return false;
}

/* Forward one representation override to the M7 distribution owning the stable point. */
bool UAEAdaptiveEnvWorldSubsystem::SetM7RepresentativeOverride(
	const int64 StablePointId,
	const bool bM8Active)
{
	for (const TWeakObjectPtr<UAEVegetationDistributionComponent>& Distribution : RegisteredVegetationDistributions)
	{
		if (Distribution.IsValid() && Distribution->SetM8RepresentativeOverride(StablePointId, bM8Active))
		{
			return true;
		}
	}
	return false;
}

/* Spawn one Blueprint shell, then discover Construction Script components safely. */
AActor* UAEAdaptiveEnvWorldSubsystem::SpawnM8PoolActor(
	const TSubclassOf<AActor> ActorClass,
	AActor* Owner,
	FString& OutError)
{
	OutError.Reset();
	UWorld* World = GetWorld();
	if (World == nullptr || ActorClass == nullptr)
	{
		OutError = TEXT("M8 pool spawn requires a World and actor class.");
		return nullptr;
	}
	const FTransform SpawnTransform(FRotator::ZeroRotator, FVector::ZeroVector);
	AActor* Actor = World->SpawnActorDeferred<AActor>(
		ActorClass,
		SpawnTransform,
		Owner,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Actor == nullptr)
	{
		OutError = TEXT("Deferred M8 actor spawn failed.");
		return nullptr;
	}
	Actor->FinishSpawning(SpawnTransform);
	UAELSystemPlantComponent* Plant = Actor->FindComponentByClass<UAELSystemPlantComponent>();
	if (Plant == nullptr)
	{
		OutError = TEXT("Actor class has no L-System plant component after FinishSpawning.");
		Actor->Destroy();
		return nullptr;
	}
	UnregisterLSystemPlant(Plant);
	Plant->ClearGeneratedPlant();
	Actor->SetActorEnableCollision(false);
	Actor->SetActorHiddenInGame(true);
	M8ManagedPoolActors.Add(Actor);
	return Actor;
}

/* Queues only projected candidate Cells so M4/M5 establish a real baseline before M7 display. */
void UAEAdaptiveEnvWorldSubsystem::RequestM7BaselineInitialization(
	const UAEVegetationDistributionComponent* Distribution)
{
	check(IsInGameThread());
	if (Distribution == nullptr)
	{
		return;
	}
	TArray<int32> OccupiedCellIndices;
	Distribution->GetOccupiedCellIndices(OccupiedCellIndices);
	for (const int32 CellIndex : OccupiedCellIndices)
	{
		PendingM7BaselineCellIndices.Add(CellIndex);
	}
}

// Forward a world-position cell query to the owned behaviour grid.
bool UAEAdaptiveEnvWorldSubsystem::GetBehaviourCellAtWorldLocation(const FVector& Location, FAEBehaviourCellSnapshot& OutSnapshot) const
{
	return BehaviourGrid.GetCellSnapshotAtWorldLocation(Location, OutSnapshot);
}

// Forward a coordinate cell query to the owned behaviour grid.
bool UAEAdaptiveEnvWorldSubsystem::GetBehaviourCell(const FIntPoint& Coordinate, FAEBehaviourCellSnapshot& OutSnapshot) const
{
	return BehaviourGrid.GetCellSnapshot(Coordinate, OutSnapshot);
}

// Return configured grid dimensions in cells.
FIntPoint UAEAdaptiveEnvWorldSubsystem::GetGridDimensions() const
{
	return BehaviourGrid.GetConfig().Dimensions;
}

// Return configured half-open world XY bounds.
FBox2D UAEAdaptiveEnvWorldSubsystem::GetGridWorldBounds() const
{
	return BehaviourGrid.GetWorldBounds();
}

// Return the current raw behaviour data revision.
int64 UAEAdaptiveEnvWorldSubsystem::GetBehaviourRevision() const
{
	return static_cast<int64>(BehaviourGrid.GetBehaviourRevision());
}

// Return the current per-tick dirty cell count.
int32 UAEAdaptiveEnvWorldSubsystem::GetDirtyCellCount() const
{
	return BehaviourGrid.GetDirtyCellIndices().Num();
}

// Return aggregate sample processing statistics by value.
FAEBehaviourGridStats UAEAdaptiveEnvWorldSubsystem::GetBehaviourGridStats() const
{
	return BehaviourGrid.GetStats();
}

// Reset all behaviour state while preserving active registrations.
void UAEAdaptiveEnvWorldSubsystem::ResetBehaviourGrid()
{
	// Clear grid data, queues, ordering guards, clocks, and scheduler counters.
	BehaviourGrid.Reset();
	ExposureGrid.Reset();
	ConstraintGrid.Reset();
	ResponseGrid.Reset();
	PathHeatmapGrid.Reset();
	PendingM6VisualCommands.Reset();
	PendingM7BaselineCellIndices.Reset();
	for (const TWeakObjectPtr<UAEVegetationDistributionComponent>& Distribution : RegisteredVegetationDistributions)
	{
		if (Distribution.IsValid())
		{
			RequestM7BaselineInitialization(Distribution.Get());
		}
	}
	M6VisualAccumulator = 0.0;
	for (const TWeakObjectPtr<UAEPathHeatmapRendererComponent>& Renderer : RegisteredPathHeatmapRenderers)
	{
		if (Renderer.IsValid())
		{
			Renderer->ResetVisualOutput();
			if (bM6Enabled)
			{
				Renderer->InitializeVisualOutput(GetGridDimensions(), GetGridWorldBounds());
			}
		}
	}
	M4InvalidSampleCount = 0;
	PendingSamples.Reset();
	ProcessingSamples.Reset();
	LastQueuedSequenceByAgent.Reset();
	LastQueuedTimestampByAgent.Reset();
	BehaviourAccumulator = 0.0;
	BehaviourTimeSeconds = 0.0;
	ProcessedBehaviourStepCount = 0;
	SchedulerOverrunCount = 0;
	PendingDebugActiveCellIndices.Reset();
	// Reset each valid tracker so its next observation is treated as the first.
	for (const TWeakObjectPtr<UAEBehaviourTrackerComponent>& Tracker : RegisteredTrackers)
	{
		if (Tracker.IsValid())
		{
			Tracker->ResetSamplingState();
		}
	}
}

/* Validates and atomically applies one complete M3/M4/M5 product profile. */
bool UAEAdaptiveEnvWorldSubsystem::ApplyEnvironmentProfile(UAEAdaptiveEnvironmentProfile* Profile, FString& OutError)
{
	check(IsInGameThread());
	OutError.Reset();
	if (!IsValid(Profile))
	{
		OutError = TEXT("Environment profile is null or invalid.");
		return false;
	}

	const uint32 NextRevision = ActiveEnvironmentConfig.RuntimeRevision == MAX_uint32
		? 1
		: ActiveEnvironmentConfig.RuntimeRevision + 1;
	FAEActiveEnvironmentConfig Candidate;
	const FAEM2ValidationResult Validation = FAEM2ConfigService::BuildActiveConfig(*Profile, NextRevision, Candidate);
	if (!Validation.IsValid())
	{
		OutError = Validation.ToString();
		return false;
	}

	// Commit the complete candidate once at the Game Thread boundary.
	const FName PreviousProfileId = ActiveEnvironmentConfig.ProfileId;
	ActiveEnvironmentConfig = MoveTemp(Candidate);
	ActiveMoistureTexture = ActiveEnvironmentConfig.MoistureTexture.Get();
	bM3Enabled = true;
	bM4Enabled = true;
	bM5Enabled = true;
	bM6Enabled = GetDefault<UAdaptiveEnvSettings>()->bEnableM6;
	bM7Enabled = GetDefault<UAdaptiveEnvSettings>()->bEnableM7;
	RebuildM3FromCurrentRawGrid();
	ConstraintGrid.Reset();
	ResponseGrid.Reset();
	PathHeatmapGrid.Reset();
	PendingM6VisualCommands.Reset();
	PendingM7BaselineCellIndices.Reset();
	for (const TWeakObjectPtr<UAEVegetationDistributionComponent>& Distribution : RegisteredVegetationDistributions)
	{
		if (Distribution.IsValid())
		{
			RequestM7BaselineInitialization(Distribution.Get());
		}
	}
	for (const TWeakObjectPtr<UAEPathHeatmapRendererComponent>& Renderer : RegisteredPathHeatmapRenderers)
	{
		if (Renderer.IsValid())
		{
			Renderer->ResetVisualOutput();
			if (bM6Enabled)
			{
				Renderer->InitializeVisualOutput(GetGridDimensions(), GetGridWorldBounds());
			}
		}
	}
	UE_LOG(
		LogAdaptiveEnv,
		Log,
		TEXT("Environment profile applied. World=%s OldProfile=%s NewProfile=%s ConfigVersion=%d RuntimeRevision=%u"),
		*GetNameSafe(GetWorld()),
		*PreviousProfileId.ToString(),
		*ActiveEnvironmentConfig.ProfileId.ToString(),
		ActiveEnvironmentConfig.ConfigVersion,
		ActiveEnvironmentConfig.RuntimeRevision);
	return true;
}

/* Resolve one immutable M7 snapshot across registered distribution owners. */
bool UAEAdaptiveEnvWorldSubsystem::GetM7PlantInstanceState(
	const int64 StablePointId,
	FAEPlantInstanceSnapshot& OutSnapshot) const
{
	if (!bM7Enabled)
	{
		return false;
	}
	for (const TWeakObjectPtr<UAEVegetationDistributionComponent>& Distribution : RegisteredVegetationDistributions)
	{
		if (Distribution.IsValid() && Distribution->GetPlantInstanceState(StablePointId, OutSnapshot))
		{
			return true;
		}
	}
	return false;
}

/* Collect all M7 snapshots in stable identity order for read-only consumers. */
void UAEAdaptiveEnvWorldSubsystem::GetM7PlantInstanceStates(
	TArray<FAEPlantInstanceSnapshot>& OutSnapshots) const
{
	OutSnapshots.Reset();
	if (!bM7Enabled)
	{
		return;
	}
	TArray<FAEPlantInstanceSnapshot> DistributionSnapshots;
	for (const TWeakObjectPtr<UAEVegetationDistributionComponent>& Distribution : RegisteredVegetationDistributions)
	{
		if (Distribution.IsValid())
		{
			Distribution->GetPlantInstanceStates(DistributionSnapshots);
			OutSnapshots.Append(DistributionSnapshots);
		}
	}
	OutSnapshots.Sort([](const FAEPlantInstanceSnapshot& A, const FAEPlantInstanceSnapshot& B)
	{
		return A.StablePointId < B.StablePointId;
	});
}

/* Forward a coordinate query to the World-owned M3 Grid. */
bool UAEAdaptiveEnvWorldSubsystem::GetM3Cell(const FIntPoint& Coordinate, FAEM3CellSnapshot& OutSnapshot) const
{
	return bM3Enabled && ExposureGrid.GetCellSnapshot(Coordinate, OutSnapshot);
}

/* Forward a world-position query to the World-owned M3 Grid. */
bool UAEAdaptiveEnvWorldSubsystem::GetM3CellAtWorldLocation(const FVector& Location, FAEM3CellSnapshot& OutSnapshot) const
{
	return bM3Enabled && ExposureGrid.GetCellSnapshotAtWorldLocation(Location, OutSnapshot);
}

/* Forward one coordinate query to the World-owned M4 Grid. */
bool UAEAdaptiveEnvWorldSubsystem::GetM4Cell(const FIntPoint& Coordinate, FAEEnvironmentConstraintSnapshot& OutSnapshot) const
{
	return bM4Enabled && ConstraintGrid.GetCellSnapshot(Coordinate, OutSnapshot);
}

/* Forward one world-position query to the World-owned M4 Grid. */
bool UAEAdaptiveEnvWorldSubsystem::GetM4CellAtWorldLocation(const FVector& Location, FAEEnvironmentConstraintSnapshot& OutSnapshot) const
{
	return bM4Enabled && ConstraintGrid.GetCellSnapshotAtWorldLocation(Location, OutSnapshot);
}

/* Forward one coordinate query to the World-owned M5 Grid. */
bool UAEAdaptiveEnvWorldSubsystem::GetM5Cell(const FIntPoint& Coordinate, FAEEcologicalResponseSnapshot& OutSnapshot) const
{
	return bM5Enabled && ResponseGrid.GetCellSnapshot(Coordinate, OutSnapshot);
}

/* Forward one world-position query to the World-owned M5 Grid. */
bool UAEAdaptiveEnvWorldSubsystem::GetM5CellAtWorldLocation(const FVector& Location, FAEEcologicalResponseSnapshot& OutSnapshot) const
{
	return bM5Enabled && ResponseGrid.GetCellSnapshotAtWorldLocation(Location, OutSnapshot);
}

/* Forward one coordinate query to the World-owned M6 Grid. */
bool UAEAdaptiveEnvWorldSubsystem::GetM6Cell(
	const FIntPoint& Coordinate,
	FAEPathHeatmapSnapshot& OutSnapshot) const
{
	return bM6Enabled
		&& PathHeatmapGrid.GetCellSnapshot(Coordinate, OutSnapshot);
}

/* Forward one world-position query to the World-owned M6 Grid. */
bool UAEAdaptiveEnvWorldSubsystem::GetM6CellAtWorldLocation(
	const FVector& Location,
	FAEPathHeatmapSnapshot& OutSnapshot) const
{
	return bM6Enabled
		&& PathHeatmapGrid.GetCellSnapshotAtWorldLocation(Location, OutSnapshot);
}

// Forward a bounded debug-cell query to the behaviour grid.
void UAEAdaptiveEnvWorldSubsystem::GetDebugCells(const FVector& Location, const float RadiusCm, const int32 MaxCells, TArray<FAEBehaviourCellSnapshot>& OutCells) const
{
	OutCells.Reset();
	TArray<FIntPoint> Coordinates;
	BuildDebugCellCoordinates(Location, RadiusCm, MaxCells, Coordinates);
	OutCells.Reserve(Coordinates.Num());
	for (const FIntPoint& Coordinate : Coordinates)
	{
		FAEBehaviourCellSnapshot Snapshot;
		if (BehaviourGrid.GetCellSnapshot(Coordinate, Snapshot))
		{
			OutCells.Add(MoveTemp(Snapshot));
		}
	}
}

/* Forward a bounded active-cell query to the World-owned M3 Grid. */
void UAEAdaptiveEnvWorldSubsystem::GetM3DebugCells(
	const FVector& Location,
	const float RadiusCm,
	const int32 MaxCells,
	TArray<FAEM3CellSnapshot>& OutCells) const
{
	if (!bM3Enabled)
	{
		OutCells.Reset();
		return;
	}
	OutCells.Reset();
	TArray<FIntPoint> Coordinates;
	BuildDebugCellCoordinates(Location, RadiusCm, MaxCells, Coordinates);
	OutCells.Reserve(Coordinates.Num());
	for (const FIntPoint& Coordinate : Coordinates)
	{
		FAEM3CellSnapshot Snapshot;
		if (ExposureGrid.GetCellSnapshot(Coordinate, Snapshot))
		{
			OutCells.Add(MoveTemp(Snapshot));
		}
	}
}

/* Collect committed M4 snapshots through the shared bounded debug coordinate window. */
void UAEAdaptiveEnvWorldSubsystem::GetM4DebugCells(const FVector& Location, const float RadiusCm, const int32 MaxCells, TArray<FAEEnvironmentConstraintSnapshot>& OutCells) const
{
	OutCells.Reset();
	if (!bM4Enabled) return;
	TArray<FIntPoint> Coordinates;
	BuildDebugCellCoordinates(Location, RadiusCm, MaxCells, Coordinates);
	for (const FIntPoint& Coordinate : Coordinates)
	{
		FAEEnvironmentConstraintSnapshot Snapshot;
		if (ConstraintGrid.GetCellSnapshot(Coordinate, Snapshot)) OutCells.Add(MoveTemp(Snapshot));
	}
}

/* Collect committed M5 snapshots through the shared bounded debug coordinate window. */
void UAEAdaptiveEnvWorldSubsystem::GetM5DebugCells(const FVector& Location, const float RadiusCm, const int32 MaxCells, TArray<FAEEcologicalResponseSnapshot>& OutCells) const
{
	OutCells.Reset();
	if (!bM5Enabled) return;
	TArray<FIntPoint> Coordinates;
	BuildDebugCellCoordinates(Location, RadiusCm, MaxCells, Coordinates);
	for (const FIntPoint& Coordinate : Coordinates)
	{
		FAEEcologicalResponseSnapshot Snapshot;
		if (ResponseGrid.GetCellSnapshot(Coordinate, Snapshot)) OutCells.Add(MoveTemp(Snapshot));
	}
}

/* Resolve a stable parameter-aware normalization maximum for M3 debug colour. */
float UAEAdaptiveEnvWorldSubsystem::GetM3DebugMaximumValue(const EAEHeatmapDebugMode Mode) const
{
	switch (Mode)
	{
	case EAEHeatmapDebugMode::CurrentExposure:
		return static_cast<float>(ActiveEnvironmentConfig.M3.ExposureDynamics.Maximum);
	case EAEHeatmapDebugMode::PassExposure:
	case EAEHeatmapDebugMode::TravelExposure:
	case EAEHeatmapDebugMode::DwellExposure:
	case EAEHeatmapDebugMode::SprintExposure:
	case EAEHeatmapDebugMode::CollectExposure:
	case EAEHeatmapDebugMode::CombatExposure:
		return 1.0f;
	default:
		return 0.0f;
	}
}

// Apply deferred removals before additions for trackers and renderers.
void UAEAdaptiveEnvWorldSubsystem::ApplyPendingRegistrations()
{
	// Remove requested and expired tracker references before adding new ones.
	for (const TWeakObjectPtr<UAEBehaviourTrackerComponent>& Tracker : PendingTrackerRemoves)
	{
		RegisteredTrackers.Remove(Tracker);
	}
	PendingTrackerRemoves.Reset();
	RegisteredTrackers.RemoveAll([](const TWeakObjectPtr<UAEBehaviourTrackerComponent>& Item) { return !Item.IsValid(); });
	for (const TWeakObjectPtr<UAEBehaviourTrackerComponent>& Tracker : PendingTrackerAdds)
	{
		if (Tracker.IsValid())
		{
			RegisteredTrackers.AddUnique(Tracker);
		}
	}
	PendingTrackerAdds.Reset();

	// Remove requested and expired renderer references before adding new ones.
	for (const TWeakObjectPtr<UAEHeatmapRendererComponent>& Renderer : PendingRendererRemoves)
	{
		RegisteredRenderers.Remove(Renderer);
	}
	PendingRendererRemoves.Reset();
	RegisteredRenderers.RemoveAll([](const TWeakObjectPtr<UAEHeatmapRendererComponent>& Item) { return !Item.IsValid(); });
	for (const TWeakObjectPtr<UAEHeatmapRendererComponent>& Renderer : PendingRendererAdds)
	{
		if (Renderer.IsValid())
		{
			RegisteredRenderers.AddUnique(Renderer);
		}
	}
	PendingRendererAdds.Reset();

	// Apply M6 renderer removals before additions and initialize new visual bindings once.
	for (const TWeakObjectPtr<UAEPathHeatmapRendererComponent>& Renderer : PendingPathHeatmapRendererRemoves)
	{
		RegisteredPathHeatmapRenderers.Remove(Renderer);
	}
	PendingPathHeatmapRendererRemoves.Reset();
	RegisteredPathHeatmapRenderers.RemoveAll(
		[](const TWeakObjectPtr<UAEPathHeatmapRendererComponent>& Item)
		{
			return !Item.IsValid();
		});
	for (const TWeakObjectPtr<UAEPathHeatmapRendererComponent>& Renderer : PendingPathHeatmapRendererAdds)
	{
		if (!Renderer.IsValid())
		{
			continue;
		}
		RegisteredPathHeatmapRenderers.AddUnique(Renderer);
		if (bM6Enabled
			&& Renderer->InitializeVisualOutput(GetGridDimensions(), GetGridWorldBounds()))
		{
			QueueFullM6VisualRebuild(*Renderer);
		}
	}
	PendingPathHeatmapRendererAdds.Reset();

	// Apply moisture-source removals before additions so sampling sees one stable array.
	for (const TWeakObjectPtr<UAEMoistureSourceComponent>& Source : PendingMoistureSourceRemoves)
	{
		RegisteredMoistureSources.Remove(Source);
	}
	PendingMoistureSourceRemoves.Reset();
	RegisteredMoistureSources.RemoveAll([](const TWeakObjectPtr<UAEMoistureSourceComponent>& Item) { return !Item.IsValid(); });
	for (const TWeakObjectPtr<UAEMoistureSourceComponent>& Source : PendingMoistureSourceAdds)
	{
		if (Source.IsValid()) RegisteredMoistureSources.AddUnique(Source);
	}
	PendingMoistureSourceAdds.Reset();

	for (const TWeakObjectPtr<UAEVegetationDistributionComponent>& Distribution : PendingVegetationDistributionRemoves)
	{
		RegisteredVegetationDistributions.Remove(Distribution);
	}
	PendingVegetationDistributionRemoves.Reset();
	RegisteredVegetationDistributions.RemoveAll(
		[](const TWeakObjectPtr<UAEVegetationDistributionComponent>& Item) { return !Item.IsValid(); });
	for (const TWeakObjectPtr<UAEVegetationDistributionComponent>& Distribution : PendingVegetationDistributionAdds)
	{
		if (Distribution.IsValid())
		{
			RegisteredVegetationDistributions.AddUnique(Distribution);
			if (bM7Enabled)
			{
				Distribution->InitializeDistribution(
					GetGridDimensions(),
					GetGridWorldBounds(),
					GetDefault<UAdaptiveEnvSettings>()->M4GroundTraceHalfHeightCm);
			}
		}
	}
	PendingVegetationDistributionAdds.Reset();

	// Apply M8 removals before additions so fixed-step visual reads use one stable array.
	for (const TWeakObjectPtr<UAELSystemPlantComponent>& Plant : PendingLSystemPlantRemoves)
	{
		RegisteredLSystemPlants.Remove(Plant);
	}
	PendingLSystemPlantRemoves.Reset();
	RegisteredLSystemPlants.RemoveAll(
		[](const TWeakObjectPtr<UAELSystemPlantComponent>& Item) { return !Item.IsValid(); });
	for (const TWeakObjectPtr<UAELSystemPlantComponent>& Plant : PendingLSystemPlantAdds)
	{
		if (Plant.IsValid())
		{
			RegisteredLSystemPlants.AddUnique(Plant);
		}
	}
	PendingLSystemPlantAdds.Reset();

	// Apply M8 neighborhood-manager removals before additions for stable fixed-step iteration.
	for (const TWeakObjectPtr<UAERepresentativePlantManagerComponent>& Manager : PendingRepresentativePlantManagerRemoves)
	{
		RegisteredRepresentativePlantManagers.Remove(Manager);
	}
	PendingRepresentativePlantManagerRemoves.Reset();
	RegisteredRepresentativePlantManagers.RemoveAll(
		[](const TWeakObjectPtr<UAERepresentativePlantManagerComponent>& Item) { return !Item.IsValid(); });
	for (const TWeakObjectPtr<UAERepresentativePlantManagerComponent>& Manager : PendingRepresentativePlantManagerAdds)
	{
		if (Manager.IsValid())
		{
			RegisteredRepresentativePlantManagers.AddUnique(Manager);
		}
	}
	PendingRepresentativePlantManagerAdds.Reset();
}

// Pull one sample from each valid tracker at the shared fixed time.
void UAEAdaptiveEnvWorldSubsystem::SampleRegisteredTrackers(const float StepSeconds)
{
	// Skip expired weak references without mutating the active array.
	for (const TWeakObjectPtr<UAEBehaviourTrackerComponent>& Tracker : RegisteredTrackers)
	{
		if (!Tracker.IsValid())
		{
			continue;
		}
		// Submit only complete samples produced by the tracker.
		FAEBehaviourSample Sample;
		if (Tracker->CaptureBehaviourSample(BehaviourTimeSeconds, StepSeconds, Sample))
		{
			SubmitBehaviourSample(Sample);
		}
	}
}

// Freeze, sort, and aggregate the current sample batch deterministically.
void UAEAdaptiveEnvWorldSubsystem::ProcessPendingSamples()
{
	// Swap producer and consumer buffers so new submissions remain isolated.
	Swap(PendingSamples, ProcessingSamples);
	PendingSamples.Reset();
	// Sort by timestamp, agent identity, and sequence for stable replay results.
	ProcessingSamples.Sort([](const FAEBehaviourSample& Left, const FAEBehaviourSample& Right)
	{
		if (Left.Timestamp != Right.Timestamp)
		{
			return Left.Timestamp < Right.Timestamp;
		}
		if (Left.AgentId != Right.AgentId)
		{
			return Left.AgentId < Right.AgentId;
		}
		return Left.SequenceNumber < Right.SequenceNumber;
	});

	// Aggregate the stable batch on the Game Thread, then release it.
	for (const FAEBehaviourSample& Sample : ProcessingSamples)
	{
		BehaviourGrid.AccumulateSample(Sample);
	}
	ProcessingSamples.Reset();
}

/* Advance M3 immediately after one fixed-step raw aggregation stage. */
void UAEAdaptiveEnvWorldSubsystem::UpdateM3(const float StepSeconds)
{
	if (!bM3Enabled)
	{
		return;
	}

	// Convert the shared fixed real step into the only M3 simulation-time basis.
	const double DeltaSimulationHours = static_cast<double>(StepSeconds) * SimulationHoursPerRealSecond;
	const double SimulationTimeHours = BehaviourTimeSeconds * SimulationHoursPerRealSecond;
	if (!ExposureGrid.Update(
		BehaviourGrid,
		BehaviourGrid.GetDirtyCellIndices(),
		SimulationTimeHours,
		DeltaSimulationHours,
		BehaviourGrid.GetBehaviourRevision(),
		ActiveEnvironmentConfig.M3))
	{
		bM3Enabled = false;
		UE_LOG(LogAdaptiveEnv, Error, TEXT("M3 update failed and was disabled. World=%s BehaviourRevision=%llu"), *GetNameSafe(GetWorld()), BehaviourGrid.GetBehaviourRevision());
	}
}

/* Sample terrain and moisture, then commit M4 before any M5 input is frozen. */
void UAEAdaptiveEnvWorldSubsystem::UpdateM4(const float StepSeconds)
{
	if (!bM4Enabled) return;
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		bM4Enabled = false;
		return;
	}

	// Merge raw, M3, and M7 baseline requests with Cells still waiting on an M4 transition.
	TArray<int32> NewlyRelevant = BehaviourGrid.GetDirtyCellIndices();
	for (const int32 Index : ExposureGrid.GetLastChangedCellIndices()) NewlyRelevant.AddUnique(Index);
	for (const int32 Index : PendingM7BaselineCellIndices) NewlyRelevant.AddUnique(Index);
	const int32 RequestedM7BaselineCount = PendingM7BaselineCellIndices.Num();
	PendingM7BaselineCellIndices.Reset();
	TArray<int32> CandidateIndices;
	ConstraintGrid.BuildCandidateIndices(NewlyRelevant, CandidateIndices);
	TArray<FAEWorldConstraintObservation> Observations;
	Observations.Reserve(CandidateIndices.Num());
	const UAdaptiveEnvSettings* Settings = GetDefault<UAdaptiveEnvSettings>();
	const FIntPoint Dimensions = BehaviourGrid.GetConfig().Dimensions;

	// Execute UObject and collision queries only on the Game Thread.
	for (const int32 Index : CandidateIndices)
	{
		if (Index < 0 || Index >= Dimensions.X * Dimensions.Y) continue;
		const FIntPoint Coordinate(Index % Dimensions.X, Index / Dimensions.X);
		FAEWorldConstraintObservation Observation;
		if (FAEWorldConstraintProvider::SampleCell(*World, Coordinate, ConstraintGrid.GetCellWorldCenter(Coordinate),
			Settings->M4GroundTraceHalfHeightCm,
			static_cast<float>(ActiveEnvironmentConfig.DefaultMoistureRatio),
			ActiveMoistureTexture,
			RegisteredMoistureSources,
			Observation))
		{
			Observations.Add(MoveTemp(Observation));
		}
		else
		{
			++M4InvalidSampleCount;
		}
	}
	const double DeltaSimulationHours = static_cast<double>(StepSeconds) * SimulationHoursPerRealSecond;
	if (!ConstraintGrid.Update(Observations, DeltaSimulationHours, static_cast<uint64>(ProcessedBehaviourStepCount + 1), ActiveEnvironmentConfig.M4))
	{
		bM4Enabled = false;
		bM5Enabled = false;
		bM6Enabled = false;
		bM7Enabled = false;
		UE_LOG(LogAdaptiveEnv, Error, TEXT("M4 update failed; M4 and dependent M5/M6 were disabled. World=%s"), *GetNameSafe(World));
	}
	else if (RequestedM7BaselineCount > 0)
	{
		UE_LOG(
			LogAdaptiveEnv,
			Log,
			TEXT("M7 baseline environment initialized. World=%s RequestedCells=%d ValidObservations=%d"),
			*GetNameSafe(World),
			RequestedM7BaselineCount,
			Observations.Num());
	}
}

/* Advance registered M7 owners from shared M4/M5 fixed-step work sets. */
void UAEAdaptiveEnvWorldSubsystem::UpdateM7(const float StepSeconds)
{
	if (!bM7Enabled || !bM4Enabled || !bM5Enabled)
	{
		return;
	}
	const double DeltaSimulationHours =
		static_cast<double>(StepSeconds) * SimulationHoursPerRealSecond;
	const int64 CurrentStep = ProcessedBehaviourStepCount + 1;
	const TArray<int32>& SharedM5Dirty = ResponseGrid.GetLastChangedCellIndices();
	const TArray<int32>& DistributionDirty = ConstraintGrid.GetLastChangedCellIndices();
	for (const TWeakObjectPtr<UAEVegetationDistributionComponent>& Distribution : RegisteredVegetationDistributions)
	{
		if (Distribution.IsValid())
		{
			Distribution->AdvanceM7(
				*this,
				SharedM5Dirty,
				DistributionDirty,
				DeltaSimulationHours,
				CurrentStep);
		}
	}
}

/* Activate bounded player-neighborhood representatives after M7 commits. */
void UAEAdaptiveEnvWorldSubsystem::UpdateM8NeighborhoodActivation(const float StepSeconds)
{
	if (!bM8Enabled || !bM7Enabled)
	{
		return;
	}
	for (const TWeakObjectPtr<UAERepresentativePlantManagerComponent>& Manager : RegisteredRepresentativePlantManagers)
	{
		if (Manager.IsValid())
		{
			Manager->AdvanceNeighborhoodActivation(*this, StepSeconds);
		}
	}
}

/* Resolve registered M8 lifecycle visuals after activation without modifying fixed mesh topology. */
void UAEAdaptiveEnvWorldSubsystem::UpdateM8()
{
	if (!bM8Enabled)
	{
		return;
	}
	const int32 Budget = FMath::Max(GetDefault<UAdaptiveEnvSettings>()->M8MaxPlantsPerStep, 1);
	TArray<int32> ScheduledIndices;
	FAEM8RoundRobinScheduler::BuildWindow(
		RegisteredLSystemPlants.Num(),
		Budget,
		M8UpdateCursor,
		ScheduledIndices);

	// Advance only the fair bounded window selected for this fixed step.
	for (const int32 PlantIndex : ScheduledIndices)
	{
		if (RegisteredLSystemPlants.IsValidIndex(PlantIndex))
		{
			if (UAELSystemPlantComponent* Plant = RegisteredLSystemPlants[PlantIndex].Get())
			{
				Plant->AdvanceM8(*this);
			}
		}
	}
}

/* Apply bounded M7 visual writes after all fixed substeps complete. */
void UAEAdaptiveEnvWorldSubsystem::UpdateM7VisualAdapters()
{
	if (!bM7Enabled)
	{
		return;
	}
	const int32 Budget = FMath::Max(GetDefault<UAdaptiveEnvSettings>()->M7MaxInstanceUpdatesPerFrame, 1);
	for (const TWeakObjectPtr<UAEVegetationDistributionComponent>& Distribution : RegisteredVegetationDistributions)
	{
		if (Distribution.IsValid())
		{
			Distribution->ApplyVisualBudget(Budget);
		}
	}
}

/* Freeze matching committed M3/M4 snapshots and advance the sole Damage owner. */
void UAEAdaptiveEnvWorldSubsystem::UpdateM5(const float StepSeconds)
{
	if (!bM5Enabled || !bM3Enabled || !bM4Enabled) return;
	TArray<int32> CandidateIndices;
	ResponseGrid.BuildCandidateIndices(ExposureGrid.GetLastChangedCellIndices(), ConstraintGrid.GetLastChangedCellIndices(), CandidateIndices);
	TArray<FAEM5InputSnapshot> Inputs;
	Inputs.Reserve(CandidateIndices.Num());
	const FIntPoint Dimensions = BehaviourGrid.GetConfig().Dimensions;

	// Freeze only Cells with both upstream snapshots committed for this World.
	for (const int32 Index : CandidateIndices)
	{
		if (Index < 0 || Index >= Dimensions.X * Dimensions.Y) continue;
		const FIntPoint Coordinate(Index % Dimensions.X, Index / Dimensions.X);
		FAEM3CellSnapshot M3;
		FAEEnvironmentConstraintSnapshot M4;
		if (!ExposureGrid.GetCellSnapshot(Coordinate, M3) || !ConstraintGrid.GetCellSnapshot(Coordinate, M4)) continue;
		FAEM5InputSnapshot& Input = Inputs.AddDefaulted_GetRef();
		Input.Coordinate = Coordinate;
		Input.Exposure = M3.CurrentExposure;
		Input.ExposureMaximum = ActiveEnvironmentConfig.M3.ExposureDynamics.Maximum;
		Input.ConstraintPressureRatio = M4.ConstraintPressureRatio;
		Input.HabitatSuitabilityRatio = M4.HabitatSuitabilityRatio;
		Input.ExposureRevision = static_cast<uint64>(FMath::Max(M3.ExposureRevision, static_cast<int64>(0)));
		Input.ConstraintRevision = static_cast<uint64>(FMath::Max(M4.ConstraintRevision, static_cast<int64>(0)));
		Input.SimulationStep = static_cast<uint64>(ProcessedBehaviourStepCount + 1);
		Input.ConfigRevision = ActiveEnvironmentConfig.RuntimeRevision;
	}
	const double DeltaSimulationHours = static_cast<double>(StepSeconds) * SimulationHoursPerRealSecond;
	if (!ResponseGrid.Update(Inputs, DeltaSimulationHours, ActiveEnvironmentConfig.M5, ActiveEnvironmentConfig.RuntimeRevision))
	{
		bM5Enabled = false;
		bM6Enabled = false;
		bM7Enabled = false;
		UE_LOG(LogAdaptiveEnv, Error, TEXT("M5 update failed; M5 and dependent M6/M7 were disabled. World=%s"), *GetNameSafe(GetWorld()));
	}
}

/* Freeze M5 snapshots and advance complete M6 path visual state. */
void UAEAdaptiveEnvWorldSubsystem::UpdateM6(const float StepSeconds)
{
	if (!bM6Enabled || !bM5Enabled)
	{
		return;
	}

	// Merge new M5 responses with Cells still completing an M6 visual transition.
	TArray<int32> CandidateIndices;
	PathHeatmapGrid.BuildCandidateIndices(
		BehaviourGrid.GetDirtyCellIndices(),
		ResponseGrid.GetLastChangedCellIndices(),
		CandidateIndices);
	TArray<FAEM6InputSnapshot> Inputs;
	Inputs.Reserve(CandidateIndices.Num());
	const FIntPoint Dimensions = BehaviourGrid.GetConfig().Dimensions;
	const uint64 CurrentStep = static_cast<uint64>(ProcessedBehaviourStepCount + 1);

	// Freeze committed M1 Flow and M5 response snapshots from valid shared Grid coordinates.
	for (const int32 Index : CandidateIndices)
	{
		if (Index < 0 || Index >= Dimensions.X * Dimensions.Y)
		{
			continue;
		}
		const FIntPoint Coordinate(Index % Dimensions.X, Index / Dimensions.X);
		FAEBehaviourCellSnapshot M1;
		BehaviourGrid.GetCellSnapshot(Coordinate, M1);
		FAEEcologicalResponseSnapshot M5;
		if (!ResponseGrid.GetCellSnapshot(Coordinate, M5))
		{
			continue;
		}
		FAEM6InputSnapshot& Input = Inputs.AddDefaulted_GetRef();
		Input.Coordinate = Coordinate;
		Input.FlowDirection = M1.FlowDirection;
		Input.FlowMagnitude = FMath::Clamp(static_cast<double>(M1.FlowMagnitude), 0.0, 1.0);
		Input.SourceBehaviourRevision = BehaviourGrid.GetBehaviourRevision();
		Input.DamageRatio = M5.DamageRatio;
		Input.SourceResponseRevision = static_cast<uint64>(
			FMath::Max(M5.ResponseRevision, static_cast<int64>(0)));
		Input.SourceResponseSimulationStep = static_cast<uint64>(
			FMath::Max(M5.SimulationStep, static_cast<int64>(0)));
		Input.CurrentSimulationStep = CurrentStep;
	}

	const double DeltaSimulationHours =
		static_cast<double>(StepSeconds) * SimulationHoursPerRealSecond;
	if (!PathHeatmapGrid.Update(Inputs, DeltaSimulationHours, M6Parameters))
	{
		bM6Enabled = false;
		PendingM6VisualCommands.Reset();
		for (const TWeakObjectPtr<UAEPathHeatmapRendererComponent>& Renderer : RegisteredPathHeatmapRenderers)
		{
			if (Renderer.IsValid())
			{
				Renderer->ResetVisualOutput();
			}
		}
		UE_LOG(
			LogAdaptiveEnv,
			Error,
			TEXT("M6 update failed and visual output was disabled. World=%s"),
			*GetNameSafe(GetWorld()));
		return;
	}

	// Coalesce all fixed substeps so render-frame pacing cannot lose a Cell update.
	for (const FAEPathHeatmapVisualCommand& Command : PathHeatmapGrid.GetVisualCommands())
	{
		PendingM6VisualCommands.Add(Command.CellIndex, Command);
	}
}

/* Flush coalesced M6 commands and apply a bounded visual refresh. */
void UAEAdaptiveEnvWorldSubsystem::UpdateM6VisualRenderers(
	const float DeltaTime)
{
	if (!bM6Enabled)
	{
		return;
	}
	const UAdaptiveEnvSettings* Settings = GetDefault<UAdaptiveEnvSettings>();
	const double RefreshStep = 1.0 / FMath::Max(Settings->M6VisualApplyRateHz, 0.1f);
	M6VisualAccumulator += FMath::Max(static_cast<double>(DeltaTime), 0.0);
	if (M6VisualAccumulator < RefreshStep)
	{
		return;
	}
	M6VisualAccumulator = FMath::Fmod(M6VisualAccumulator, RefreshStep);

	// Publish commands in row-major order before each renderer applies its own queue budget.
	TArray<FAEPathHeatmapVisualCommand> OrderedCommands;
	PendingM6VisualCommands.GenerateValueArray(OrderedCommands);
	OrderedCommands.Sort(
		[](const FAEPathHeatmapVisualCommand& A, const FAEPathHeatmapVisualCommand& B)
		{
			return A.CellIndex < B.CellIndex;
		});
	for (const TWeakObjectPtr<UAEPathHeatmapRendererComponent>& Renderer : RegisteredPathHeatmapRenderers)
	{
		if (Renderer.IsValid())
		{
			Renderer->EnqueueVisualCommands(OrderedCommands);
			Renderer->ApplyVisualBudget(
				FMath::Max(Settings->M6MaxVisualCommandsPerFrame, 1));
		}
	}
	PendingM6VisualCommands.Reset();
}

/* Queue one deterministic full texture reconstruction for a newly bound renderer. */
void UAEAdaptiveEnvWorldSubsystem::QueueFullM6VisualRebuild(
	UAEPathHeatmapRendererComponent& Renderer) const
{
	TArray<FAEPathHeatmapVisualCommand> Commands;
	PathHeatmapGrid.BuildFullVisualCommands(Commands);
	Renderer.EnqueueVisualCommands(Commands);
}

/* Rebuild M3 deterministically from all current cumulative raw Cell totals. */
void UAEAdaptiveEnvWorldSubsystem::RebuildM3FromCurrentRawGrid()
{
	ExposureGrid.Reset();
	if (!bM3Enabled)
	{
		return;
	}

	// Treat every row-major Cell as dirty so current raw totals are consumed once under the new bundle.
	const FIntPoint Dimensions = BehaviourGrid.GetConfig().Dimensions;
	const int32 CellCount = Dimensions.X * Dimensions.Y;
	TArray<int32> AllCellIndices;
	AllCellIndices.Reserve(CellCount);
	for (int32 Index = 0; Index < CellCount; ++Index)
	{
		AllCellIndices.Add(Index);
	}
	const double SimulationTimeHours = BehaviourTimeSeconds * SimulationHoursPerRealSecond;
	if (!ExposureGrid.Update(
		BehaviourGrid,
		AllCellIndices,
		SimulationTimeHours,
		0.0,
		BehaviourGrid.GetBehaviourRevision(),
		ActiveEnvironmentConfig.M3))
	{
		bM3Enabled = false;
	}
}

/* Preserve every raw Cell changed between independent debug refreshes. */
void UAEAdaptiveEnvWorldSubsystem::AccumulateDebugActiveCells()
{
	for (const int32 Index : BehaviourGrid.GetDirtyCellIndices())
	{
		PendingDebugActiveCellIndices.Add(Index);
	}
}

/* Build a bounded deterministic coordinate window around recently active raw Cells. */
void UAEAdaptiveEnvWorldSubsystem::BuildDebugCellCoordinates(
	const FVector& Location,
	const float RadiusCm,
	const int32 MaxCells,
	TArray<FIntPoint>& OutCoordinates) const
{
	OutCoordinates.Reset();
	if (MaxCells <= 0 || PendingDebugActiveCellIndices.IsEmpty())
	{
		return;
	}

	struct FCandidate
	{
		int32 Index = INDEX_NONE;
		double DistanceSquared = 0.0;
	};

	// Expand recent activity into one deduplicated in-bounds Chebyshev neighbourhood.
	const FAEHeatmapGridConfig& Config = BehaviourGrid.GetConfig();
	const int32 CellCount = Config.Dimensions.X * Config.Dimensions.Y;
	const int32 NeighbourRadius = FMath::Max(GetDefault<UAdaptiveEnvSettings>()->DebugActiveNeighbourRadiusCells, 0);
	TBitArray<> IncludedFlags(false, CellCount);
	for (const int32 ActiveIndex : PendingDebugActiveCellIndices)
	{
		if (ActiveIndex < 0 || ActiveIndex >= CellCount)
		{
			continue;
		}
		const FIntPoint ActiveCoordinate(ActiveIndex % Config.Dimensions.X, ActiveIndex / Config.Dimensions.X);
		for (int32 OffsetY = -NeighbourRadius; OffsetY <= NeighbourRadius; ++OffsetY)
		{
			for (int32 OffsetX = -NeighbourRadius; OffsetX <= NeighbourRadius; ++OffsetX)
			{
				const FIntPoint Coordinate = ActiveCoordinate + FIntPoint(OffsetX, OffsetY);
				int32 NeighbourIndex = INDEX_NONE;
				if (BehaviourGrid.CellToIndex(Coordinate, NeighbourIndex))
				{
					IncludedFlags[NeighbourIndex] = true;
				}
			}
		}
	}

	// Apply the renderer-centred XY radius before the nearest-first Cell budget.
	const double RadiusSquared = FMath::Square(static_cast<double>(FMath::Max(RadiusCm, 0.0f)));
	const FVector2D QueryLocation(Location);
	TArray<FCandidate> Candidates;
	for (TConstSetBitIterator<> Iterator(IncludedFlags); Iterator; ++Iterator)
	{
		const int32 Index = Iterator.GetIndex();
		const FIntPoint Coordinate(Index % Config.Dimensions.X, Index / Config.Dimensions.X);
		const FVector Center = BehaviourGrid.GetCellWorldCenter(Coordinate);
		const double DistanceSquared = FVector2D::DistSquared(FVector2D(Center), QueryLocation);
		if (DistanceSquared <= RadiusSquared)
		{
			Candidates.Add({Index, DistanceSquared});
		}
	}
	Candidates.Sort([](const FCandidate& A, const FCandidate& B)
	{
		return A.DistanceSquared < B.DistanceSquared
			|| (FMath::IsNearlyEqual(A.DistanceSquared, B.DistanceSquared) && A.Index < B.Index);
	});

	// Publish stable coordinates shared by M1 and M3 read-only debug queries.
	const int32 ResultCount = FMath::Min(Candidates.Num(), MaxCells);
	OutCoordinates.Reserve(ResultCount);
	for (int32 ResultIndex = 0; ResultIndex < ResultCount; ++ResultIndex)
	{
		const int32 Index = Candidates[ResultIndex].Index;
		OutCoordinates.Add(FIntPoint(Index % Config.Dimensions.X, Index / Config.Dimensions.X));
	}
}

// Refresh registered debug renderers at an independent configured rate.
void UAEAdaptiveEnvWorldSubsystem::UpdateDebugRenderers(const float DeltaTime)
{
	// Accumulate safe render time until one debug refresh is due.
	const UAdaptiveEnvSettings* Settings = GetDefault<UAdaptiveEnvSettings>();
	const double RefreshStep = 1.0 / FMath::Max(Settings->DebugRefreshRateHz, 0.1f);
	DebugAccumulator += FMath::Max(static_cast<double>(DeltaTime), 0.0);
	if (DebugAccumulator < RefreshStep)
	{
		return;
	}
	DebugAccumulator = FMath::Fmod(DebugAccumulator, RefreshStep);

	// Render only through valid weak registrations.
	for (const TWeakObjectPtr<UAEHeatmapRendererComponent>& Renderer : RegisteredRenderers)
	{
		if (Renderer.IsValid())
		{
			Renderer->RenderDebug(*this, static_cast<float>(RefreshStep));
		}
	}

	// Start a fresh activity window only after every renderer consumed this refresh.
	PendingDebugActiveCellIndices.Reset();
}

// Validate sample identity, finite values, ranges, and supported tags.
bool UAEAdaptiveEnvWorldSubsystem::ValidateQueuedSample(const FAEBehaviourSample& Sample, EAEBehaviourSubmitResult& OutResult) const
{
	// Require a stable agent identity.
	if (!Sample.AgentId.IsValid())
	{
		OutResult = EAEBehaviourSubmitResult::InvalidAgent;
		return false;
	}
	// Reject invalid spatial inputs before distance or grid operations.
	if (Sample.WorldLocation.ContainsNaN()
		|| Sample.Velocity.ContainsNaN()
		|| (Sample.bHasPreviousLocation && Sample.PreviousWorldLocation.ContainsNaN()))
	{
		OutResult = EAEBehaviourSubmitResult::InvalidLocation;
		return false;
	}
	// Reject non-finite, negative, or invalid ordering values.
	if (!FMath::IsFinite(Sample.Timestamp)
		|| !FMath::IsFinite(Sample.DeltaSeconds)
		|| !FMath::IsFinite(Sample.TravelDistanceMeters)
		|| !FMath::IsFinite(Sample.EventIntensity)
		|| Sample.DeltaSeconds < 0.0f
		|| Sample.TravelDistanceMeters < 0.0f
		|| Sample.EventIntensity < 0.0f
		|| Sample.SequenceNumber < 0)
	{
		OutResult = EAEBehaviourSubmitResult::InvalidTimestamp;
		return false;
	}

	// Accept only native behaviour tags understood by the raw grid.
	const FGameplayTag& Tag = Sample.BehaviourTag;
	const bool bKnownTag = Tag == AdaptiveEnvGameplayTags::Behaviour_Move.GetTag()
		|| Tag == AdaptiveEnvGameplayTags::Behaviour_Dwell.GetTag()
		|| Tag == AdaptiveEnvGameplayTags::Behaviour_Sprint.GetTag()
		|| Tag == AdaptiveEnvGameplayTags::Behaviour_Collect.GetTag()
		|| Tag == AdaptiveEnvGameplayTags::Behaviour_Combat.GetTag();
	if (!bKnownTag)
	{
		OutResult = EAEBehaviourSubmitResult::InvalidTag;
		return false;
	}
	return true;
}
