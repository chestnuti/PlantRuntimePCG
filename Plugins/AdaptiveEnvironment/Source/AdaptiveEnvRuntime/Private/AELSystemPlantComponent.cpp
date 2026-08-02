#include "AELSystemPlantComponent.h"

#include "AdaptiveEnvLog.h"
#include "AdaptiveEnvWorldSubsystem.h"
#include "AELSystemGenerator.h"
#include "AELSystemRuleAsset.h"
#include "Components/BoxComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "NiagaraSystem.h"

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
	InitializeLeafSystem();
	bGenerated = true;
	ApplyResolvedVisualState(ResolvePlantState(GetWorld()->GetSubsystem<UAEAdaptiveEnvWorldSubsystem>()));
	return true;
}

/* Destroy every visual owner and reset generated logical state. */
void UAELSystemPlantComponent::ClearGeneratedPlant()
{
	if (LeafComponent != nullptr)
	{
		DestroyOwnedComponent(LeafComponent);
		LeafComponent = nullptr;
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
	BranchModules.Reset();
	GeneratedPlant = FAELSystemGeneratedPlant();
	bGenerated = false;
	bWaitingForDebrisRelease = false;
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
		State.SourceSimulationStep = Snapshot.SimulationStep;
		ApplyResolvedVisualState(State);
	}
	return true;
}

