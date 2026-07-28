#include "AEVegetationDistributionComponent.h"

#include "AdaptiveEnvLog.h"
#include "AdaptiveEnvWorldSubsystem.h"
#include "AEPlantBiomeMapAsset.h"
#include "AEPlantDistributionService.h"
#include "AEPlantSpeciesProfile.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
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
	const FBox2D& GridBounds)
{
	CachedGridDimensions = GridDimensions;
	CachedGridBounds = GridBounds;
	RebuildStructuralDistribution();
}

/* Rebuilds only after an explicit structural-contract change. */
bool UAEVegetationDistributionComponent::RebuildStructuralDistribution()
{
	DestroyOwnedInstances();
	StablePointLookup.Reset();
	if (CachedGridDimensions.X <= 0 || CachedGridDimensions.Y <= 0 || !CachedGridBounds.bIsValid)
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
	if (Mesh == nullptr || Owner == nullptr)
	{
		return false;
	}

	const FName ComponentName(*FString::Printf(TEXT("AE_M7_%s_%d"), *Profile.SpeciesId.ToString(), SpeciesIndex));
	OutRuntime.Instances = NewObject<UHierarchicalInstancedStaticMeshComponent>(Owner, ComponentName);
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
	DistributionConfig.WorldZ = Owner->GetActorLocation().Z;
	if (!FAEPlantDistributionService::GenerateStableCandidatePool(
		DistributionConfig,
		OutRuntime.Candidates,
		Error))
	{
		UE_LOG(LogAdaptiveEnv, Warning, TEXT("M7 candidate pool rejected. Profile=%s Error=%s"), *Profile.GetPathName(), *Error);
		return false;
	}
	OutRuntime.CandidateIndicesByCell.SetNum(CachedGridDimensions.X * CachedGridDimensions.Y);
	OutRuntime.InitializedHealth.Init(false, OutRuntime.Candidates.Num());

	FRandomStream RotationRandom(DistributionSeed ^ GetTypeHash(Profile.SpeciesId) ^ 0x52A4C39D);
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

		FTransform Transform(
			FRotator(0.0f, RotationRandom.FRandRange(0.0f, 360.0f), 0.0f),
			Candidate.Location);
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
			const float DensityRatio = FMath::Pow(1.0f - FMath::Clamp(M5.DamageRatio, 0.0f, 1.0f), Profile->DensityDamageExponent);
			bool bCellTransitionActive = false;
			for (const int32 PointIndex : Runtime.CandidateIndicesByCell[CellIndex])
			{
				const FAEM7CandidatePoint& Candidate = Runtime.Candidates[PointIndex];
				FAEPlantInstanceSnapshot& Snapshot = Runtime.Snapshots[PointIndex];
				const float BiomeWeight = Profile->BiomeMap != nullptr ? Profile->BiomeMap->SampleWeight(Candidate.Location) : 1.0f;
				const float DistributionRatio = FMath::Clamp(M4.HabitatSuitabilityRatio * BiomeWeight * DensityRatio, 0.0f, 1.0f);
				const float TargetHealth = FMath::Clamp(
					1.0f - FMath::Clamp(M5.DamageRatio, 0.0f, 1.0f) + Candidate.HealthVariation,
					0.0f,
					1.0f);
				const bool bWasHealthInitialized = Runtime.InitializedHealth[PointIndex];
				const float PreviousHealth = Snapshot.HealthRatio;
				Snapshot.HealthRatio = FAEM7LifecycleModel::ResolveHealthRatio(
					PreviousHealth,
					TargetHealth,
					bWasHealthInitialized,
					DeltaSimulationHours,
					Profile->DeclineRatePerSimulationHour,
					Profile->RecoveryRatePerSimulationHour);
				Runtime.InitializedHealth[PointIndex] = true;
				const bool bAtTarget = FMath::Abs(Snapshot.HealthRatio - TargetHealth) <= Profile->StateEpsilon;
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
				if (Snapshot.LifecycleState == EAEPlantLifecycleState::Growing && Snapshot.HealthRatio >= 1.0f - Profile->StateEpsilon)
				{
					Snapshot.LifecycleState = EAEPlantLifecycleState::Stable;
				}
				Snapshot.DistributionRatio = DistributionRatio;
				Snapshot.bVisible = Candidate.SelectionKey < DistributionRatio
					&& Snapshot.LifecycleState != EAEPlantLifecycleState::Dead;
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
			FTransform Transform;
			if (Runtime.Instances->GetInstanceTransform(Index, Transform, true))
			{
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
