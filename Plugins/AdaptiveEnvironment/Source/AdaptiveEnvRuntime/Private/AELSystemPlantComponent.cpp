#include "AELSystemPlantComponent.h"

#include "AdaptiveEnvLog.h"
#include "AdaptiveEnvWorldSubsystem.h"
#include "AELSystemGenerator.h"
#include "AELSystemRuleAsset.h"
#include "Components/BoxComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"

/* Create a non-ticking M8 component whose work remains ordered by the World subsystem. */
UAELSystemPlantComponent::UAELSystemPlantComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

/* Register one M8 plant and optionally build its fixed independent preview. */
void UAELSystemPlantComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UWorld* World = GetWorld())
	{
		if (UAEAdaptiveEnvWorldSubsystem* Subsystem = World->GetSubsystem<UAEAdaptiveEnvWorldSubsystem>())
		{
			Subsystem->RegisterLSystemPlant(this);
		}
	}
	if (bGenerateOnBeginPlay && !bGenerated)
	{
		FString Error;
		if (!GeneratePreview(Error))
		{
			UE_LOG(LogAdaptiveEnv, Warning, TEXT("M8 preview generation failed. Owner=%s Error=%s"), *GetNameSafe(GetOwner()), *Error);
		}
	}
}

/* Unregister one M8 plant and destroy only components it created. */
void UAELSystemPlantComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UAEAdaptiveEnvWorldSubsystem* Subsystem = World->GetSubsystem<UAEAdaptiveEnvWorldSubsystem>())
		{
			Subsystem->UnregisterLSystemPlant(this);
		}
	}
	ClearGeneratedPlant();
	Super::EndPlay(EndPlayReason);
}

/* Generate deterministic logical data and submit every fixed mesh module once. */
bool UAELSystemPlantComponent::GeneratePreview(FString& OutError)
{
	OutError.Reset();
	if (RuleAsset == nullptr || GetOwner() == nullptr || GetWorld() == nullptr)
	{
		OutError = TEXT("M8 requires a Rule Asset, owner, and World.");
		return false;
	}

	// Clear only the previous M8-owned result before a deliberate full regeneration.
	ClearGeneratedPlant();
	const int64 PlantId = SourceStablePointId != 0
		? SourceStablePointId
		: static_cast<int64>(HashCombineFast(GetTypeHash(GetOwner()->GetFName()), GetTypeHash(GenerationSeed)));
	if (!FAELSystemGenerator::Generate(*RuleAsset, GenerationSeed, PlantId, GeneratedPlant, OutError))
	{
		return false;
	}
	if (!BuildFixedMeshComponents(OutError))
	{
		ClearGeneratedPlant();
		return false;
	}
	if (!InitializeLeafInstances(OutError))
	{
		ClearGeneratedPlant();
		return false;
	}
	bGenerated = true;
	ApplyResolvedVisualState(
		ResolvePlantState(GetWorld()->GetSubsystem<UAEAdaptiveEnvWorldSubsystem>()),
		false);
	return true;
}

/* Destroy every visual owner and reset generated logical state. */
void UAELSystemPlantComponent::ClearGeneratedPlant()
{
	for (UHierarchicalInstancedStaticMeshComponent* Component : OwnedLeafInstanceComponents)
	{
		DestroyOwnedComponent(Component);
	}
	for (UDynamicMeshComponent* Component : OwnedMeshComponents)
	{
		DestroyOwnedComponent(Component);
	}
	for (UBoxComponent* Component : OwnedPhysicsRoots)
	{
		DestroyOwnedComponent(Component);
	}
	OwnedMeshComponents.Reset();
	OwnedPhysicsRoots.Reset();
	OwnedLeafInstanceComponents.Reset();
	BranchModules.Reset();
	GeneratedPlant = FAELSystemGeneratedPlant();
	bGenerated = false;
	bWaitingForDebrisRelease = false;
	bOwnerDestroyRequested = false;
	LeafInstanceCount = 0;
}

