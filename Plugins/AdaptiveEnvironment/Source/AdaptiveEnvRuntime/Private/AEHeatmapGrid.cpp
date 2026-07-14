#include "AEHeatmapGrid.h"

#include "AdaptiveEnvGameplayTags.h"

// Validate configuration and allocate a contiguous row-major grid.
bool FAEHeatmapGrid::Initialize(const FAEHeatmapGridConfig& InConfig)
{
	// Reject invalid dimensions, units, or smoothing parameters.
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

	// Calculate in 64 bits and reject allocations beyond TArray indexing.
	const int64 CellCount = static_cast<int64>(InConfig.Dimensions.X) * InConfig.Dimensions.Y;
	if (CellCount <= 0 || CellCount > MAX_int32)
	{
		return false;
	}

	// Derive the lower-left world origin from the configured grid centre.
	Config = InConfig;
	const FVector2D Extent(
		static_cast<double>(Config.Dimensions.X) * Config.CellSizeCm * 0.5,
		static_cast<double>(Config.Dimensions.Y) * Config.CellSizeCm * 0.5);
	WorldMin = Config.WorldCenter - Extent;
	// Allocate cells and clear all derived tracking state.
	Cells.SetNumZeroed(static_cast<int32>(CellCount));
	DirtyFlags.Init(false, Cells.Num());
	DirtyCellIndices.Reset();
	AgentStates.Reset();
	BehaviourRevision = 0;
	Stats = FAEBehaviourGridStats();
	return true;
}

// Clear cell values and all per-agent aggregation state.
void FAEHeatmapGrid::Reset()
{
	// Restore every allocated cell without changing grid configuration.
	for (FAEBehaviourCellData& Cell : Cells)
	{
		Cell = FAEBehaviourCellData();
	}

	// Reset dirty tracking, ordering guards, revision, and statistics.
	DirtyFlags.Init(false, Cells.Num());
	DirtyCellIndices.Reset();
	AgentStates.Reset();
	BehaviourRevision = 0;
	Stats = FAEBehaviourGridStats();
}

// Build the half-open world XY bounds from origin and dimensions.
FBox2D FAEHeatmapGrid::GetWorldBounds() const
{
	const FVector2D Size(
		static_cast<double>(Config.Dimensions.X) * Config.CellSizeCm,
		static_cast<double>(Config.Dimensions.Y) * Config.CellSizeCm);
	return FBox2D(WorldMin, WorldMin + Size);
}

// Convert a world XY position to a validated integer cell coordinate.
bool FAEHeatmapGrid::WorldToCell(const FVector& WorldLocation, FIntPoint& OutCoordinate) const
{
	// Reject unavailable grids and invalid positions before arithmetic.
	if (!IsValid() || WorldLocation.ContainsNaN())
	{
		return false;
	}

	// Floor relative coordinates so upper bounds remain excluded.
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

// Flatten a valid XY coordinate into row-major storage.
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

// Convert a cell coordinate to its world-space centre at ground Z.
FVector FAEHeatmapGrid::GetCellWorldCenter(const FIntPoint& Coordinate) const
{
	return FVector(
		WorldMin.X + (static_cast<double>(Coordinate.X) + 0.5) * Config.CellSizeCm,
		WorldMin.Y + (static_cast<double>(Coordinate.Y) + 0.5) * Config.CellSizeCm,
		0.0);
}

// Validate and aggregate one sample into raw grid metrics.
EAEBehaviourSubmitResult FAEHeatmapGrid::AccumulateSample(const FAEBehaviourSample& Sample)
{
	// Raw grid mutation is restricted to the Game Thread.
	check(IsInGameThread());

	// Validate sample values before updating statistics or agent state.
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

	// Enforce accepted sequence and timestamp ordering for this agent.
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

	// Commit ordering state after all rejection checks pass.
	AgentState.LastSequenceNumber = Sample.SequenceNumber;
	AgentState.LastTimestamp = Sample.Timestamp;
	AgentState.bHasSequence = true;
	++Stats.AcceptedSampleCount;

	// Route discrete events, first observations, and movement segments separately.
	bool bChangedGrid = false;
	if (IsEventTag(Sample.BehaviourTag))
	{
		// Apply a discrete event only to the cell containing its position.
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
		// Use the first observation to initialize pass and occupancy state.
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
		// Resample the XY segment at half-cell spacing for continuous coverage.
		const FVector Segment = Sample.WorldLocation - Sample.PreviousWorldLocation;
		const float DistanceXY = FVector2D(Segment.X, Segment.Y).Size();
		const float Spacing = FMath::Max(Config.CellSizeCm * 0.5f, 1.0f);
		const int32 StepCount = FMath::Max(1, FMath::CeilToInt(DistanceXY / Spacing));
		const float DistancePerStep = Sample.TravelDistanceMeters / StepCount;
		const FVector2D DirectionXY = FVector2D(Segment.X, Segment.Y).GetSafeNormal();
		const bool bAddsFlow = IsFlowTag(Sample.BehaviourTag) && !DirectionXY.IsNearlyZero();

		// Track each cell entry along segment endpoints and sample points.
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

		// Allocate equal distance and flow contributions at subsegment midpoints.
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

		// Assign dwell duration to the final occupied cell.
		if (Sample.BehaviourTag == AdaptiveEnvGameplayTags::Behaviour_Dwell.GetTag())
		{
			FIntPoint Coordinate;
			if (WorldToCell(Sample.WorldLocation, Coordinate))
			{
				AddRawContribution(Coordinate, 0.0f, 0.0f, Sample.DeltaSeconds, 0.0f, 0.0f, 0.0f, FVector2f::ZeroVector, 0.0f, Sample.Timestamp);
				bChangedGrid = true;
			}
		}
		// Count a fully outside segment once for diagnostics.
		if (!bChangedGrid)
		{
			++Stats.OutOfBoundsSampleCount;
		}
	}

	// Increment revision once for an accepted sample that changed any cell.
	if (bChangedGrid)
	{
		++BehaviourRevision;
	}
	return EAEBehaviourSubmitResult::Accepted;
}

// Record a sample rejected by the subsystem queue before grid aggregation.
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

// Copy one valid internal cell into a public snapshot.
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

// Resolve a world position and return its public cell snapshot.
bool FAEHeatmapGrid::GetCellSnapshotAtWorldLocation(const FVector& WorldLocation, FAEBehaviourCellSnapshot& OutSnapshot) const
{
	FIntPoint Coordinate;
	return WorldToCell(WorldLocation, Coordinate) && GetCellSnapshot(Coordinate, OutSnapshot);
}

// Collect visible non-empty cells within a bounded debug radius.
void FAEHeatmapGrid::GetNonEmptyCellsInRadius(const FVector& WorldLocation, const float RadiusCm, const int32 MaxCells, TArray<FAEBehaviourCellSnapshot>& OutCells) const
{
	// Clear caller output and reject empty budgets.
	OutCells.Reset();
	if (MaxCells <= 0)
	{
		return;
	}

	// Scan row-major cells until the output budget is exhausted.
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

		// Include the cell only when its centre lies inside the XY radius.
		const FIntPoint Coordinate(Index % Config.Dimensions.X, Index / Config.Dimensions.X);
		const FVector Center = GetCellWorldCenter(Coordinate);
		if (FVector2D::DistSquared(FVector2D(Center), FVector2D(WorldLocation)) <= RadiusSquared)
		{
			OutCells.Add(MakeSnapshot(Coordinate, Index));
		}
	}
}

