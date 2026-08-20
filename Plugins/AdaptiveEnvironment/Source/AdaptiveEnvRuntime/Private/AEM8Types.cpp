#include "AEM8Types.h"

/* Expand leaf regions into stable plant-local transforms without accessing UObject state. */
void FAEM8LeafInstanceBuilder::Build(
	const TArray<FAELeafEmitterDescriptor>& Emitters,
	const float DensityScale,
	const float UniformScale,
	const float ScaleVariationRatio,
	const int32 MaximumLeafInstances,
	TArray<FAEM8LeafInstanceDescriptor>& OutInstances)
{
	OutInstances.Reset();
	if (!FMath::IsFinite(DensityScale) || DensityScale <= 0.0f
		|| !FMath::IsFinite(UniformScale) || UniformScale <= 0.0f
		|| !FMath::IsFinite(ScaleVariationRatio) || MaximumLeafInstances <= 0)
	{
		return;
	}

	const float SafeVariation = FMath::Clamp(ScaleVariationRatio, 0.0f, 1.0f);
	OutInstances.Reserve(FMath::Min(MaximumLeafInstances, Emitters.Num() * 4));
	for (const FAELeafEmitterDescriptor& Emitter : Emitters)
	{
		if (!FMath::IsFinite(Emitter.EmitterLengthCm) || Emitter.EmitterLengthCm <= 0.0f
			|| !FMath::IsFinite(Emitter.EmitterRadiusCm) || Emitter.EmitterRadiusCm < 0.0f
			|| !FMath::IsFinite(Emitter.DensityPerMeter) || Emitter.DensityPerMeter <= 0.0f)
		{
			continue;
		}

		// Convert centimetres to metres before applying the authored density.
		const int32 RequestedCount = FMath::Max(
			FMath::RoundToInt(Emitter.EmitterLengthCm * 0.01f * Emitter.DensityPerMeter * DensityScale),
			1);
		FRandomStream Random(Emitter.Seed);
		const FQuat EmitterRotation = Emitter.LocalTransform.GetRotation().GetNormalized();
		for (int32 LeafIndex = 0; LeafIndex < RequestedCount && OutInstances.Num() < MaximumLeafInstances; ++LeafIndex)
		{
			const float AlongCm = Random.FRandRange(0.0f, Emitter.EmitterLengthCm);
			const float RadialAngleRadians = Random.FRandRange(0.0f, 2.0f * UE_PI);
			const float RadialDistanceCm = FMath::Sqrt(Random.FRand()) * Emitter.EmitterRadiusCm;
			const FVector RadialOffset = EmitterRotation.GetForwardVector() * FMath::Cos(RadialAngleRadians) * RadialDistanceCm
				+ EmitterRotation.GetRightVector() * FMath::Sin(RadialAngleRadians) * RadialDistanceCm;
			const FVector Location = Emitter.LocalTransform.GetLocation()
				- EmitterRotation.GetUpVector() * AlongCm
				+ RadialOffset;
			const FQuat VariationRotation = FRotator(
				Random.FRandRange(-35.0f, 35.0f),
				Random.FRandRange(0.0f, 360.0f),
				Random.FRandRange(-20.0f, 20.0f)).Quaternion();
			const float Scale = UniformScale * Random.FRandRange(1.0f - SafeVariation, 1.0f + SafeVariation);

			FAEM8LeafInstanceDescriptor& Instance = OutInstances.AddDefaulted_GetRef();
			Instance.OwnerBranchModuleId = Emitter.OwnerBranchModuleId;
			Instance.PlantLocalTransform = FTransform(
				EmitterRotation * VariationRotation,
				Location,
				FVector(Scale));
			Instance.VisibilityThreshold = Random.FRand();
		}
		if (OutInstances.Num() >= MaximumLeafInstances)
		{
			break;
		}
	}
}

/* Build one deterministic round-robin window without accessing UObject state. */
void FAEM8RoundRobinScheduler::BuildWindow(
	const int32 ItemCount,
	const int32 Budget,
	int32& InOutCursor,
	TArray<int32>& OutIndices)
{
	OutIndices.Reset();
	if (ItemCount <= 0)
	{
		InOutCursor = 0;
		return;
	}

	// Normalize after registrations changed the active array size.
	InOutCursor = ((InOutCursor % ItemCount) + ItemCount) % ItemCount;
	const int32 UpdateCount = FMath::Min(FMath::Max(Budget, 0), ItemCount);
	OutIndices.Reserve(UpdateCount);

	// Emit one wrapping window in stable registration order.
	for (int32 Offset = 0; Offset < UpdateCount; ++Offset)
	{
		OutIndices.Add((InOutCursor + Offset) % ItemCount);
	}

	InOutCursor = (InOutCursor + UpdateCount) % ItemCount;
}