/* Bind one immutable M7 snapshot and optionally rebuild using its internal stable identity. */
bool UAELSystemPlantComponent::BindToM7PlantSnapshot(
	const FAEPlantInstanceSnapshot& Snapshot,
	const bool bRegenerate,
	FString& OutError)
{
	OutError.Reset();
	if (Snapshot.StablePointId <= 0
		|| !FMath::IsFinite(Snapshot.HealthRatio)
		|| !FMath::IsFinite(Snapshot.LifecycleProgressRatio))
	{
		OutError = TEXT("M8 requires a valid M7 plant snapshot.");
		return false;
	}

	// Store the internal lookup identity while keeping it out of the Blueprint binding chain.
	SourceStablePointId = Snapshot.StablePointId;
	InputMode = EAELSystemInputMode::M7Driven;
	if (bRegenerate && !GeneratePreview(OutError))
	{
		return false;
	}

	// Apply the selected snapshot immediately; later fixed steps continue through the subsystem lookup.
	if (bGenerated)
	{
		FAELSystemResolvedPlantState State;
		State.StablePointId = Snapshot.StablePointId;
		State.SpeciesId = Snapshot.SpeciesId;
		State.HealthRatio = FMath::Clamp(Snapshot.HealthRatio, 0.0f, 1.0f);
		State.LifecycleState = Snapshot.LifecycleState;
		State.LifecycleProgressRatio = FMath::Clamp(Snapshot.LifecycleProgressRatio, 0.0f, 1.0f);
		State.bVisible = Snapshot.bVisible;
		State.DeathFadeRatio = FMath::Clamp(Snapshot.DeathFadeRatio, 0.0f, 1.0f);
		State.SourceSimulationStep = Snapshot.SimulationStep;
		ApplyResolvedVisualState(State, false);
	}
	return true;
}

/* Detach one prebuilt module so its child mesh and HISM leaves fall as one rigid assembly. */
bool UAELSystemPlantComponent::BreakBranchModule(const int64 BranchModuleId)
{
	FBranchModuleRuntime* Runtime = BranchModules.Find(BranchModuleId);
	if (Runtime == nullptr || BranchModuleId == 0
		|| Runtime->StructuralState == EAEBranchStructuralState::Broken
		|| !Runtime->PhysicsRoot.IsValid())
	{
		return false;
	}

	// Persist structural loss before visual or physics state changes.
	Runtime->StructuralState = EAEBranchStructuralState::Broken;
	Runtime->bDetachedDebrisAlive = true;
	double CurrentSimulationTimeSeconds = 0.0;
	UAEAdaptiveEnvWorldSubsystem* Subsystem = nullptr;
	if (UWorld* World = GetWorld())
	{
		Subsystem = World->GetSubsystem<UAEAdaptiveEnvWorldSubsystem>();
		if (Subsystem != nullptr)
		{
			CurrentSimulationTimeSeconds = Subsystem->GetBehaviourTimeSeconds();
			Subsystem->RecordM8BrokenBranch(
				SourceStablePointId,
				RuleAsset != nullptr ? RuleAsset->SpeciesId : NAME_None,
				GetRuleContentHash(),
				GenerationSeed,
				BranchModuleId);
		}
	}
	Runtime->DetachedExpireTimeSeconds = CurrentSimulationTimeSeconds
		+ FMath::Max(static_cast<double>(DetachedBranchLifetimeSeconds), 0.0);
	UBoxComponent* PhysicsRoot = Runtime->PhysicsRoot.Get();
	PhysicsRoot->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	PhysicsRoot->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	PhysicsRoot->SetCollisionProfileName(TEXT("PhysicsActor"));
	PhysicsRoot->SetSimulatePhysics(true);
	if (Runtime->Material.IsValid())
	{
		Runtime->Material->SetScalarParameterValue(TEXT("AE_Broken"), 1.0f);
	}

	return true;
}