/* Detach one prebuilt module, enable its simplified rigid body, and shed every attached leaf. */
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

	// Tell Niagara to convert all attached leaves to free-fall particles.
	if (LeafComponent != nullptr)
	{
		LeafComponent->SetVariableInt(TEXT("AE_BrokenBranchModuleId"), static_cast<int32>(BranchModuleId));
		LeafComponent->SetVariableFloat(TEXT("AE_DetachAllLeaves"), 1.0f);
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
		Runtime->Material.Reset();
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
		if (!Runtime.bDetachedDebrisAlive && Runtime.PhysicsRoot.IsValid())
		{
			Runtime.PhysicsRoot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
	if (LeafComponent != nullptr)
	{
		LeafComponent->Deactivate();
		LeafComponent->SetVisibility(false, true);
	}
}

/* Cancel delayed release without regenerating fixed geometry. */
void UAELSystemPlantComponent::CancelDebrisReleaseWait()
{
	bWaitingForDebrisRelease = false;
	ApplyResolvedVisualState(LastResolvedState);
	if (LeafComponent != nullptr)
	{
		LeafComponent->Activate();
	}
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

/* Restore stored module loss without replaying break physics or Niagara events. */
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
			if (Runtime->PhysicsRoot.IsValid()) Runtime->PhysicsRoot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
	for (const int64 ModuleId : State.DeadWoodBranchModuleIds)
	{
		if (FBranchModuleRuntime* Runtime = BranchModules.Find(ModuleId))
		{
			Runtime->StructuralState = EAEBranchStructuralState::DeadWood;
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
		ApplyResolvedVisualState(ResolvePlantState(&Subsystem));
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
		Result.SourceSimulationStep = Snapshot.SimulationStep;
		return Result;
	}
	if (InputMode == EAELSystemInputMode::M7Driven)
	{
		Result.HealthRatio = 0.0f;
		Result.LifecycleState = EAEPlantLifecycleState::Dead;
		Result.LifecycleProgressRatio = 1.0f;
		Result.bVisible = false;
		return Result;
	}

	Result.HealthRatio = FMath::Clamp(ManualState.HealthRatio, 0.0f, 1.0f);
	Result.LifecycleState = ManualState.LifecycleState;
	Result.LifecycleProgressRatio = FMath::Clamp(ManualState.LifecycleProgressRatio, 0.0f, 1.0f);
	Result.bVisible = ManualState.bVisible;
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

/* Create one per-plant Niagara consumer and publish stable emitter aggregates. */
void UAELSystemPlantComponent::InitializeLeafSystem()
{
	if (LeafSystem == nullptr || GetOwner() == nullptr || GetOwner()->GetRootComponent() == nullptr)
	{
		return;
	}
	LeafComponent = NewObject<UNiagaraComponent>(GetOwner());
	LeafComponent->SetupAttachment(GetOwner()->GetRootComponent());
	LeafComponent->SetAsset(LeafSystem);
	LeafComponent->SetAutoActivate(true);
	LeafComponent->RegisterComponent();
	LeafComponent->SetVariableInt(TEXT("AE_LeafEmitterCount"), GeneratedPlant.LeafEmitters.Num());
	LeafComponent->SetVariableInt(TEXT("AE_GenerationSeed"), GenerationSeed);
	LeafComponent->SetVariableFloat(TEXT("AE_DetachAllLeaves"), 0.0f);

	// Publish stable emitter arrays consumed by the authored Niagara system.
	TArray<FVector> Positions;
	TArray<FQuat> Rotations;
	TArray<float> LengthsCm;
	TArray<float> RadiiCm;
	TArray<float> DensitiesPerMeter;
	TArray<int32> ModuleIds;
	TArray<int32> Seeds;
	Positions.Reserve(GeneratedPlant.LeafEmitters.Num());
	Rotations.Reserve(GeneratedPlant.LeafEmitters.Num());
	LengthsCm.Reserve(GeneratedPlant.LeafEmitters.Num());
	RadiiCm.Reserve(GeneratedPlant.LeafEmitters.Num());
	DensitiesPerMeter.Reserve(GeneratedPlant.LeafEmitters.Num());
	ModuleIds.Reserve(GeneratedPlant.LeafEmitters.Num());
	Seeds.Reserve(GeneratedPlant.LeafEmitters.Num());
	for (const FAELeafEmitterDescriptor& Emitter : GeneratedPlant.LeafEmitters)
	{
		Positions.Add(Emitter.LocalTransform.GetLocation());
		Rotations.Add(Emitter.LocalTransform.GetRotation());
		LengthsCm.Add(Emitter.EmitterLengthCm);
		RadiiCm.Add(Emitter.EmitterRadiusCm);
		DensitiesPerMeter.Add(Emitter.DensityPerMeter);
		ModuleIds.Add(static_cast<int32>(Emitter.OwnerBranchModuleId));
		Seeds.Add(Emitter.Seed);
	}
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayPosition(
		LeafComponent, TEXT("AE_EmitterPositions"), Positions);
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayQuat(
		LeafComponent, TEXT("AE_EmitterRotations"), Rotations);
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayFloat(
		LeafComponent, TEXT("AE_EmitterLengthsCm"), LengthsCm);
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayFloat(
		LeafComponent, TEXT("AE_EmitterRadiiCm"), RadiiCm);
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayFloat(
		LeafComponent, TEXT("AE_EmitterDensitiesPerMeter"), DensitiesPerMeter);
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayInt32(
		LeafComponent, TEXT("AE_EmitterModuleIds"), ModuleIds);
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayInt32(
		LeafComponent, TEXT("AE_EmitterSeeds"), Seeds);
}

/* Map M7 or manual lifecycle state to reversible visuals while preserving broken modules. */
void UAELSystemPlantComponent::ApplyResolvedVisualState(const FAELSystemResolvedPlantState& State)
{
	LastResolvedState = State;
	const float Health = FMath::Clamp(State.HealthRatio, 0.0f, 1.0f);
	const float Progress = FMath::Clamp(State.LifecycleProgressRatio, 0.0f, 1.0f);
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

	// Update reversible material values and preserve persistent module damage.
	for (TPair<int64, FBranchModuleRuntime>& Pair : BranchModules)
	{
		FBranchModuleRuntime& Runtime = Pair.Value;
		if (Runtime.StructuralState == EAEBranchStructuralState::Intact && Health <= DeadWoodHealthThreshold)
		{
			Runtime.StructuralState = EAEBranchStructuralState::DeadWood;
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
				&& !(Runtime.StructuralState == EAEBranchStructuralState::Broken
					&& !Runtime.bDetachedDebrisAlive);
			Runtime.MeshComponent->SetVisibility(bModuleVisible, true);
		}
		if (Runtime.Material.IsValid())
		{
			Runtime.Material->SetScalarParameterValue(TEXT("AE_HealthRatio"), Health);
			Runtime.Material->SetScalarParameterValue(TEXT("AE_LifecycleProgressRatio"), Progress);
			Runtime.Material->SetScalarParameterValue(TEXT("AE_WiltRatio"), WiltRatio);
			Runtime.Material->SetScalarParameterValue(
				TEXT("AE_DeadWood"),
				Runtime.StructuralState == EAEBranchStructuralState::DeadWood ? 1.0f : 0.0f);
		}
	}

	// Publish aggregate leaf controls; individual leaf replay remains deliberately unspecified.
	if (LeafComponent != nullptr)
	{
		LeafComponent->SetVisibility(State.bVisible, true);
		LeafComponent->SetVariableFloat(TEXT("AE_HealthRatio"), Health);
		LeafComponent->SetVariableFloat(TEXT("AE_LeafDensityScale"), LeafDensityScale * FMath::Clamp(LeafRegrowth, 0.0f, 1.0f));
		LeafComponent->SetVariableFloat(TEXT("AE_SheddingRatio"), bDeclining ? Progress : 0.0f);
		LeafComponent->SetVariableFloat(TEXT("AE_WiltRatio"), WiltRatio);
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
