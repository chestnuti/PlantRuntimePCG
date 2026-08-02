#include "AEM8Types.h"

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
