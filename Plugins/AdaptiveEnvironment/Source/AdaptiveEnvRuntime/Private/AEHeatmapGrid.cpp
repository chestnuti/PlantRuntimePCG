#include "AEHeatmapGrid.h"

#include "AdaptiveEnvGameplayTags.h"

bool FAEHeatmapGrid::Initialize(const FAEHeatmapGridConfig& InConfig)
{
	if (InConfig.Dimensions.X <= 0
		|| InConfig.Dimensions.Y <= 0
		|| !FMath::IsFinite(InConfig.CellSizeCm)
		|| InConfig.CellSizeCm <= 0.0f
		|| InConfig.KernelRadiusCells < 0
		|| !FMath::IsFinite(InConfig.KernelSigma)
		|| InConfig.KernelSigma <= 0.0f)
	{
		return false;
	}

	const int64 CellCount = static_cast<int64>(InConfig.Dimensions.X) * InConfig.Dimensions.Y;
	if (CellCount <= 0 || CellCount > MAX_int32)
	{
		return false;
	}

	Config = InConfig;
	const FVector2D Extent(
		static_cast<double>(Config.Dimensions.X) * Config.CellSizeCm * 0.5,
		static_cast<double>(Config.Dimensions.Y) * Config.CellSizeCm * 0.5);
	WorldMin = Config.WorldCenter - Extent;
	Cells.SetNumZeroed(static_cast<int32>(CellCount));
	DirtyFlags.Init(false, Cells.Num());
	DirtyCellIndices.Reset();
	AgentStates.Reset();
	BehaviourRevision = 0;
	Stats = FAEBehaviourGridStats();
	return true;
}

void FAEHeatmapGrid::Reset()
{
	for (FAEBehaviourCellData& Cell : Cells)
	{
		Cell = FAEBehaviourCellData();
	}

	DirtyFlags.Init(false, Cells.Num());
	DirtyCellIndices.Reset();
	AgentStates.Reset();
	BehaviourRevision = 0;
	Stats = FAEBehaviourGridStats();
}

FBox2D FAEHeatmapGrid::GetWorldBounds() const
{
	const FVector2D Size(
		static_cast<double>(Config.Dimensions.X) * Config.CellSizeCm,
		static_cast<double>(Config.Dimensions.Y) * Config.CellSizeCm);
	return FBox2D(WorldMin, WorldMin + Size);
}

bool FAEHeatmapGrid::WorldToCell(const FVector& WorldLocation, FIntPoint& OutCoordinate) const
{
	if (!IsValid() || WorldLocation.ContainsNaN())
	{
		return false;
	}

	const int32 X = FMath::FloorToInt((WorldLocation.X - WorldMin.X) / Config.CellSizeCm);
	const int32 Y = FMath::FloorToInt((WorldLocation.Y - WorldMin.Y) / Config.CellSizeCm);
	const FIntPoint Coordinate(X, Y);
	int32 UnusedIndex = INDEX_NONE;
	if (!CellToIndex(Coordinate, UnusedIndex))
	{
		return false;
	}

	OutCoordinate = Coordinate;
	return true;
}

bool FAEHeatmapGrid::CellToIndex(const FIntPoint& Coordinate, int32& OutIndex) const
{
	if (Coordinate.X < 0
		|| Coordinate.Y < 0
		|| Coordinate.X >= Config.Dimensions.X
		|| Coordinate.Y >= Config.Dimensions.Y)
	{
		return false;
	}

	OutIndex = Coordinate.Y * Config.Dimensions.X + Coordinate.X;
	return Cells.IsValidIndex(OutIndex);
}

FVector FAEHeatmapGrid::GetCellWorldCenter(const FIntPoint& Coordinate) const
{
	return FVector(
		WorldMin.X + (static_cast<double>(Coordinate.X) + 0.5) * Config.CellSizeCm,
		WorldMin.Y + (static_cast<double>(Coordinate.Y) + 0.5) * Config.CellSizeCm,
		0.0);
}

