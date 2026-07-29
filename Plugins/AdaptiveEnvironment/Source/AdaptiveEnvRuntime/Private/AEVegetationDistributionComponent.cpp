#include "AEVegetationDistributionComponent.h"

#include "AdaptiveEnvLog.h"
#include "AdaptiveEnvWorldSubsystem.h"
#include "AEPlantBiomeMapAsset.h"
#include "AEPlantDistributionService.h"
#include "AEPlantSpeciesProfile.h"
#include "AEWorldConstraintProvider.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

/* Configures M7 as a World-subsystem-driven component. */
UAEVegetationDistributionComponent::UAEVegetationDistributionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

/* Registers this self-owned vegetation distribution with its World. */
void UAEVegetationDistributionComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UWorld* World = GetWorld())
	{
		if (UAEAdaptiveEnvWorldSubsystem* Subsystem = World->GetSubsystem<UAEAdaptiveEnvWorldSubsystem>())
		{
			Subsystem->RegisterVegetationDistribution(this);
		}
	}
}

/* Unregisters M7 and releases dynamically owned HISM components. */
void UAEVegetationDistributionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UAEAdaptiveEnvWorldSubsystem* Subsystem = World->GetSubsystem<UAEAdaptiveEnvWorldSubsystem>())
		{
			Subsystem->UnregisterVegetationDistribution(this);
		}
	}
	DestroyOwnedInstances();
	Super::EndPlay(EndPlayReason);
}

/* Freezes the shared Grid contract and builds the structural pool once. */
void UAEVegetationDistributionComponent::InitializeDistribution(
	const FIntPoint& GridDimensions,
	const FBox2D& GridBounds,
	const float GroundTraceHalfHeightCm)
{
	CachedGridDimensions = GridDimensions;
	CachedGridBounds = GridBounds;
	CachedGroundTraceHalfHeightCm = GroundTraceHalfHeightCm;
	RebuildStructuralDistribution();
}

/* Rebuilds only after an explicit structural-contract change. */
bool UAEVegetationDistributionComponent::RebuildStructuralDistribution()
{
	DestroyOwnedInstances();
	StablePointLookup.Reset();
	if (CachedGridDimensions.X <= 0 || CachedGridDimensions.Y <= 0 || !CachedGridBounds.bIsValid
		|| !FMath::IsFinite(CachedGroundTraceHalfHeightCm) || CachedGroundTraceHalfHeightCm <= 0.0f)
	{
		return false;
	}

	for (int32 SpeciesIndex = 0; SpeciesIndex < SpeciesProfiles.Num(); ++SpeciesIndex)
	{
		UAEPlantSpeciesProfile* Profile = SpeciesProfiles[SpeciesIndex];
		if (Profile == nullptr)
		{
			continue;
		}
		FSpeciesRuntime Runtime;
		if (BuildSpeciesRuntime(*Profile, Runtime, SpeciesIndex))
		{
			const int32 RuntimeIndex = SpeciesRuntime.Add(MoveTemp(Runtime));
			for (int32 PointIndex = 0; PointIndex < SpeciesRuntime[RuntimeIndex].Snapshots.Num(); ++PointIndex)
			{
				StablePointLookup.Add(
					SpeciesRuntime[RuntimeIndex].Snapshots[PointIndex].StablePointId,
					TPair<int32, int32>(RuntimeIndex, PointIndex));
			}
		}
	}
	bInitialized = SpeciesRuntime.Num() > 0;
	if (bInitialized)
	{
		// Defer upstream baseline ownership to the World subsystem after the structural pool is complete.
		if (UWorld* World = GetWorld())
		{
			if (UAEAdaptiveEnvWorldSubsystem* Subsystem = World->GetSubsystem<UAEAdaptiveEnvWorldSubsystem>())
			{
				Subsystem->RequestM7BaselineInitialization(this);
			}
		}
	}
	UE_LOG(
		LogAdaptiveEnv,
		Log,
		TEXT("M7 structural distribution built. Component=%s Species=%d Candidates=%d Seed=%d"),
		*GetPathName(),
		SpeciesRuntime.Num(),
		GetStableCandidateCount(),
		DistributionSeed);
	return bInitialized;
}

