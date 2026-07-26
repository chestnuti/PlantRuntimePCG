#include "AEVegetationPatchComponent.h"

#include "AEPlantVisualResponseProfile.h"
#include "AEVegetationSpeciesResponseProfile.h"
#include "AdaptiveEnvSettings.h"
#include "AdaptiveEnvWorldSubsystem.h"
#include "Components/InstancedStaticMeshComponent.h"

/* Disable component Tick because the World Subsystem owns M7 scheduling. */
UAEVegetationPatchComponent::UAEVegetationPatchComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

/* Establish identity and defer registration to the next safe World tick. */
void UAEVegetationPatchComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!PatchId.IsValid())
	{
		PatchId = FGuid::NewGuid();
	}
	++RegistrationGeneration;
	if (UWorld* World = GetWorld())
	{
		if (UAEAdaptiveEnvWorldSubsystem* Subsystem =
			World->GetSubsystem<UAEAdaptiveEnvWorldSubsystem>())
		{
			Subsystem->RegisterVegetationPatch(this);
		}
	}
}

/* Remove this Patch from the World-owned state service. */
void UAEVegetationPatchComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UAEAdaptiveEnvWorldSubsystem* Subsystem =
			World->GetSubsystem<UAEAdaptiveEnvWorldSubsystem>())
		{
			Subsystem->UnregisterVegetationPatch(this);
		}
	}
	ResetVisualOutput();
	Super::EndPlay(EndPlayReason);
}

/* Freeze profile values and aggregate stable instance counts by shared Grid Cell. */
bool UAEVegetationPatchComponent::BuildPatchRegistration(
	const FIntPoint& GridDimensions,
	const FBox2D& GridWorldBounds,
	FAEVegetationPatchRegistration& OutRegistration,
	FString& OutError)
{
	check(IsInGameThread());
	OutError.Reset();
	if (!PatchId.IsValid() || !IsValid(SpeciesProfile) || !IsValid(VisualProfile)
		|| !IsValid(TargetInstances)
		|| GridDimensions.X <= 0 || GridDimensions.Y <= 0
		|| !GridWorldBounds.bIsValid)
	{
		OutError = TEXT("Patch identity, profiles, target instances, or Grid contract is invalid.");
		return false;
	}

	// Freeze the traceable species parameters before reading instance locations.
	FAEM7SpeciesParameters Parameters;
	if (!SpeciesProfile->BuildValidatedParameters(Parameters, OutError))
	{
		return false;
	}
	const FVector2D WorldSize = GridWorldBounds.GetSize();
	const FVector2D CellSize(
		WorldSize.X / GridDimensions.X,
		WorldSize.Y / GridDimensions.Y);
	if (CellSize.X <= 0.0 || CellSize.Y <= 0.0)
	{
		OutError = TEXT("Grid Cell size is invalid.");
		return false;
	}

	// Count instances per Cell to create one normalized spatial aggregation contract.
	TMap<int32, int32> CountsByCell;
	RegisteredInstanceCount = TargetInstances->GetInstanceCount();
	for (int32 InstanceIndex = 0; InstanceIndex < RegisteredInstanceCount; ++InstanceIndex)
	{
		FTransform WorldTransform;
		if (!TargetInstances->GetInstanceTransform(InstanceIndex, WorldTransform, true))
		{
			continue;
		}
		const FVector Location = WorldTransform.GetLocation();
		const FIntPoint Coordinate(
			FMath::FloorToInt((Location.X - GridWorldBounds.Min.X) / CellSize.X),
			FMath::FloorToInt((Location.Y - GridWorldBounds.Min.Y) / CellSize.Y));
		if (Coordinate.X < 0 || Coordinate.Y < 0
			|| Coordinate.X >= GridDimensions.X || Coordinate.Y >= GridDimensions.Y)
		{
			continue;
		}
		const int32 CellIndex = Coordinate.Y * GridDimensions.X + Coordinate.X;
		++CountsByCell.FindOrAdd(CellIndex);
	}
	if (CountsByCell.IsEmpty())
	{
		OutError = TEXT("No target instance lies inside the half-open shared Grid.");
		return false;
	}

	// Publish sorted normalized weights and the current registration generation.
	OutRegistration = FAEVegetationPatchRegistration();
	OutRegistration.PatchId = PatchId;
	OutRegistration.SpeciesId = SpeciesProfile->SpeciesId;
	OutRegistration.RegistrationGeneration = RegistrationGeneration;
	OutRegistration.Parameters = Parameters;
	OutRegistration.Component = this;
	TArray<int32> CellIndices;
	CountsByCell.GenerateKeyArray(CellIndices);
	CellIndices.Sort();
	int32 ValidInstanceCount = 0;
	for (const TPair<int32, int32>& Pair : CountsByCell)
	{
		ValidInstanceCount += Pair.Value;
	}
	for (const int32 CellIndex : CellIndices)
	{
		OutRegistration.WeightedCells.Add({
			CellIndex,
			static_cast<double>(CountsByCell.FindChecked(CellIndex))
				/ static_cast<double>(ValidInstanceCount)});
	}

	// Ensure both required material-facing slots are available before commands arrive.
	TargetInstances->NumCustomDataFloats = FMath::Max(
		TargetInstances->NumCustomDataFloats,
		FMath::Max(
			VisualProfile->HealthCustomDataIndex,
			VisualProfile->DensityVisibilityCustomDataIndex) + 1);
	TargetInstances->MarkRenderStateDirty();
	return true;
}