/* Select a bounded nearest-first activation batch without accessing UObject state. */
void FAEM8NeighborhoodSelector::SelectNearest(
	const TArray<FAEPlantInstanceSnapshot>& Snapshots,
	const FVector& PlayerWorldLocation,
	const float ActivationRadiusCm,
	const int32 MaximumSelectionCount,
	const TSet<int64>& ExcludedStablePointIds,
	TArray<FAEPlantInstanceSnapshot>& OutSelectedSnapshots)
{
	OutSelectedSnapshots.Reset();
	if (PlayerWorldLocation.ContainsNaN()
		|| !FMath::IsFinite(ActivationRadiusCm)
		|| ActivationRadiusCm < 0.0f
		|| MaximumSelectionCount <= 0)
	{
		return;
	}

	struct FCandidate
	{
		const FAEPlantInstanceSnapshot* Snapshot = nullptr;
		double DistanceSquared = 0.0;
	};
	const double RadiusSquared = FMath::Square(static_cast<double>(ActivationRadiusCm));
	TArray<FCandidate> Candidates;
	Candidates.Reserve(Snapshots.Num());
	for (const FAEPlantInstanceSnapshot& Snapshot : Snapshots)
	{
		if (!Snapshot.bVisible
			|| Snapshot.StablePointId == 0
			|| ExcludedStablePointIds.Contains(Snapshot.StablePointId)
			|| Snapshot.WorldLocation.ContainsNaN())
		{
			continue;
		}
		const double DistanceSquared = FVector::DistSquared(PlayerWorldLocation, Snapshot.WorldLocation);
		if (DistanceSquared <= RadiusSquared)
		{
			Candidates.Add({&Snapshot, DistanceSquared});
		}
	}

	Candidates.Sort([](const FCandidate& A, const FCandidate& B)
	{
		if (!FMath::IsNearlyEqual(A.DistanceSquared, B.DistanceSquared))
		{
			return A.DistanceSquared < B.DistanceSquared;
		}
		return A.Snapshot->StablePointId < B.Snapshot->StablePointId;
	});

	const int32 SelectionCount = FMath::Min(MaximumSelectionCount, Candidates.Num());
	OutSelectedSnapshots.Reserve(SelectionCount);
	for (int32 Index = 0; Index < SelectionCount; ++Index)
	{
		OutSelectedSnapshots.Add(*Candidates[Index].Snapshot);
	}
}

/* Resolve one inclusive fixed-time branch expiry boundary. */
bool FAEM8PoolPolicy::IsDetachedBranchExpired(
	const double CurrentTimeSeconds,
	const double ExpireTimeSeconds)
{
	/* Reject invalid clocks and include the exact configured lifetime boundary. */
	return FMath::IsFinite(CurrentTimeSeconds) && FMath::IsFinite(ExpireTimeSeconds) && CurrentTimeSeconds + UE_DOUBLE_SMALL_NUMBER >= ExpireTimeSeconds;
}

/* Prevent actor reassignment while any detached branch geometry remains alive. */
bool FAEM8PoolPolicy::CanReturnToAvailable(const int32 LiveDetachedBranchCount)
{
	return LiveDetachedBranchCount == 0;
}

/* Complete the current fade before persistent dead wood becomes permanently retired. */
float FAEM8MaterialPolicy::ResolveDeathFadeRatio(
	const float SourceDeathFadeRatio,
	const EAEBranchStructuralState StructuralState,
	bool& bInOutPersistentFadeLocked)
{
	const float NormalizedSourceFade = FMath::Clamp(SourceDeathFadeRatio, 0.0f, 1.0f);
	if (StructuralState != EAEBranchStructuralState::DeadWood)
	{
		bInOutPersistentFadeLocked = false;
		return NormalizedSourceFade;
	}

	bInOutPersistentFadeLocked = bInOutPersistentFadeLocked
		|| NormalizedSourceFade >= 1.0f - UE_KINDA_SMALL_NUMBER;
	return bInOutPersistentFadeLocked ? 1.0f : NormalizedSourceFade;
}

/* Gate destructive cleanup on explicit configuration and completed persistent fade. */
bool FAEM8MaterialPolicy::ShouldDestroyOwnerAfterPersistentFade(
	const bool bDestroyConfigured,
	const bool bHasPersistentDeadWood,
	const bool bAllPersistentDeadWoodFadeLocked)
{
	return bDestroyConfigured && bHasPersistentDeadWood && bAllPersistentDeadWoodFadeLocked;
}
