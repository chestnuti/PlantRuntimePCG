#include "AEVegetationPatchComponent.h"

#include "AEPlantVisualResponseProfile.h"
#include "AEVegetationSpeciesResponseProfile.h"
#include "AdaptiveEnvLog.h"
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
	ResolvedTargetInstances = nullptr;
	Super::EndPlay(EndPlayReason);
}

/* Validate and retain one explicit same-Actor runtime target. */
bool UAEVegetationPatchComponent::SetTargetInstancesComponent(
	UInstancedStaticMeshComponent* InTargetInstances)
{
	check(IsInGameThread());
	if (!IsValid(InTargetInstances)
		|| GetOwner() == nullptr
		|| InTargetInstances->GetOwner() != GetOwner())
	{
		UE_LOG(
			LogAdaptiveEnv,
			Warning,
			TEXT("M7 target assignment rejected. PatchOwner=%s TargetOwner=%s Target=%s."),
			*GetPathNameSafe(GetOwner()),
			*GetPathNameSafe(
				IsValid(InTargetInstances)
					? InTargetInstances->GetOwner()
					: nullptr),
			*GetPathNameSafe(InTargetInstances));
		return false;
	}

	ResolvedTargetInstances = InTargetInstances;
	UE_LOG(
		LogAdaptiveEnv,
		Log,
		TEXT("M7 target assigned. Patch=%s Component=%s ObjectId=%u Instances=%d."),
		*PatchId.ToString(EGuidFormats::DigitsWithHyphens),
		*GetPathNameSafe(ResolvedTargetInstances),
		ResolvedTargetInstances->GetUniqueID(),
		ResolvedTargetInstances->GetInstanceCount());
	return true;
}

/* Return only the legacy serialized pointer for existing Blueprint nodes. */
UInstancedStaticMeshComponent*
UAEVegetationPatchComponent::GetDeprecatedTargetInstances() const
{
	return TargetInstances_DEPRECATED;
}

/* Preserve legacy Blueprint writes while requiring the new API for registration. */
void UAEVegetationPatchComponent::SetDeprecatedTargetInstances(
	UInstancedStaticMeshComponent* InTargetInstances)
{
	TargetInstances_DEPRECATED = InTargetInstances;
}

/* Queue one safe remove-and-add cycle after instances or profiles change. */
void UAEVegetationPatchComponent::RefreshPatchRegistration()
{
	check(IsInGameThread());
	if (!HasBegunPlay())
	{
		UE_LOG(
			LogAdaptiveEnv,
			Verbose,
			TEXT("M7 refresh ignored before BeginPlay. Patch=%s."),
			*GetPathNameSafe(this));
		return;
	}

	// Invalidate commands from the previous spatial registration.
	++RegistrationGeneration;
	ResetVisualOutput();

	// Defer state mutation to the World scheduler registration boundary.
	if (UWorld* World = GetWorld())
	{
		if (UAEAdaptiveEnvWorldSubsystem* Subsystem =
			World->GetSubsystem<UAEAdaptiveEnvWorldSubsystem>())
		{
			Subsystem->RefreshVegetationPatch(this);
		}
	}
}