EAEBehaviourSubmitResult FAEHeatmapGrid::AccumulateSample(const FAEBehaviourSample& Sample)
{
	check(IsInGameThread());

	EAEBehaviourSubmitResult ValidationResult = EAEBehaviourSubmitResult::Accepted;
	if (!ValidateSample(Sample, ValidationResult))
	{
		++Stats.RejectedSampleCount;
		if (ValidationResult == EAEBehaviourSubmitResult::DuplicateSequence)
		{
			++Stats.DuplicateSampleCount;
		}
		else if (ValidationResult == EAEBehaviourSubmitResult::OutOfOrder)
		{
			++Stats.OutOfOrderSampleCount;
		}
		return ValidationResult;
	}

	FAgentState& AgentState = AgentStates.FindOrAdd(Sample.AgentId);
	if (AgentState.bHasSequence)
	{
		if (Sample.SequenceNumber == AgentState.LastSequenceNumber)
		{
			++Stats.RejectedSampleCount;
			++Stats.DuplicateSampleCount;
			return EAEBehaviourSubmitResult::DuplicateSequence;
		}
		if (Sample.SequenceNumber < AgentState.LastSequenceNumber || Sample.Timestamp < AgentState.LastTimestamp)
		{
			++Stats.RejectedSampleCount;
			++Stats.OutOfOrderSampleCount;
			return EAEBehaviourSubmitResult::OutOfOrder;
		}
	}

	AgentState.LastSequenceNumber = Sample.SequenceNumber;
	AgentState.LastTimestamp = Sample.Timestamp;
	AgentState.bHasSequence = true;
	++Stats.AcceptedSampleCount;

	bool bChangedGrid = false;
	if (IsEventTag(Sample.BehaviourTag))
	{
		FIntPoint Coordinate;
		if (WorldToCell(Sample.WorldLocation, Coordinate))
		{
			const float CollectDelta = Sample.BehaviourTag == AdaptiveEnvGameplayTags::Behaviour_Collect.GetTag() ? Sample.EventIntensity : 0.0f;
			const float CombatDelta = Sample.BehaviourTag == AdaptiveEnvGameplayTags::Behaviour_Combat.GetTag() ? Sample.EventIntensity : 0.0f;
			AddRawContribution(Coordinate, 0.0f, 0.0f, 0.0f, 0.0f, CollectDelta, CombatDelta, FVector2f::ZeroVector, 0.0f, Sample.Timestamp);
			bChangedGrid = true;
		}
		else
		{
			++Stats.OutOfBoundsSampleCount;
		}
	}
	else if (!Sample.bHasPreviousLocation)
	{
		FIntPoint Coordinate;
		if (WorldToCell(Sample.WorldLocation, Coordinate))
		{
			const float PassDelta = (!AgentState.bInsideGrid || AgentState.LastCell != Coordinate) ? 1.0f : 0.0f;
			AddRawContribution(Coordinate, PassDelta, 0.0f, Sample.BehaviourTag == AdaptiveEnvGameplayTags::Behaviour_Dwell.GetTag() ? Sample.DeltaSeconds : 0.0f, 0.0f, 0.0f, 0.0f, FVector2f::ZeroVector, 0.0f, Sample.Timestamp);
			AgentState.LastCell = Coordinate;
			AgentState.bInsideGrid = true;
			bChangedGrid = true;
		}
		else
		{
			AgentState.bInsideGrid = false;
			++Stats.OutOfBoundsSampleCount;
		}
	}
	else
	{
		const FVector Segment = Sample.WorldLocation - Sample.PreviousWorldLocation;
		const float DistanceXY = FVector2D(Segment.X, Segment.Y).Size();
		const float Spacing = FMath::Max(Config.CellSizeCm * 0.5f, 1.0f);
		const int32 StepCount = FMath::Max(1, FMath::CeilToInt(DistanceXY / Spacing));
		const float DistancePerStep = Sample.TravelDistanceMeters / StepCount;
		const FVector2D DirectionXY = FVector2D(Segment.X, Segment.Y).GetSafeNormal();
		const bool bAddsFlow = IsFlowTag(Sample.BehaviourTag) && !DirectionXY.IsNearlyZero();

		for (int32 PointIndex = 0; PointIndex <= StepCount; ++PointIndex)
		{
			const float Alpha = static_cast<float>(PointIndex) / StepCount;
			const FVector Point = FMath::Lerp(Sample.PreviousWorldLocation, Sample.WorldLocation, Alpha);
			FIntPoint Coordinate;
			if (WorldToCell(Point, Coordinate))
			{
				const float PassDelta = (!AgentState.bInsideGrid || AgentState.LastCell != Coordinate) ? 1.0f : 0.0f;
				if (PassDelta > 0.0f)
				{
					AddRawContribution(Coordinate, PassDelta, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, FVector2f::ZeroVector, 0.0f, Sample.Timestamp);
					bChangedGrid = true;
				}
				AgentState.LastCell = Coordinate;
				AgentState.bInsideGrid = true;
			}
			else
			{
				AgentState.bInsideGrid = false;
			}
		}

		for (int32 StepIndex = 0; StepIndex < StepCount; ++StepIndex)
		{
			const float Alpha = (static_cast<float>(StepIndex) + 0.5f) / StepCount;
			const FVector MidPoint = FMath::Lerp(Sample.PreviousWorldLocation, Sample.WorldLocation, Alpha);
			FIntPoint Coordinate;
			if (!WorldToCell(MidPoint, Coordinate))
			{
				continue;
			}

			const float SprintDistance = Sample.BehaviourTag == AdaptiveEnvGameplayTags::Behaviour_Sprint.GetTag() ? DistancePerStep : 0.0f;
			const FVector2f FlowDelta = bAddsFlow
				? FVector2f(static_cast<float>(DirectionXY.X), static_cast<float>(DirectionXY.Y)) * DistancePerStep
				: FVector2f::ZeroVector;
			AddRawContribution(Coordinate, 0.0f, DistancePerStep, 0.0f, SprintDistance, 0.0f, 0.0f, FlowDelta, bAddsFlow ? DistancePerStep : 0.0f, Sample.Timestamp);
			bChangedGrid = true;
		}

		if (Sample.BehaviourTag == AdaptiveEnvGameplayTags::Behaviour_Dwell.GetTag())
		{
			FIntPoint Coordinate;
			if (WorldToCell(Sample.WorldLocation, Coordinate))
			{
				AddRawContribution(Coordinate, 0.0f, 0.0f, Sample.DeltaSeconds, 0.0f, 0.0f, 0.0f, FVector2f::ZeroVector, 0.0f, Sample.Timestamp);
				bChangedGrid = true;
			}
		}
		if (!bChangedGrid)
		{
			++Stats.OutOfBoundsSampleCount;
		}
	}

	if (bChangedGrid)
	{
		++BehaviourRevision;
	}
	return EAEBehaviourSubmitResult::Accepted;
}