/* Keep only the newest command for the current registration generation. */
void UAEVegetationPatchComponent::EnqueueVisualCommand(
	const FAEVegetationPatchVisualCommand& Command)
{
	if (Command.PatchId != PatchId
		|| Command.Target.Get() != this
		|| Command.RegistrationGeneration != RegistrationGeneration
		|| (PendingVisualCommand.IsSet()
			&& Command.PatchStateRevision < PendingVisualCommand->PatchStateRevision))
	{
		return;
	}
	PendingVisualCommand = Command;
	NextPendingInstanceIndex = 0;
}

/* Apply a bounded stable instance range and flush render state once. */
int32 UAEVegetationPatchComponent::ApplyVisualBudget(const int32 MaxInstanceUpdates)
{
	check(IsInGameThread());
	if (!PendingVisualCommand.IsSet() || !IsValid(TargetInstances)
		|| !IsValid(VisualProfile) || MaxInstanceUpdates <= 0
		|| TargetInstances->GetInstanceCount() != RegisteredInstanceCount)
	{
		return 0;
	}

	// Update one stable contiguous range so budget pacing cannot change final selection.
	const int32 EndIndex = FMath::Min(
		NextPendingInstanceIndex + MaxInstanceUpdates,
		RegisteredInstanceCount);
	for (int32 InstanceIndex = NextPendingInstanceIndex;
		InstanceIndex < EndIndex;
		++InstanceIndex)
	{
		const bool bVisible =
			GetInstanceVisibilityKey(InstanceIndex) < PendingVisualCommand->DensityRatio;
		TargetInstances->SetCustomDataValue(
			InstanceIndex,
			VisualProfile->HealthCustomDataIndex,
			PendingVisualCommand->HealthRatio,
			false);
		TargetInstances->SetCustomDataValue(
			InstanceIndex,
			VisualProfile->DensityVisibilityCustomDataIndex,
			bVisible ? 1.0f : 0.0f,
			false);
	}
	const int32 AppliedCount = EndIndex - NextPendingInstanceIndex;
	NextPendingInstanceIndex = EndIndex;
	if (AppliedCount > 0)
	{
		TargetInstances->MarkRenderStateDirty();
	}
	if (NextPendingInstanceIndex >= RegisteredInstanceCount)
	{
		PendingVisualCommand.Reset();
		NextPendingInstanceIndex = 0;
	}
	return AppliedCount;
}

/* Discard queued visual work for teardown or reset. */
void UAEVegetationPatchComponent::ResetVisualOutput()
{
	PendingVisualCommand.Reset();
	NextPendingInstanceIndex = 0;
}

/* Hash Patch identity, stable registered index, and project seed reproducibly. */
double UAEVegetationPatchComponent::GetInstanceVisibilityKey(
	const int32 InstanceIndex) const
{
	uint32 Hash = GetTypeHash(PatchId);
	Hash = HashCombineFast(Hash, GetTypeHash(InstanceIndex));
	Hash = HashCombineFast(
		Hash,
		GetTypeHash(GetDefault<UAdaptiveEnvSettings>()->DefaultRandomSeed));
	return static_cast<double>(Hash) / (static_cast<double>(MAX_uint32) + 1.0);
}