// Clear dirty flags for cells consumed during the current tick.
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

// Validate sample identity, finite values, ranges, and supported tags.
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

// Recognize tags that represent continuous movement or dwelling.
bool FAEHeatmapGrid::IsMovementTag(const FGameplayTag& Tag) const
{
	return Tag == AdaptiveEnvGameplayTags::Behaviour_Move.GetTag()
		|| Tag == AdaptiveEnvGameplayTags::Behaviour_Dwell.GetTag()
		|| Tag == AdaptiveEnvGameplayTags::Behaviour_Sprint.GetTag();
}

// Recognize movement tags that contribute a directional flow vector.
bool FAEHeatmapGrid::IsFlowTag(const FGameplayTag& Tag) const
{
	return Tag == AdaptiveEnvGameplayTags::Behaviour_Move.GetTag()
		|| Tag == AdaptiveEnvGameplayTags::Behaviour_Sprint.GetTag();
}

// Recognize supported instantaneous event tags.
bool FAEHeatmapGrid::IsEventTag(const FGameplayTag& Tag) const
{
	return Tag == AdaptiveEnvGameplayTags::Behaviour_Collect.GetTag()
		|| Tag == AdaptiveEnvGameplayTags::Behaviour_Combat.GetTag();
}

// Add one cell index to the dirty list without duplicates.
void FAEHeatmapGrid::MarkDirty(const int32 Index)
{
	if (DirtyFlags.IsValidIndex(Index) && !DirtyFlags[Index])
	{
		DirtyFlags[Index] = true;
		DirtyCellIndices.Add(Index);
	}
}

// Distribute raw activity across valid neighbours with a normalized Gaussian kernel.
void FAEHeatmapGrid::AddSmoothedActivity(const FIntPoint& Coordinate, const float Contribution)
{
	// Ignore empty contributions before building kernel weights.
	if (Contribution <= 0.0f)
	{
		return;
	}

	// Store valid neighbour indices and their unnormalized weights inline.
	struct FWeightedIndex
	{
		int32 Index = INDEX_NONE;
		float Weight = 0.0f;
	};

	TArray<FWeightedIndex, TInlineAllocator<25>> WeightedIndices;
	float WeightSum = 0.0f;
	const float SigmaSquared = FMath::Square(Config.KernelSigma);
	// Calculate weights only for neighbours inside the grid.
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

	// Normalize weights so the total smoothed contribution is conserved.
	for (const FWeightedIndex& Item : WeightedIndices)
	{
		Cells[Item.Index].SmoothedActivity += Contribution * (Item.Weight / WeightSum);
		MarkDirty(Item.Index);
	}
}

// Apply raw metric deltas and derived smoothed activity to one cell.
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
	// Reject invalid coordinates before mutating contiguous storage.
	int32 Index = INDEX_NONE;
	if (!CellToIndex(Coordinate, Index))
	{
		return;
	}

	// Accumulate independent raw facts and the latest timestamp.
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

	// Smooth a combined debug activity value without altering raw facts.
	const float ActivityContribution = PassDelta + DistanceDelta + DwellDelta + CollectDelta + CombatDelta;
	AddSmoothedActivity(Coordinate, ActivityContribution);
}

// Convert mutable cell storage into an immutable public snapshot.
FAEBehaviourCellSnapshot FAEHeatmapGrid::MakeSnapshot(const FIntPoint& Coordinate, const int32 Index) const
{
	// Copy raw metrics and spatial identity.
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
	// Normalize distance-weighted flow only when a stable denominator exists.
	if (Cell.FlowWeight > UE_SMALL_NUMBER)
	{
		const FVector2f AverageFlow = Cell.FlowSum / Cell.FlowWeight;
		Snapshot.FlowMagnitude = AverageFlow.Size();
		Snapshot.FlowDirection = FVector2D(AverageFlow.GetSafeNormal());
	}
	return Snapshot;
}