/* Report whether delayed release must retain this actor shell. */
bool UAELSystemPlantComponent::HasLiveDetachedBranches() const
{
	return GetLiveDetachedBranchCount() > 0;
}

/* Count detached modules whose geometry has not reached its expiry. */
int32 UAELSystemPlantComponent::GetLiveDetachedBranchCount() const
{
	int32 Count = 0;
	for (const TPair<int64, FBranchModuleRuntime>& Pair : BranchModules)
	{
		Count += Pair.Value.bDetachedDebrisAlive ? 1 : 0;
	}
	return Count;
}

/* Remove bounded expired debris without changing persistent Broken state. */
int32 UAELSystemPlantComponent::ExpireDetachedBranches(
	const double CurrentSimulationTimeSeconds,
	const int32 MaximumCleanupCount)
{
	int32 Remaining = FMath::Max(MaximumCleanupCount, 0);
	int32 RemovedCount = 0;
	TArray<int64> ModuleIds;
	BranchModules.GetKeys(ModuleIds);
	ModuleIds.Sort();
	for (const int64 ModuleId : ModuleIds)
	{
		FBranchModuleRuntime* Runtime = BranchModules.Find(ModuleId);
		if (Runtime == nullptr || !Runtime->bDetachedDebrisAlive
			|| !FAEM8PoolPolicy::IsDetachedBranchExpired(CurrentSimulationTimeSeconds, Runtime->DetachedExpireTimeSeconds)
			|| Remaining <= 0)
		{
			continue;
		}

		// Stop physics before destroying only the expired module-owned components.
		if (UHierarchicalInstancedStaticMeshComponent* LeafInstances = Runtime->LeafInstances.Get())
		{
			LeafInstanceCount -= LeafInstances->GetInstanceCount();
			OwnedLeafInstanceComponents.Remove(LeafInstances);
			DestroyOwnedComponent(LeafInstances);
		}
		if (UDynamicMeshComponent* MeshComponent = Runtime->MeshComponent.Get())
		{
			OwnedMeshComponents.Remove(MeshComponent);
			DestroyOwnedComponent(MeshComponent);
		}
		if (UBoxComponent* PhysicsRoot = Runtime->PhysicsRoot.Get())
		{
			PhysicsRoot->SetSimulatePhysics(false);
			PhysicsRoot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			OwnedPhysicsRoots.Remove(PhysicsRoot);
			DestroyOwnedComponent(PhysicsRoot);
		}
		Runtime->PhysicsRoot.Reset();
		Runtime->MeshComponent.Reset();
		Runtime->LeafInstances.Reset();
		Runtime->Material.Reset();
		Runtime->LeafMaterialInstance.Reset();
		Runtime->bDetachedDebrisAlive = false;
		Runtime->DetachedExpireTimeSeconds = 0.0;
		--Remaining;
		++RemovedCount;
	}
	return RemovedCount;
}

