#include "AdaptiveEnvWorldSubsystem.h"

#include "AEBehaviourTrackerComponent.h"
#include "AEHeatmapRendererComponent.h"
#include "AdaptiveEnvGameplayTags.h"
#include "AdaptiveEnvLog.h"
#include "AdaptiveEnvSettings.h"

void UAEAdaptiveEnvWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	InstanceId = FGuid::NewGuid();
	TickCount = 0;
	const UAdaptiveEnvSettings* Settings = GetDefault<UAdaptiveEnvSettings>();
	bRuntimeEnabled = Settings->bEnableRuntime;
	BehaviourStepSeconds = 1.0f / FMath::Max(Settings->BehaviourSampleRateHz, 1.0f);
	MaxBehaviourSubstepsPerFrame = FMath::Max(Settings->MaxBehaviourSubstepsPerFrame, 1);

	FAEHeatmapGridConfig GridConfig;
	GridConfig.WorldCenter = Settings->GridWorldCenter;
	GridConfig.Dimensions = FIntPoint(Settings->GridWidth, Settings->GridHeight);
	GridConfig.CellSizeCm = Settings->CellSizeCm;
	GridConfig.KernelRadiusCells = Settings->ActivityKernelRadiusCells;
	GridConfig.KernelSigma = Settings->ActivityKernelSigma;
	if (!BehaviourGrid.Initialize(GridConfig))
	{
		bRuntimeEnabled = false;
		UE_LOG(LogAdaptiveEnv, Error, TEXT("Behaviour grid initialization failed."));
	}

	UE_LOG(
		LogAdaptiveEnv,
		Log,
		TEXT("World subsystem initialized. World=%s Instance=%s"),
		*GetNameSafe(GetWorld()),
		*InstanceId.ToString(EGuidFormats::DigitsWithHyphens));
}

void UAEAdaptiveEnvWorldSubsystem::Deinitialize()
{
	UE_LOG(
		LogAdaptiveEnv,
		Log,
		TEXT("World subsystem deinitialized. World=%s Instance=%s Ticks=%lld"),
		*GetNameSafe(GetWorld()),
		*InstanceId.ToString(EGuidFormats::DigitsWithHyphens),
		TickCount);

	bRuntimeEnabled = false;
	RegisteredTrackers.Reset();
	PendingTrackerAdds.Reset();
	PendingTrackerRemoves.Reset();
	RegisteredRenderers.Reset();
	PendingRendererAdds.Reset();
	PendingRendererRemoves.Reset();
	PendingSamples.Reset();
	ProcessingSamples.Reset();
	LastQueuedSequenceByAgent.Reset();
	LastQueuedTimestampByAgent.Reset();
	BehaviourGrid.Reset();
	Super::Deinitialize();
}

void UAEAdaptiveEnvWorldSubsystem::Tick(float DeltaTime)
{
	++TickCount;
	ApplyPendingRegistrations();

	const double SafeDeltaTime = FMath::Max(static_cast<double>(DeltaTime), 0.0);
	BehaviourAccumulator += SafeDeltaTime;
	const int32 AvailableSteps = FMath::FloorToInt((BehaviourAccumulator + UE_DOUBLE_SMALL_NUMBER) / BehaviourStepSeconds);
	const int32 StepsToRun = FMath::Min(AvailableSteps, MaxBehaviourSubstepsPerFrame);
	if (AvailableSteps > MaxBehaviourSubstepsPerFrame)
	{
		++SchedulerOverrunCount;
		BehaviourAccumulator = FMath::Fmod(BehaviourAccumulator, static_cast<double>(BehaviourStepSeconds));
	}
	else
	{
		BehaviourAccumulator -= static_cast<double>(StepsToRun) * BehaviourStepSeconds;
	}

	for (int32 StepIndex = 0; StepIndex < StepsToRun; ++StepIndex)
	{
		BehaviourTimeSeconds += BehaviourStepSeconds;
		SampleRegisteredTrackers(BehaviourStepSeconds);
		ProcessPendingSamples();
		++ProcessedBehaviourStepCount;
	}

	UpdateDebugRenderers(DeltaTime);
	BehaviourGrid.ClearDirtyCells();
}

bool UAEAdaptiveEnvWorldSubsystem::IsTickable() const
{
	return bRuntimeEnabled && IsInitialized();
}

TStatId UAEAdaptiveEnvWorldSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UAEAdaptiveEnvWorldSubsystem, STATGROUP_Tickables);
}

bool UAEAdaptiveEnvWorldSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game
		|| WorldType == EWorldType::PIE
		|| WorldType == EWorldType::GamePreview;
}