void FAEHeatmapGrid::RecordRejectedSample(const EAEBehaviourSubmitResult Result)
{
	++Stats.RejectedSampleCount;
	if (Result == EAEBehaviourSubmitResult::DuplicateSequence)
	{
		++Stats.DuplicateSampleCount;
	}
	else if (Result == EAEBehaviourSubmitResult::OutOfOrder)
	{
		++Stats.OutOfOrderSampleCount;
	}
}

bool FAEHeatmapGrid::GetCellSnapshot(const FIntPoint& Coordinate, FAEBehaviourCellSnapshot& OutSnapshot) const
{
	int32 Index = INDEX_NONE;
	if (!CellToIndex(Coordinate, Index))
	{
		return false;
	}
	OutSnapshot = MakeSnapshot(Coordinate, Index);
	return true;
}

bool FAEHeatmapGrid::GetCellSnapshotAtWorldLocation(const FVector& WorldLocation, FAEBehaviourCellSnapshot& OutSnapshot) const
{
	FIntPoint Coordinate;
	return WorldToCell(WorldLocation, Coordinate) && GetCellSnapshot(Coordinate, OutSnapshot);
}

void FAEHeatmapGrid::GetNonEmptyCellsInRadius(const FVector& WorldLocation, const float RadiusCm, const int32 MaxCells, TArray<FAEBehaviourCellSnapshot>& OutCells) const
{
	OutCells.Reset();
	if (MaxCells <= 0)
	{
		return;
	}

	const float RadiusSquared = FMath::Square(FMath::Max(RadiusCm, 0.0f));
	for (int32 Index = 0; Index < Cells.Num() && OutCells.Num() < MaxCells; ++Index)
	{
		const FAEBehaviourCellData& Cell = Cells[Index];
		if (Cell.PassCount <= 0.0f
			&& Cell.TravelDistanceMeters <= 0.0f
			&& Cell.DwellSeconds <= 0.0f
			&& Cell.CollectEventCount <= 0.0f
			&& Cell.CombatEventCount <= 0.0f
			&& Cell.SmoothedActivity <= 0.0f)
		{
			continue;
		}

		const FIntPoint Coordinate(Index % Config.Dimensions.X, Index / Config.Dimensions.X);
		const FVector Center = GetCellWorldCenter(Coordinate);
		if (FVector2D::DistSquared(FVector2D(Center), FVector2D(WorldLocation)) <= RadiusSquared)
		{
			OutCells.Add(MakeSnapshot(Coordinate, Index));
		}
	}
}