/* Creates one species HISM and its immutable candidate mapping. */
bool UAEVegetationDistributionComponent::BuildSpeciesRuntime(
	UAEPlantSpeciesProfile& Profile,
	FSpeciesRuntime& OutRuntime,
	const int32 SpeciesIndex)
{
	FString Error;
	if (!Profile.IsValidProfile(Error))
	{
		UE_LOG(LogAdaptiveEnv, Warning, TEXT("M7 species rejected. Profile=%s Error=%s"), *Profile.GetPathName(), *Error);
		return false;
	}
	UStaticMesh* Mesh = Profile.StaticMesh.LoadSynchronous();
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (Mesh == nullptr || Owner == nullptr || World == nullptr)
	{
		return false;
	}

	// Generate stable planimetric identities before any terrain-dependent filtering.
	FAEPlantDistributionConfig DistributionConfig;
	DistributionConfig.WorldBounds = CachedGridBounds;
	DistributionConfig.GridDimensions = CachedGridDimensions;
	DistributionConfig.SpeciesId = Profile.SpeciesId;
	DistributionConfig.Seed = DistributionSeed;
	DistributionConfig.MinimumSpacingCm = Profile.MinimumSpacingCm;
	DistributionConfig.MaximumInstancesPerSquareMeter = Profile.MaximumInstancesPerSquareMeter;
	DistributionConfig.AttemptsPerExpectedPoint = Profile.PoissonAttemptsPerExpectedPoint;
	DistributionConfig.MaximumCandidateCount = MaxCandidatesPerSpecies;
	DistributionConfig.HealthVariationAmplitude = Profile.HealthVariationAmplitude;
	if (!FAEPlantDistributionService::GenerateStableCandidatePool(
		DistributionConfig,
		OutRuntime.Candidates,
		Error))
	{
		UE_LOG(LogAdaptiveEnv, Warning, TEXT("M7 candidate pool rejected. Profile=%s Error=%s"), *Profile.GetPathName(), *Error);
		return false;
	}

	// Project each stable XY point onto approved ecological ground without changing its identity.
	TArray<FAEM7CandidatePoint> ProjectedCandidates;
	ProjectedCandidates.Reserve(OutRuntime.Candidates.Num());
	int32 RejectedGroundCount = 0;
	const double TraceCenterZ = Owner->GetActorLocation().Z;
	for (FAEM7CandidatePoint& Candidate : OutRuntime.Candidates)
	{
		FAEGroundSurfaceSample Ground;
		const FVector TraceCenter(Candidate.Location.X, Candidate.Location.Y, TraceCenterZ);
		if (!FAEWorldConstraintProvider::TraceGroundSurface(
			*World,
			TraceCenter,
			CachedGroundTraceHalfHeightCm,
			Owner,
			Ground))
		{
			++RejectedGroundCount;
			continue;
		}
		Candidate.SurfaceNormal = Ground.WorldNormal;
		Candidate.Location = Ground.WorldLocation + Ground.WorldNormal * Profile.GroundOffsetCm;
		ProjectedCandidates.Add(MoveTemp(Candidate));
	}
	OutRuntime.Candidates = MoveTemp(ProjectedCandidates);
	UE_LOG(
		LogAdaptiveEnv,
		Log,
		TEXT("M7 ground projection completed. Component=%s Species=%s Projected=%d RejectedNoGround=%d"),
		*GetPathName(),
		*Profile.SpeciesId.ToString(),
		OutRuntime.Candidates.Num(),
		RejectedGroundCount);
	if (OutRuntime.Candidates.IsEmpty())
	{
		UE_LOG(
			LogAdaptiveEnv,
			Warning,
			TEXT("M7 species has no candidates on approved ground. Profile=%s"),
			*Profile.GetPathName());
		return false;
	}

	// Create the species HISM only after structural projection succeeds.
	const FName ComponentName(*FString::Printf(TEXT("AE_M7_%s_%d"), *Profile.SpeciesId.ToString(), SpeciesIndex));
	OutRuntime.Instances = NewObject<UHierarchicalInstancedStaticMeshComponent>(Owner, ComponentName);
	if (OutRuntime.Instances == nullptr)
	{
		return false;
	}
	if (Owner->GetRootComponent() != nullptr)
	{
		OutRuntime.Instances->SetupAttachment(Owner->GetRootComponent());
	}
	OutRuntime.Instances->SetStaticMesh(Mesh);
	OutRuntime.Instances->NumCustomDataFloats = 5;
	// Apply the species collision contract before registration creates runtime physics state.
	OutRuntime.Instances->SetCollisionProfileName(Profile.CollisionProfileName, false);
	OutRuntime.Instances->SetCollisionEnabled(Profile.CollisionEnabled);
	OutRuntime.Instances->SetGenerateOverlapEvents(
		Profile.bGenerateOverlapEvents && Profile.CollisionEnabled != ECollisionEnabled::NoCollision);
	OutRuntime.Instances->SetCanEverAffectNavigation(Profile.bCanEverAffectNavigation);
	Owner->AddInstanceComponent(OutRuntime.Instances);
	OutRuntime.Instances->RegisterComponent();
	OutRuntime.Profile = &Profile;

	OutRuntime.CandidateIndicesByCell.SetNum(CachedGridDimensions.X * CachedGridDimensions.Y);
	OutRuntime.InitializedHealth.Init(false, OutRuntime.Candidates.Num());
	OutRuntime.BaseWorldTransforms.Reserve(OutRuntime.Candidates.Num());

	// Build HISM indices in stable candidate order with identity-derived yaw.
	for (int32 CandidateIndex = 0; CandidateIndex < OutRuntime.Candidates.Num(); ++CandidateIndex)
	{
		const FAEM7CandidatePoint& Candidate = OutRuntime.Candidates[CandidateIndex];
		FAEPlantInstanceSnapshot& Snapshot = OutRuntime.Snapshots.AddDefaulted_GetRef();
		Snapshot.StablePointId = static_cast<int64>(Candidate.StablePointId);
		Snapshot.SpeciesId = Profile.SpeciesId;
		Snapshot.CellCoordinate = Candidate.CellCoordinate;
		Snapshot.WorldLocation = Candidate.Location;
		Snapshot.HealthRatio = 0.0f;
		Snapshot.LifecycleState = EAEPlantLifecycleState::Growing;
		OutRuntime.CandidateIndicesByCell[CoordinateToIndex(Candidate.CellCoordinate)].Add(CandidateIndex);
		OutRuntime.DistributionDirtyCells.Add(CoordinateToIndex(Candidate.CellCoordinate));

		const float StableYawDegrees = FAEPlantDistributionService::HashToUnitFloat(
			FAEPlantDistributionService::MixHash(Candidate.StablePointId ^ 0x52A4C39D6E8F10B5ull))
			* 360.0f;
		const FVector YawAxis = Profile.bAlignToGroundNormal
			? Candidate.SurfaceNormal
			: FVector::UpVector;
		const FQuat SurfaceAlignment = Profile.bAlignToGroundNormal
			? FQuat::FindBetweenNormals(FVector::UpVector, Candidate.SurfaceNormal)
			: FQuat::Identity;
		const FQuat StableYaw(YawAxis, FMath::DegreesToRadians(StableYawDegrees));
		FTransform Transform(StableYaw * SurfaceAlignment, Candidate.Location);
		OutRuntime.BaseWorldTransforms.Add(Transform);
		// Keep new instances hidden until their authoritative M4/M5 baseline is evaluated.
		Transform.SetScale3D(FVector::ZeroVector);
		OutRuntime.Instances->AddInstance(Transform, true);
	}
	return OutRuntime.Candidates.Num() > 0;
}