EAEBehaviourSubmitResult UAEAdaptiveEnvWorldSubsystem::SubmitBehaviourSample(const FAEBehaviourSample& Sample)
{
	check(IsInGameThread());

	EAEBehaviourSubmitResult Result = EAEBehaviourSubmitResult::Accepted;
	if (!ValidateQueuedSample(Sample, Result))
	{
		BehaviourGrid.RecordRejectedSample(Result);
		return Result;
	}

	if (const int64* LastSequence = LastQueuedSequenceByAgent.Find(Sample.AgentId))
	{
		if (Sample.SequenceNumber == *LastSequence)
		{
			BehaviourGrid.RecordRejectedSample(EAEBehaviourSubmitResult::DuplicateSequence);
			return EAEBehaviourSubmitResult::DuplicateSequence;
		}
		if (Sample.SequenceNumber < *LastSequence)
		{
			BehaviourGrid.RecordRejectedSample(EAEBehaviourSubmitResult::OutOfOrder);
			return EAEBehaviourSubmitResult::OutOfOrder;
		}
	}
	if (const double* LastTimestamp = LastQueuedTimestampByAgent.Find(Sample.AgentId))
	{
		if (Sample.Timestamp < *LastTimestamp)
		{
			BehaviourGrid.RecordRejectedSample(EAEBehaviourSubmitResult::OutOfOrder);
			return EAEBehaviourSubmitResult::OutOfOrder;
		}
	}

	LastQueuedSequenceByAgent.Add(Sample.AgentId, Sample.SequenceNumber);
	LastQueuedTimestampByAgent.Add(Sample.AgentId, Sample.Timestamp);
	PendingSamples.Add(Sample);
	return EAEBehaviourSubmitResult::Accepted;
}

void UAEAdaptiveEnvWorldSubsystem::RegisterBehaviourTracker(UAEBehaviourTrackerComponent* Tracker)
{
	if (IsValid(Tracker))
	{
		PendingTrackerAdds.AddUnique(Tracker);
	}
}

void UAEAdaptiveEnvWorldSubsystem::UnregisterBehaviourTracker(UAEBehaviourTrackerComponent* Tracker)
{
	if (Tracker != nullptr)
	{
		PendingTrackerRemoves.AddUnique(Tracker);
	}
}

void UAEAdaptiveEnvWorldSubsystem::RegisterHeatmapRenderer(UAEHeatmapRendererComponent* Renderer)
{
	if (IsValid(Renderer))
	{
		PendingRendererAdds.AddUnique(Renderer);
	}
}

void UAEAdaptiveEnvWorldSubsystem::UnregisterHeatmapRenderer(UAEHeatmapRendererComponent* Renderer)
{
	if (Renderer != nullptr)
	{
		PendingRendererRemoves.AddUnique(Renderer);
	}
}

bool UAEAdaptiveEnvWorldSubsystem::GetBehaviourCellAtWorldLocation(const FVector& Location, FAEBehaviourCellSnapshot& OutSnapshot) const
{
	return BehaviourGrid.GetCellSnapshotAtWorldLocation(Location, OutSnapshot);
}

bool UAEAdaptiveEnvWorldSubsystem::GetBehaviourCell(const FIntPoint& Coordinate, FAEBehaviourCellSnapshot& OutSnapshot) const
{
	return BehaviourGrid.GetCellSnapshot(Coordinate, OutSnapshot);
}

FIntPoint UAEAdaptiveEnvWorldSubsystem::GetGridDimensions() const
{
	return BehaviourGrid.GetConfig().Dimensions;
}

FBox2D UAEAdaptiveEnvWorldSubsystem::GetGridWorldBounds() const
{
	return BehaviourGrid.GetWorldBounds();
}

int64 UAEAdaptiveEnvWorldSubsystem::GetBehaviourRevision() const
{
	return static_cast<int64>(BehaviourGrid.GetBehaviourRevision());
}

int32 UAEAdaptiveEnvWorldSubsystem::GetDirtyCellCount() const
{
	return BehaviourGrid.GetDirtyCellIndices().Num();
}

FAEBehaviourGridStats UAEAdaptiveEnvWorldSubsystem::GetBehaviourGridStats() const
{
	return BehaviourGrid.GetStats();
}

void UAEAdaptiveEnvWorldSubsystem::ResetBehaviourGrid()
{
	BehaviourGrid.Reset();
	PendingSamples.Reset();
	ProcessingSamples.Reset();
	LastQueuedSequenceByAgent.Reset();
	LastQueuedTimestampByAgent.Reset();
	BehaviourAccumulator = 0.0;
	BehaviourTimeSeconds = 0.0;
	ProcessedBehaviourStepCount = 0;
	SchedulerOverrunCount = 0;
	for (const TWeakObjectPtr<UAEBehaviourTrackerComponent>& Tracker : RegisteredTrackers)
	{
		if (Tracker.IsValid())
		{
			Tracker->ResetSamplingState();
		}
	}
}