void FAEHeatmapGrid::ClearDirtyCells()
{
	for (const int32 Index : DirtyCellIndices)
	{
		if (DirtyFlags.IsValidIndex(Index))
		{
			DirtyFlags[Index] = false;
		}
	}
	DirtyCellIndices.Reset();
}

bool FAEHeatmapGrid::ValidateSample(const FAEBehaviourSample& Sample, EAEBehaviourSubmitResult& OutResult) const
{
	if (!Sample.AgentId.IsValid())
	{
		OutResult = EAEBehaviourSubmitResult::InvalidAgent;
		return false;
	}
	if (Sample.WorldLocation.ContainsNaN()
		|| Sample.Velocity.ContainsNaN()
		|| (Sample.bHasPreviousLocation && Sample.PreviousWorldLocation.ContainsNaN()))
	{
		OutResult = EAEBehaviourSubmitResult::InvalidLocation;
		return false;
	}
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
	if (!IsMovementTag(Sample.BehaviourTag) && !IsEventTag(Sample.BehaviourTag))
	{
		OutResult = EAEBehaviourSubmitResult::InvalidTag;
		return false;
	}
	return true;
}

bool FAEHeatmapGrid::IsMovementTag(const FGameplayTag& Tag) const
{
	return Tag == AdaptiveEnvGameplayTags::Behaviour_Move.GetTag()
		|| Tag == AdaptiveEnvGameplayTags::Behaviour_Dwell.GetTag()
		|| Tag == AdaptiveEnvGameplayTags::Behaviour_Sprint.GetTag();
}

bool FAEHeatmapGrid::IsFlowTag(const FGameplayTag& Tag) const
{
	return Tag == AdaptiveEnvGameplayTags::Behaviour_Move.GetTag()
		|| Tag == AdaptiveEnvGameplayTags::Behaviour_Sprint.GetTag();
}

bool FAEHeatmapGrid::IsEventTag(const FGameplayTag& Tag) const
{
	return Tag == AdaptiveEnvGameplayTags::Behaviour_Collect.GetTag()
		|| Tag == AdaptiveEnvGameplayTags::Behaviour_Combat.GetTag();
}

void FAEHeatmapGrid::MarkDirty(const int32 Index)
{
	if (DirtyFlags.IsValidIndex(Index) && !DirtyFlags[Index])
	{
		DirtyFlags[Index] = true;
		DirtyCellIndices.Add(Index);
	}
}