/* Advances the union of source Dirty, distribution Dirty, and active transitions. */
void UAEVegetationDistributionComponent::AdvanceM7(
	const UAEAdaptiveEnvWorldSubsystem& Subsystem,
	const TArray<int32>& M5DirtyCellIndices,
	const TArray<int32>& DistributionDirtyCellIndices,
	const double DeltaSimulationHours,
	const int64 SimulationStep)
{
	if (!bInitialized)
	{
		return;
	}
	for (FSpeciesRuntime& Runtime : SpeciesRuntime)
	{
		UAEPlantSpeciesProfile* Profile = Runtime.Profile.Get();
		if (Profile == nullptr)
		{
			continue;
		}
		TSet<int32> CandidateCells = Runtime.ActiveTransitionCells;
		CandidateCells.Append(Runtime.DistributionDirtyCells);
		for (const int32 CellIndex : M5DirtyCellIndices)
		{
			CandidateCells.Add(CellIndex);
		}
		for (const int32 CellIndex : DistributionDirtyCellIndices)
		{
			CandidateCells.Add(CellIndex);
		}
		Runtime.ActiveTransitionCells.Reset();
		Runtime.DistributionDirtyCells.Reset();

		TArray<int32> OrderedCells = CandidateCells.Array();
		OrderedCells.Sort();
		for (const int32 CellIndex : OrderedCells)
		{
			if (!Runtime.CandidateIndicesByCell.IsValidIndex(CellIndex))
			{
				continue;
			}
			const FIntPoint Coordinate(CellIndex % CachedGridDimensions.X, CellIndex / CachedGridDimensions.X);
			FAEEnvironmentConstraintSnapshot M4;
			FAEEcologicalResponseSnapshot M5;
			// Unsampled upstream cells represent intact baseline habitat until M4/M5 commits them.
			const bool bHasM4 = Subsystem.GetM4Cell(Coordinate, M4);
			const bool bHasM5 = Subsystem.GetM5Cell(Coordinate, M5);
			if (!bHasM4) M4.HabitatSuitabilityRatio = 1.0f;
			if (!bHasM5) M5.DamageRatio = 0.0f;
			bool bCellTransitionActive = false;
			for (const int32 PointIndex : Runtime.CandidateIndicesByCell[CellIndex])
			{
				const FAEM7CandidatePoint& Candidate = Runtime.Candidates[PointIndex];
				FAEPlantInstanceSnapshot& Snapshot = Runtime.Snapshots[PointIndex];
				const float BiomeWeight = Profile->BiomeMap != nullptr ? Profile->BiomeMap->SampleWeight(Candidate.Location) : 1.0f;
				const float DistributionRatio = FMath::Clamp(M4.HabitatSuitabilityRatio * BiomeWeight, 0.0f, 1.0f);
				const bool bStructurallyEligible = Candidate.SelectionKey < DistributionRatio;
				const float TargetHealth = FMath::Clamp(
					1.0f - FMath::Clamp(M5.DamageRatio, 0.0f, 1.0f) + Candidate.HealthVariation,
					0.0f,
					1.0f);
				const bool bWasHealthInitialized = Runtime.InitializedHealth[PointIndex];
				const float PreviousHealth = Snapshot.HealthRatio;
				const bool bPreviouslyVisible = Snapshot.bVisible;
				Snapshot.HealthRatio = FAEM7LifecycleModel::ResolveHealthRatio(
					PreviousHealth,
					TargetHealth,
					bWasHealthInitialized,
					DeltaSimulationHours,
					Profile->DeclineRatePerSimulationHour,
					Profile->RecoveryRatePerSimulationHour);
				Runtime.InitializedHealth[PointIndex] = true;
				const bool bAtTarget = FMath::IsNearlyEqual(
					Snapshot.HealthRatio,
					TargetHealth,
					UE_KINDA_SMALL_NUMBER);
				if (!bWasHealthInitialized)
				{
					Snapshot.LifecycleState = FAEM7LifecycleModel::ResolveInitialState(
						Snapshot.HealthRatio,
						Profile->DeadHealthThreshold);
				}
				else if (Snapshot.HealthRatio <= Profile->DeadHealthThreshold)
				{
					Snapshot.LifecycleState = EAEPlantLifecycleState::Dead;
				}
				else if (!bAtTarget && TargetHealth < PreviousHealth)
				{
					Snapshot.LifecycleState = EAEPlantLifecycleState::Declining;
				}
				else if (!bAtTarget && Snapshot.LifecycleState == EAEPlantLifecycleState::Growing
					&& TargetHealth >= PreviousHealth)
				{
					Snapshot.LifecycleState = EAEPlantLifecycleState::Growing;
				}
				else if (!bAtTarget && TargetHealth > PreviousHealth)
				{
					Snapshot.LifecycleState = EAEPlantLifecycleState::Recovering;
				}
				else
				{
					Snapshot.LifecycleState =
						Snapshot.LifecycleState == EAEPlantLifecycleState::Growing
						? EAEPlantLifecycleState::Growing
						: EAEPlantLifecycleState::Stable;
				}
				if (Snapshot.LifecycleState == EAEPlantLifecycleState::Growing
					&& Snapshot.HealthRatio >= 1.0f - UE_KINDA_SMALL_NUMBER)
				{
					Snapshot.LifecycleState = EAEPlantLifecycleState::Stable;
				}
				Snapshot.DistributionRatio = DistributionRatio;
				Snapshot.bVisible = FAEM7LifecycleModel::ResolveVisibility(
					bStructurallyEligible,
					bPreviouslyVisible,
					bWasHealthInitialized,
					Snapshot.HealthRatio,
					Profile->DeadHealthThreshold,
					Profile->StateEpsilon);
				Snapshot.LifecycleProgressRatio = FMath::Clamp(
					bWasHealthInitialized
						? FMath::Abs(Snapshot.HealthRatio - PreviousHealth) / FMath::Max(Profile->StateEpsilon, 0.000001f)
						: 0.0f,
					0.0f,
					1.0f);
				Snapshot.SimulationStep = SimulationStep;
				Runtime.PendingVisualIndices.Add(PointIndex);
				bCellTransitionActive |= !bAtTarget;
			}
			if (bCellTransitionActive)
			{
				Runtime.ActiveTransitionCells.Add(CellIndex);
			}
		}
	}
}