void UAEAdaptiveEnvWorldSubsystem::GetDebugCells(const FVector& Location, const float RadiusCm, const int32 MaxCells, TArray<FAEBehaviourCellSnapshot>& OutCells) const
{
	BehaviourGrid.GetNonEmptyCellsInRadius(Location, RadiusCm, MaxCells, OutCells);
}

void UAEAdaptiveEnvWorldSubsystem::ApplyPendingRegistrations()
{
	for (const TWeakObjectPtr<UAEBehaviourTrackerComponent>& Tracker : PendingTrackerRemoves)
	{
		RegisteredTrackers.Remove(Tracker);
	}
	PendingTrackerRemoves.Reset();
	RegisteredTrackers.RemoveAll([](const TWeakObjectPtr<UAEBehaviourTrackerComponent>& Item) { return !Item.IsValid(); });
	for (const TWeakObjectPtr<UAEBehaviourTrackerComponent>& Tracker : PendingTrackerAdds)
	{
		if (Tracker.IsValid())
		{
			RegisteredTrackers.AddUnique(Tracker);
		}
	}
	PendingTrackerAdds.Reset();

	for (const TWeakObjectPtr<UAEHeatmapRendererComponent>& Renderer : PendingRendererRemoves)
	{
		RegisteredRenderers.Remove(Renderer);
	}
	PendingRendererRemoves.Reset();
	RegisteredRenderers.RemoveAll([](const TWeakObjectPtr<UAEHeatmapRendererComponent>& Item) { return !Item.IsValid(); });
	for (const TWeakObjectPtr<UAEHeatmapRendererComponent>& Renderer : PendingRendererAdds)
	{
		if (Renderer.IsValid())
		{
			RegisteredRenderers.AddUnique(Renderer);
		}
	}
	PendingRendererAdds.Reset();
}

void UAEAdaptiveEnvWorldSubsystem::SampleRegisteredTrackers(const float StepSeconds)
{
	for (const TWeakObjectPtr<UAEBehaviourTrackerComponent>& Tracker : RegisteredTrackers)
	{
		if (!Tracker.IsValid())
		{
			continue;
		}
		FAEBehaviourSample Sample;
		if (Tracker->CaptureBehaviourSample(BehaviourTimeSeconds, StepSeconds, Sample))
		{
			SubmitBehaviourSample(Sample);
		}
	}
}

void UAEAdaptiveEnvWorldSubsystem::ProcessPendingSamples()
{
	Swap(PendingSamples, ProcessingSamples);
	PendingSamples.Reset();
	ProcessingSamples.Sort([](const FAEBehaviourSample& Left, const FAEBehaviourSample& Right)
	{
		if (Left.Timestamp != Right.Timestamp)
		{
			return Left.Timestamp < Right.Timestamp;
		}
		if (Left.AgentId != Right.AgentId)
		{
			return Left.AgentId < Right.AgentId;
		}
		return Left.SequenceNumber < Right.SequenceNumber;
	});

	for (const FAEBehaviourSample& Sample : ProcessingSamples)
	{
		BehaviourGrid.AccumulateSample(Sample);
	}
	ProcessingSamples.Reset();
}

void UAEAdaptiveEnvWorldSubsystem::UpdateDebugRenderers(const float DeltaTime)
{
	const UAdaptiveEnvSettings* Settings = GetDefault<UAdaptiveEnvSettings>();
	const double RefreshStep = 1.0 / FMath::Max(Settings->DebugRefreshRateHz, 0.1f);
	DebugAccumulator += FMath::Max(static_cast<double>(DeltaTime), 0.0);
	if (DebugAccumulator < RefreshStep)
	{
		return;
	}
	DebugAccumulator = FMath::Fmod(DebugAccumulator, RefreshStep);

	for (const TWeakObjectPtr<UAEHeatmapRendererComponent>& Renderer : RegisteredRenderers)
	{
		if (Renderer.IsValid())
		{
			Renderer->RenderDebug(*this, static_cast<float>(RefreshStep));
		}
	}
}

bool UAEAdaptiveEnvWorldSubsystem::ValidateQueuedSample(const FAEBehaviourSample& Sample, EAEBehaviourSubmitResult& OutResult) const
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

	const FGameplayTag& Tag = Sample.BehaviourTag;
	const bool bKnownTag = Tag == AdaptiveEnvGameplayTags::Behaviour_Move.GetTag()
		|| Tag == AdaptiveEnvGameplayTags::Behaviour_Dwell.GetTag()
		|| Tag == AdaptiveEnvGameplayTags::Behaviour_Sprint.GetTag()
		|| Tag == AdaptiveEnvGameplayTags::Behaviour_Collect.GetTag()
		|| Tag == AdaptiveEnvGameplayTags::Behaviour_Combat.GetTag();
	if (!bKnownTag)
	{
		OutResult = EAEBehaviourSubmitResult::InvalidTag;
		return false;
	}
	return true;
}