void FAEHeatmapGrid::AddSmoothedActivity(const FIntPoint& Coordinate, const float Contribution)
{
	if (Contribution <= 0.0f)
	{
		return;
	}

	struct FWeightedIndex
	{
		int32 Index = INDEX_NONE;
		float Weight = 0.0f;
	};

	TArray<FWeightedIndex, TInlineAllocator<25>> WeightedIndices;
	float WeightSum = 0.0f;
	const float SigmaSquared = FMath::Square(Config.KernelSigma);
	for (int32 OffsetY = -Config.KernelRadiusCells; OffsetY <= Config.KernelRadiusCells; ++OffsetY)
	{
		for (int32 OffsetX = -Config.KernelRadiusCells; OffsetX <= Config.KernelRadiusCells; ++OffsetX)
		{
			int32 Index = INDEX_NONE;
			if (!CellToIndex(Coordinate + FIntPoint(OffsetX, OffsetY), Index))
			{
				continue;
			}
			const float DistanceSquared = static_cast<float>(OffsetX * OffsetX + OffsetY * OffsetY);
			const float Weight = FMath::Exp(-DistanceSquared / (2.0f * SigmaSquared));
			WeightedIndices.Add({ Index, Weight });
			WeightSum += Weight;
		}
	}

	if (WeightSum <= UE_SMALL_NUMBER)
	{
		return;
	}

	for (const FWeightedIndex& Item : WeightedIndices)
	{
		Cells[Item.Index].SmoothedActivity += Contribution * (Item.Weight / WeightSum);
		MarkDirty(Item.Index);
	}
}

void FAEHeatmapGrid::AddRawContribution(
	const FIntPoint& Coordinate,
	const float PassDelta,
	const float DistanceDelta,
	const float DwellDelta,
	const float SprintDistanceDelta,
	const float CollectDelta,
	const float CombatDelta,
	const FVector2f& FlowDelta,
	const float FlowWeightDelta,
	const double Timestamp)
{
	int32 Index = INDEX_NONE;
	if (!CellToIndex(Coordinate, Index))
	{
		return;
	}

	FAEBehaviourCellData& Cell = Cells[Index];
	Cell.PassCount += PassDelta;
	Cell.TravelDistanceMeters += DistanceDelta;
	Cell.DwellSeconds += DwellDelta;
	Cell.SprintDistanceMeters += SprintDistanceDelta;
	Cell.CollectEventCount += CollectDelta;
	Cell.CombatEventCount += CombatDelta;
	Cell.FlowSum += FlowDelta;
	Cell.FlowWeight += FlowWeightDelta;
	Cell.LastUpdatedTime = FMath::Max(Cell.LastUpdatedTime, Timestamp);
	MarkDirty(Index);

	const float ActivityContribution = PassDelta + DistanceDelta + DwellDelta + CollectDelta + CombatDelta;
	AddSmoothedActivity(Coordinate, ActivityContribution);
}

FAEBehaviourCellSnapshot FAEHeatmapGrid::MakeSnapshot(const FIntPoint& Coordinate, const int32 Index) const
{
	const FAEBehaviourCellData& Cell = Cells[Index];
	FAEBehaviourCellSnapshot Snapshot;
	Snapshot.Coordinate = Coordinate;
	Snapshot.WorldCenter = GetCellWorldCenter(Coordinate);
	Snapshot.PassCount = Cell.PassCount;
	Snapshot.TravelDistanceMeters = Cell.TravelDistanceMeters;
	Snapshot.DwellSeconds = Cell.DwellSeconds;
	Snapshot.SprintDistanceMeters = Cell.SprintDistanceMeters;
	Snapshot.CollectEventCount = Cell.CollectEventCount;
	Snapshot.CombatEventCount = Cell.CombatEventCount;
	Snapshot.SmoothedActivity = Cell.SmoothedActivity;
	Snapshot.LastUpdatedTime = Cell.LastUpdatedTime;
	if (Cell.FlowWeight > UE_SMALL_NUMBER)
	{
		const FVector2f AverageFlow = Cell.FlowSum / Cell.FlowWeight;
		Snapshot.FlowMagnitude = AverageFlow.Size();
		Snapshot.FlowDirection = FVector2D(AverageFlow.GetSafeNormal());
	}
	return Snapshot;
}