/* Writes queued custom data and visibility transforms within one frame budget. */
void UAEVegetationDistributionComponent::ApplyVisualBudget(const int32 MaximumUpdates)
{
	int32 Remaining = FMath::Max(MaximumUpdates, 0);
	for (FSpeciesRuntime& Runtime : SpeciesRuntime)
	{
		if (Runtime.Instances == nullptr || Remaining <= 0)
		{
			break;
		}
		TArray<int32> Ordered = Runtime.PendingVisualIndices.Array();
		Ordered.Sort();
		for (const int32 Index : Ordered)
		{
			if (Remaining-- <= 0)
			{
				break;
			}
			const FAEPlantInstanceSnapshot& Snapshot = Runtime.Snapshots[Index];
			Runtime.Instances->SetCustomDataValue(Index, 0, Snapshot.HealthRatio, false);
			Runtime.Instances->SetCustomDataValue(Index, 1, Snapshot.bVisible ? 1.0f : 0.0f, false);
			Runtime.Instances->SetCustomDataValue(Index, 2, static_cast<float>(Snapshot.LifecycleState) / 4.0f, false);
			Runtime.Instances->SetCustomDataValue(Index, 3, Snapshot.LifecycleProgressRatio, false);
			Runtime.Instances->SetCustomDataValue(Index, 4, Snapshot.DistributionRatio, false);
			if (Runtime.BaseWorldTransforms.IsValidIndex(Index))
			{
				FTransform Transform = Runtime.BaseWorldTransforms[Index];
				Transform.SetScale3D(Snapshot.bVisible ? FVector::OneVector : FVector::ZeroVector);
				Runtime.Instances->UpdateInstanceTransform(Index, Transform, true, false, true);
			}
			Runtime.PendingVisualIndices.Remove(Index);
		}
		Runtime.Instances->MarkRenderStateDirty();
	}
}