/* Resolve a same-Actor runtime override before the serialized component reference. */
bool UAEVegetationPatchComponent::ResolveTargetInstances(FString& OutError)
{
	OutError.Reset();
	AActor* Owner = GetOwner();
	if (Owner == nullptr)
	{
		OutError = TEXT("Patch has no owning Actor.");
		return false;
	}

	// Reuse a valid runtime override only while it remains owned by this Actor.
	if (IsValid(ResolvedTargetInstances))
	{
		if (ResolvedTargetInstances->GetOwner() == Owner)
		{
			return true;
		}
		OutError = FString::Printf(
			TEXT("Resolved target belongs to another Actor. PatchOwner=%s ")
			TEXT("TargetOwner=%s Target=%s."),
			*GetPathNameSafe(Owner),
			*GetPathNameSafe(ResolvedTargetInstances->GetOwner()),
			*GetPathNameSafe(ResolvedTargetInstances));
		ResolvedTargetInstances = nullptr;
		return false;
	}

	// Resolve the editor-authored sibling component without accepting inline objects.
	UActorComponent* ReferencedComponent =
		TargetInstancesReference.GetComponent(Owner);
	ResolvedTargetInstances =
		Cast<UInstancedStaticMeshComponent>(ReferencedComponent);
	if (!IsValid(ResolvedTargetInstances))
	{
		OutError = FString::Printf(
			TEXT("TargetInstancesReference did not resolve to an ISM/HISM. Owner=%s."),
			*GetPathNameSafe(Owner));
		return false;
	}
	if (ResolvedTargetInstances->GetOwner() != Owner)
	{
		OutError = FString::Printf(
			TEXT("Resolved target is not owned by Patch Actor. PatchOwner=%s ")
			TEXT("TargetOwner=%s Target=%s."),
			*GetPathNameSafe(Owner),
			*GetPathNameSafe(ResolvedTargetInstances->GetOwner()),
			*GetPathNameSafe(ResolvedTargetInstances));
		ResolvedTargetInstances = nullptr;
		return false;
	}
	return true;
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
		|| GridDimensions.X <= 0 || GridDimensions.Y <= 0
		|| !GridWorldBounds.bIsValid)
	{
		OutError = TEXT("Patch identity, profiles, target instances, or Grid contract is invalid.");
		return false;
	}
	if (!ResolveTargetInstances(OutError))
	{
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

	// Count readable and in-bounds instances without emitting per-instance diagnostics.
	TMap<int32, int32> CountsByCell;
	RegisteredInstanceCount = ResolvedTargetInstances->GetInstanceCount();
	int32 ReadableTransformCount = 0;
	int32 InGridInstanceCount = 0;
	bool bHasFirstReadableLocation = false;
	FVector FirstReadableWorldLocation = FVector::ZeroVector;
	for (int32 InstanceIndex = 0; InstanceIndex < RegisteredInstanceCount; ++InstanceIndex)
	{
		FTransform WorldTransform;
		if (!ResolvedTargetInstances->GetInstanceTransform(
			InstanceIndex,
			WorldTransform,
			true))
		{
			continue;
		}
		++ReadableTransformCount;
		const FVector Location = WorldTransform.GetLocation();
		if (!bHasFirstReadableLocation)
		{
			FirstReadableWorldLocation = Location;
			bHasFirstReadableLocation = true;
		}
		const FIntPoint Coordinate(
			FMath::FloorToInt((Location.X - GridWorldBounds.Min.X) / CellSize.X),
			FMath::FloorToInt((Location.Y - GridWorldBounds.Min.Y) / CellSize.Y));
		if (Coordinate.X < 0 || Coordinate.Y < 0
			|| Coordinate.X >= GridDimensions.X || Coordinate.Y >= GridDimensions.Y)
		{
			continue;
		}
		++InGridInstanceCount;
		const int32 CellIndex = Coordinate.Y * GridDimensions.X + Coordinate.X;
		++CountsByCell.FindOrAdd(CellIndex);
	}
	if (RegisteredInstanceCount == 0)
	{
		OutError = FString::Printf(
			TEXT("Target component has zero instances. Component=%s ObjectId=%u."),
			*GetPathNameSafe(ResolvedTargetInstances),
			ResolvedTargetInstances->GetUniqueID());
		return false;
	}
	if (ReadableTransformCount == 0)
	{
		OutError = FString::Printf(
			TEXT("No instance transform is readable. Component=%s ObjectId=%u Total=%d."),
			*GetPathNameSafe(ResolvedTargetInstances),
			ResolvedTargetInstances->GetUniqueID(),
			RegisteredInstanceCount);
		return false;
	}
	if (InGridInstanceCount == 0)
	{
		OutError = FString::Printf(
			TEXT("All readable instances are outside Grid. Component=%s ObjectId=%u ")
			TEXT("Total=%d Readable=%d FirstWorld=(%.2f,%.2f,%.2f) ")
			TEXT("GridMin=(%.2f,%.2f) GridMax=(%.2f,%.2f) Dimensions=(%d,%d)."),
			*GetPathNameSafe(ResolvedTargetInstances),
			ResolvedTargetInstances->GetUniqueID(),
			RegisteredInstanceCount,
			ReadableTransformCount,
			FirstReadableWorldLocation.X,
			FirstReadableWorldLocation.Y,
			FirstReadableWorldLocation.Z,
			GridWorldBounds.Min.X,
			GridWorldBounds.Min.Y,
			GridWorldBounds.Max.X,
			GridWorldBounds.Max.Y,
			GridDimensions.X,
			GridDimensions.Y);
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
	ResolvedTargetInstances->NumCustomDataFloats = FMath::Max(
		ResolvedTargetInstances->NumCustomDataFloats,
		FMath::Max(
			VisualProfile->HealthCustomDataIndex,
			VisualProfile->DensityVisibilityCustomDataIndex) + 1);
	ResolvedTargetInstances->MarkRenderStateDirty();
	UE_LOG(
		LogAdaptiveEnv,
		Log,
		TEXT("M7 Patch registration data built. Patch=%s Generation=%u ")
		TEXT("Component=%s ObjectId=%u Total=%d Readable=%d InGrid=%d WeightedCells=%d."),
		*PatchId.ToString(EGuidFormats::DigitsWithHyphens),
		RegistrationGeneration,
		*GetPathNameSafe(ResolvedTargetInstances),
		ResolvedTargetInstances->GetUniqueID(),
		RegisteredInstanceCount,
		ReadableTransformCount,
		InGridInstanceCount,
		OutRegistration.WeightedCells.Num());
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
	if (!PendingVisualCommand.IsSet() || !IsValid(ResolvedTargetInstances)
		|| !IsValid(VisualProfile) || MaxInstanceUpdates <= 0
		|| ResolvedTargetInstances->GetInstanceCount() != RegisteredInstanceCount)
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
		ResolvedTargetInstances->SetCustomDataValue(
			InstanceIndex,
			VisualProfile->HealthCustomDataIndex,
			PendingVisualCommand->HealthRatio,
			false);
		ResolvedTargetInstances->SetCustomDataValue(
			InstanceIndex,
			VisualProfile->DensityVisibilityCustomDataIndex,
			bVisible ? 1.0f : 0.0f,
			false);
	}
	const int32 AppliedCount = EndIndex - NextPendingInstanceIndex;
	NextPendingInstanceIndex = EndIndex;
	if (AppliedCount > 0)
	{
		ResolvedTargetInstances->MarkRenderStateDirty();
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