/* Suspend attached visuals while detached rigid bodies finish their lifetime. */
void UAELSystemPlantComponent::EnterDebrisReleaseWait()
{
	bWaitingForDebrisRelease = true;
	for (TPair<int64, FBranchModuleRuntime>& Pair : BranchModules)
	{
		FBranchModuleRuntime& Runtime = Pair.Value;
		if (!Runtime.bDetachedDebrisAlive && Runtime.MeshComponent.IsValid())
		{
			Runtime.MeshComponent->SetVisibility(false, true);
		}
		if (!Runtime.bDetachedDebrisAlive && Runtime.LeafInstances.IsValid())
		{
			Runtime.LeafInstances->SetVisibility(false, true);
		}
		if (!Runtime.bDetachedDebrisAlive && Runtime.PhysicsRoot.IsValid())
		{
			Runtime.PhysicsRoot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
}

/* Cancel delayed release without regenerating fixed geometry. */
void UAELSystemPlantComponent::CancelDebrisReleaseWait()
{
	bWaitingForDebrisRelease = false;
	ApplyResolvedVisualState(LastResolvedState, false);
}

/* Reset one actor shell only after no detached geometry remains alive. */
bool UAELSystemPlantComponent::PrepareForPool()
{
	if (!FAEM8PoolPolicy::CanReturnToAvailable(GetLiveDetachedBranchCount()))
	{
		return false;
	}
	ClearGeneratedPlant();
	SourceStablePointId = 0;
	LastResolvedState = FAELSystemResolvedPlantState();
	if (AActor* Owner = GetOwner())
	{
		Owner->SetActorEnableCollision(false);
		Owner->SetActorHiddenInGame(true);
	}
	return true;
}

/* Restore stored module loss without replaying break physics. */
bool UAELSystemPlantComponent::ApplyPersistentStructuralState(
	const FAEM8PersistentPlantState& State,
	FString& OutError)
{
	OutError.Reset();
	if (State.StablePointId != SourceStablePointId || State.RuleContentHash != GetRuleContentHash())
	{
		OutError = TEXT("Persistent M8 state does not match the bound plant topology.");
		return false;
	}
	for (const int64 ModuleId : State.BrokenBranchModuleIds)
	{
		if (FBranchModuleRuntime* Runtime = BranchModules.Find(ModuleId))
		{
			Runtime->StructuralState = EAEBranchStructuralState::Broken;
			Runtime->bDetachedDebrisAlive = false;
			if (Runtime->MeshComponent.IsValid()) Runtime->MeshComponent->SetVisibility(false, true);
			if (Runtime->LeafInstances.IsValid()) Runtime->LeafInstances->SetVisibility(false, true);
			if (Runtime->PhysicsRoot.IsValid()) Runtime->PhysicsRoot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
	for (const int64 ModuleId : State.DeadWoodBranchModuleIds)
	{
		if (FBranchModuleRuntime* Runtime = BranchModules.Find(ModuleId))
		{
			Runtime->StructuralState = EAEBranchStructuralState::DeadWood;
			Runtime->bPersistentDeathFadeLocked = true;
			if (Runtime->Material.IsValid()) Runtime->Material->SetScalarParameterValue(TEXT("AE_DeadWood"), 1.0f);
		}
	}
	return true;
}

/* Hash the current rule topology into the signed Blueprint-compatible domain. */
int64 UAELSystemPlantComponent::GetRuleContentHash() const
{
	return RuleAsset != nullptr
		? static_cast<int64>(RuleAsset->ComputeContentHash() & MAX_int64)
		: 0;
}

/* Read persistent M8 structural state without consulting recoverable M7 health. */
EAEBranchStructuralState UAELSystemPlantComponent::GetBranchModuleState(const int64 BranchModuleId) const
{
	if (const FBranchModuleRuntime* Runtime = BranchModules.Find(BranchModuleId))
	{
		return Runtime->StructuralState;
	}
	return EAEBranchStructuralState::Intact;
}

/* Consume the latest M7 snapshot or manual fallback after M7 has committed one fixed step. */
void UAELSystemPlantComponent::AdvanceM8(const UAEAdaptiveEnvWorldSubsystem& Subsystem)
{
	if (bGenerated)
	{
		ApplyResolvedVisualState(ResolvePlantState(&Subsystem), true);
	}
}

/* Resolve one source mode while preserving manual standalone behavior. */
FAELSystemResolvedPlantState UAELSystemPlantComponent::ResolvePlantState(
	const UAEAdaptiveEnvWorldSubsystem* Subsystem) const
{
	FAELSystemResolvedPlantState Result;
	Result.StablePointId = SourceStablePointId;
	Result.SpeciesId = RuleAsset != nullptr ? RuleAsset->SpeciesId : NAME_None;

	FAEPlantInstanceSnapshot Snapshot;
	const bool bCanUseM7 = InputMode != EAELSystemInputMode::Manual
		&& Subsystem != nullptr && SourceStablePointId != 0
		&& Subsystem->GetM7PlantInstanceState(SourceStablePointId, Snapshot);
	if (bCanUseM7)
	{
		Result.SpeciesId = Snapshot.SpeciesId;
		Result.HealthRatio = FMath::Clamp(Snapshot.HealthRatio, 0.0f, 1.0f);
		Result.LifecycleState = Snapshot.LifecycleState;
		Result.LifecycleProgressRatio = FMath::Clamp(Snapshot.LifecycleProgressRatio, 0.0f, 1.0f);
		Result.bVisible = Snapshot.bVisible;
		Result.DeathFadeRatio = FMath::Clamp(Snapshot.DeathFadeRatio, 0.0f, 1.0f);
		Result.SourceSimulationStep = Snapshot.SimulationStep;
		return Result;
	}
	if (InputMode == EAELSystemInputMode::M7Driven)
	{
		Result.HealthRatio = 0.0f;
		Result.LifecycleState = EAEPlantLifecycleState::Dead;
		Result.LifecycleProgressRatio = 1.0f;
		Result.bVisible = false;
		Result.DeathFadeRatio = 1.0f;
		return Result;
	}

	Result.HealthRatio = FMath::Clamp(ManualState.HealthRatio, 0.0f, 1.0f);
	Result.LifecycleState = ManualState.LifecycleState;
	Result.LifecycleProgressRatio = FMath::Clamp(ManualState.LifecycleProgressRatio, 0.0f, 1.0f);
	Result.bVisible = ManualState.bVisible;
	Result.DeathFadeRatio = FMath::Clamp(ManualState.DeathFadeRatio, 0.0f, 1.0f);
	return Result;
}

/* Convert plain module buffers into fixed Dynamic Mesh components on the Game Thread. */
bool UAELSystemPlantComponent::BuildFixedMeshComponents(FString& OutError)
{
	AActor* Owner = GetOwner();
	if (Owner == nullptr || Owner->GetRootComponent() == nullptr)
	{
		OutError = TEXT("M8 owner requires a registered root scene component.");
		return false;
	}

	TArray<int64> ModuleIds;
	GeneratedPlant.ModuleMeshes.GetKeys(ModuleIds);
	ModuleIds.Sort();
	for (const int64 ModuleId : ModuleIds)
	{
		const FAEM8MeshBuffers& Buffers = GeneratedPlant.ModuleMeshes[ModuleId];
		if (Buffers.Vertices.IsEmpty() || Buffers.Triangles.IsEmpty())
		{
			continue;
		}

		// Resolve local bounds for one coarse detachable rigid-body proxy.
		FBox Bounds(EForceInit::ForceInit);
		for (const FVector3f& Vertex : Buffers.Vertices)
		{
			Bounds += FVector(Vertex);
		}
		UBoxComponent* PhysicsRoot = NewObject<UBoxComponent>(Owner);
		PhysicsRoot->SetupAttachment(Owner->GetRootComponent());
		PhysicsRoot->SetRelativeLocation(Bounds.GetCenter());
		PhysicsRoot->SetBoxExtent(Bounds.GetExtent().ComponentMax(FVector(1.0)));
		PhysicsRoot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PhysicsRoot->SetCanEverAffectNavigation(false);
		PhysicsRoot->RegisterComponent();
		OwnedPhysicsRoots.Add(PhysicsRoot);

		// Populate one Dynamic Mesh without retaining temporary construction objects.
		UE::Geometry::FDynamicMesh3 Mesh(true, false, true, false);
		for (int32 VertexIndex = 0; VertexIndex < Buffers.Vertices.Num(); ++VertexIndex)
		{
			const int32 MeshVertexId = Mesh.AppendVertex(
				FVector3d(Buffers.Vertices[VertexIndex]) - FVector3d(Bounds.GetCenter()));
			if (Buffers.Normals.IsValidIndex(VertexIndex))
			{
				Mesh.SetVertexNormal(MeshVertexId, Buffers.Normals[VertexIndex]);
			}
			if (Buffers.UV0.IsValidIndex(VertexIndex))
			{
				Mesh.SetVertexUV(MeshVertexId, Buffers.UV0[VertexIndex]);
			}
		}
		for (const FIntVector& Triangle : Buffers.Triangles)
		{
			Mesh.AppendTriangle(Triangle.X, Triangle.Y, Triangle.Z);
		}
		UDynamicMeshComponent* MeshComponent = NewObject<UDynamicMeshComponent>(Owner);
		MeshComponent->SetupAttachment(PhysicsRoot);
		MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MeshComponent->SetCanEverAffectNavigation(false);
		MeshComponent->SetMesh(MoveTemp(Mesh));
		MeshComponent->RegisterComponent();
		OwnedMeshComponents.Add(MeshComponent);

		UMaterialInstanceDynamic* Material = nullptr;
		if (BarkMaterial != nullptr)
		{
			Material = UMaterialInstanceDynamic::Create(BarkMaterial, MeshComponent);
			MeshComponent->SetMaterial(0, Material);
		}
		FBranchModuleRuntime& Runtime = BranchModules.Add(ModuleId);
		Runtime.ModuleId = ModuleId;
		Runtime.PhysicsRoot = PhysicsRoot;
		Runtime.MeshComponent = MeshComponent;
		Runtime.Material = Material;
	}
	return !OwnedMeshComponents.IsEmpty();
}

/* Expand logical leaf regions into one HISM child per branch module. */
bool UAELSystemPlantComponent::InitializeLeafInstances(FString& OutError)
{
	if (LeafMesh == nullptr || GeneratedPlant.LeafEmitters.IsEmpty())
	{
		return true;
	}
	AActor* Owner = GetOwner();
	if (Owner == nullptr || RuleAsset == nullptr)
	{
		OutError = TEXT("M8 HISM leaves require an owner and Rule Asset.");
		return false;
	}

	// Expand all regions deterministically before creating any UObject consumers.
	TArray<FAEM8LeafInstanceDescriptor> Instances;
	FAEM8LeafInstanceBuilder::Build(
		GeneratedPlant.LeafEmitters,
		LeafDensityScale,
		LeafUniformScale,
		LeafScaleVariationRatio,
		RuleAsset->MaxLeafInstances,
		Instances);
	TMap<int64, TArray<FAEM8LeafInstanceDescriptor>> InstancesByModule;
	for (const FAEM8LeafInstanceDescriptor& Instance : Instances)
	{
		InstancesByModule.FindOrAdd(Instance.OwnerBranchModuleId).Add(Instance);
	}

	// Create one movable HISM beneath each compatible rigid-body module root.
	TArray<int64> ModuleIds;
	InstancesByModule.GetKeys(ModuleIds);
	ModuleIds.Sort();
	for (const int64 ModuleId : ModuleIds)
	{
		FBranchModuleRuntime* Runtime = BranchModules.Find(ModuleId);
		if (Runtime == nullptr || !Runtime->PhysicsRoot.IsValid())
		{
			OutError = FString::Printf(TEXT("M8 leaf instances reference missing branch module %lld."), ModuleId);
			return false;
		}
		UHierarchicalInstancedStaticMeshComponent* LeafInstances =
			NewObject<UHierarchicalInstancedStaticMeshComponent>(Owner);
		LeafInstances->SetupAttachment(Runtime->PhysicsRoot.Get());
		LeafInstances->SetMobility(EComponentMobility::Movable);
		LeafInstances->SetStaticMesh(LeafMesh);
		LeafInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		LeafInstances->SetCanEverAffectNavigation(false);
		LeafInstances->SetNumCustomDataFloats(1);
		LeafInstances->RegisterComponent();

		const FTransform ModulePlantLocalTransform = Runtime->PhysicsRoot->GetRelativeTransform();
		for (const FAEM8LeafInstanceDescriptor& Instance : InstancesByModule[ModuleId])
		{
			const FTransform ModuleLocalTransform =
				Instance.PlantLocalTransform.GetRelativeTransform(ModulePlantLocalTransform);
			const int32 InstanceIndex = LeafInstances->AddInstance(ModuleLocalTransform, false);
			LeafInstances->SetCustomDataValue(
				InstanceIndex,
				0,
				Instance.VisibilityThreshold,
				false);
		}
		LeafInstances->MarkRenderStateDirty();

		UMaterialInstanceDynamic* LeafMaterialInstance = nullptr;
		if (LeafMaterial != nullptr)
		{
			LeafMaterialInstance = UMaterialInstanceDynamic::Create(LeafMaterial, LeafInstances);
			LeafInstances->SetMaterial(0, LeafMaterialInstance);
		}
		Runtime->LeafInstances = LeafInstances;
		Runtime->LeafMaterialInstance = LeafMaterialInstance;
		OwnedLeafInstanceComponents.Add(LeafInstances);
		LeafInstanceCount += LeafInstances->GetInstanceCount();
	}
	return true;
}

/* Map M7 or manual lifecycle state to reversible visuals while preserving broken modules. */
void UAELSystemPlantComponent::ApplyResolvedVisualState(
	const FAELSystemResolvedPlantState& State,
	const bool bAllowOwnerDestruction)
{
	LastResolvedState = State;
	const float Health = FMath::Clamp(State.HealthRatio, 0.0f, 1.0f);
	const float Progress = FMath::Clamp(State.LifecycleProgressRatio, 0.0f, 1.0f);
	const float SourceDeathFade = FMath::Clamp(State.DeathFadeRatio, 0.0f, 1.0f);
	const bool bDeclining = State.LifecycleState == EAEPlantLifecycleState::Declining
		|| State.LifecycleState == EAEPlantLifecycleState::Dead;
	const bool bRecovering = State.LifecycleState == EAEPlantLifecycleState::Recovering;
	const float LeafRetention = bDeclining ? 1.0f - Progress : 1.0f;
	const float LeafRegrowth = bRecovering ? Progress : LeafRetention;
	const float WiltRatio = RuleAsset != nullptr
		&& RuleAsset->StructuralResponse == EAEPlantStructuralResponse::SoftStemWilt
		&& bDeclining
		? FMath::GetMappedRangeValueClamped(
			FVector2D(SoftWiltStartProgressRatio, 1.0f),
			FVector2D(0.0f, 1.0f),
			Progress)
		: 0.0f;
	bool bHasPersistentDeadWood = false;
	bool bAllPersistentDeadWoodFadeLocked = true;

	// Update reversible material values and preserve persistent module damage.
	for (TPair<int64, FBranchModuleRuntime>& Pair : BranchModules)
	{
		FBranchModuleRuntime& Runtime = Pair.Value;
		if (Runtime.StructuralState == EAEBranchStructuralState::Intact && Health <= DeadWoodHealthThreshold)
		{
			Runtime.StructuralState = EAEBranchStructuralState::DeadWood;
		}
		const bool bWasPersistentDeathFadeLocked = Runtime.bPersistentDeathFadeLocked;
		const float ModuleDeathFade = FAEM8MaterialPolicy::ResolveDeathFadeRatio(
			SourceDeathFade,
			Runtime.StructuralState,
			Runtime.bPersistentDeathFadeLocked);
		if (Runtime.StructuralState == EAEBranchStructuralState::DeadWood)
		{
			bHasPersistentDeadWood = true;
			bAllPersistentDeadWoodFadeLocked &= Runtime.bPersistentDeathFadeLocked;
		}
		if (!bWasPersistentDeathFadeLocked && Runtime.bPersistentDeathFadeLocked)
		{
			if (UWorld* World = GetWorld())
			{
				if (UAEAdaptiveEnvWorldSubsystem* Subsystem = World->GetSubsystem<UAEAdaptiveEnvWorldSubsystem>())
				{
					Subsystem->RecordM8DeadWoodBranch(
						SourceStablePointId,
						State.SpeciesId,
						GetRuleContentHash(),
						GenerationSeed,
						Runtime.ModuleId);
				}
			}
		}
		if (Runtime.MeshComponent.IsValid())
		{
			const bool bModuleVisible = State.bVisible
				&& (!bWaitingForDebrisRelease || Runtime.bDetachedDebrisAlive)
				&& !(Runtime.StructuralState == EAEBranchStructuralState::Broken
					&& !Runtime.bDetachedDebrisAlive);
			Runtime.MeshComponent->SetVisibility(bModuleVisible, true);
			if (Runtime.LeafInstances.IsValid())
			{
				Runtime.LeafInstances->SetVisibility(bModuleVisible, true);
			}
		}
		if (Runtime.Material.IsValid())
		{
			Runtime.Material->SetScalarParameterValue(TEXT("AE_HealthRatio"), Health);
			Runtime.Material->SetScalarParameterValue(TEXT("AE_LifecycleProgressRatio"), Progress);
			Runtime.Material->SetScalarParameterValue(TEXT("AE_DeathFadeRatio"), ModuleDeathFade);
			Runtime.Material->SetScalarParameterValue(TEXT("AE_WiltRatio"), WiltRatio);
			Runtime.Material->SetScalarParameterValue(
				TEXT("AE_DeadWood"),
				Runtime.StructuralState == EAEBranchStructuralState::DeadWood ? 1.0f : 0.0f);
		}
		if (Runtime.LeafMaterialInstance.IsValid())
		{
			Runtime.LeafMaterialInstance->SetScalarParameterValue(TEXT("AE_HealthRatio"), Health);
			Runtime.LeafMaterialInstance->SetScalarParameterValue(TEXT("AE_LifecycleProgressRatio"), Progress);
			Runtime.LeafMaterialInstance->SetScalarParameterValue(TEXT("AE_DeathFadeRatio"), ModuleDeathFade);
			Runtime.LeafMaterialInstance->SetScalarParameterValue(
				TEXT("AE_LeafRetentionRatio"),
				FMath::Clamp(LeafRegrowth, 0.0f, 1.0f));
			Runtime.LeafMaterialInstance->SetScalarParameterValue(TEXT("AE_WiltRatio"), WiltRatio);
			Runtime.LeafMaterialInstance->SetScalarParameterValue(
				TEXT("AE_DeadWood"),
				Runtime.StructuralState == EAEBranchStructuralState::DeadWood ? 1.0f : 0.0f);
			Runtime.LeafMaterialInstance->SetScalarParameterValue(
				TEXT("AE_Broken"),
				Runtime.StructuralState == EAEBranchStructuralState::Broken ? 1.0f : 0.0f);
		}
	}

	if (bAllowOwnerDestruction && !bOwnerDestroyRequested && FAEM8MaterialPolicy::ShouldDestroyOwnerAfterPersistentFade(
		bDestroyOwnerAfterPersistentDeathFade,
		bHasPersistentDeadWood,
		bAllPersistentDeadWoodFadeLocked))
	{
		bOwnerDestroyRequested = true;
		if (AActor* Owner = GetOwner())
		{
			Owner->SetActorEnableCollision(false);
			if (UWorld* World = GetWorld())
			{
				if (UAEAdaptiveEnvWorldSubsystem* Subsystem = World->GetSubsystem<UAEAdaptiveEnvWorldSubsystem>())
				{
					Subsystem->MarkM8PlantRepresentationRetired(SourceStablePointId);
					Subsystem->UnregisterLSystemPlant(this);
					Subsystem->ForgetM8ManagedActor(Owner);
				}
			}
			Owner->Destroy();
		}
	}
}

/* Destroy one runtime visual object without affecting authored owner components. */
void UAELSystemPlantComponent::DestroyOwnedComponent(UActorComponent* Component)
{
	if (IsValid(Component))
	{
		Component->DestroyComponent();
	}
}