/* Resolves one stable point into an immutable M8-facing snapshot. */
bool UAEVegetationDistributionComponent::GetPlantInstanceState(
	const int64 StablePointId,
	FAEPlantInstanceSnapshot& OutSnapshot) const
{
	const TPair<int32, int32>* Location = StablePointLookup.Find(StablePointId);
	if (Location == nullptr || !SpeciesRuntime.IsValidIndex(Location->Key)
		|| !SpeciesRuntime[Location->Key].Snapshots.IsValidIndex(Location->Value))
	{
		return false;
	}
	OutSnapshot = SpeciesRuntime[Location->Key].Snapshots[Location->Value];
	return true;
}

/* Counts immutable candidates across all configured species. */
int32 UAEVegetationDistributionComponent::GetStableCandidateCount() const
{
	int32 Count = 0;
	for (const FSpeciesRuntime& Runtime : SpeciesRuntime)
	{
		Count += Runtime.Candidates.Num();
	}
	return Count;
}

/* Collects stable row-major Cell indices without exposing per-species storage. */
void UAEVegetationDistributionComponent::GetOccupiedCellIndices(TArray<int32>& OutCellIndices) const
{
	TSet<int32> UniqueIndices;
	for (const FSpeciesRuntime& Runtime : SpeciesRuntime)
	{
		TArray<int32> SpeciesCellIndices;
		FAEPlantDistributionService::CollectOccupiedCellIndices(
			Runtime.Candidates,
			CachedGridDimensions,
			SpeciesCellIndices);
		for (const int32 CellIndex : SpeciesCellIndices)
		{
			UniqueIndices.Add(CellIndex);
		}
	}
	OutCellIndices = UniqueIndices.Array();
	OutCellIndices.Sort();
}

/* Destroys only HISM components created and owned by this component. */
void UAEVegetationDistributionComponent::DestroyOwnedInstances()
{
	for (FSpeciesRuntime& Runtime : SpeciesRuntime)
	{
		if (Runtime.Instances != nullptr)
		{
			Runtime.Instances->DestroyComponent();
		}
	}
	SpeciesRuntime.Reset();
	bInitialized = false;
}

/* Converts one valid Cell coordinate into row-major storage. */
int32 UAEVegetationDistributionComponent::CoordinateToIndex(const FIntPoint& Coordinate) const
{
	return Coordinate.Y * CachedGridDimensions.X + Coordinate.X;
}
