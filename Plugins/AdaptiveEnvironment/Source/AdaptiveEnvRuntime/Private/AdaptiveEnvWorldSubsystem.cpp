#include "AdaptiveEnvWorldSubsystem.h"

#include "AEBehaviourTrackerComponent.h"
#include "AEHeatmapRendererComponent.h"
#include "AEParameterBundleService.h"
#include "AEM3ParameterService.h"
#include "AEM4ParameterService.h"
#include "AEPublishedParameterBundleAsset.h"
#include "AdaptiveEnvGameplayTags.h"
#include "AdaptiveEnvLog.h"
#include "AdaptiveEnvSettings.h"

// Initialize one ordered runtime pipeline for the current World.
void UAEAdaptiveEnvWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Load scheduler controls and establish a unique World instance identity.
	InstanceId = FGuid::NewGuid();
	TickCount = 0;
	const UAdaptiveEnvSettings* Settings = GetDefault<UAdaptiveEnvSettings>();
	bRuntimeEnabled = Settings->bEnableRuntime;
	BehaviourStepSeconds = 1.0f / FMath::Max(Settings->BehaviourSampleRateHz, 1.0f);
	MaxBehaviourSubstepsPerFrame = FMath::Max(Settings->MaxBehaviourSubstepsPerFrame, 1);
	SimulationHoursPerRealSecond = FMath::Max(static_cast<double>(Settings->SimulationHoursPerRealSecond), 0.0);

	// Translate project settings into the grid initialization contract.
	FAEHeatmapGridConfig GridConfig;
	GridConfig.WorldCenter = Settings->GridWorldCenter;
	GridConfig.Dimensions = FIntPoint(Settings->GridWidth, Settings->GridHeight);
	GridConfig.CellSizeCm = Settings->CellSizeCm;
	GridConfig.KernelRadiusCells = Settings->ActivityKernelRadiusCells;
	GridConfig.KernelSigma = Settings->ActivityKernelSigma;
	// Disable runtime updates when the grid configuration cannot be allocated.
	if (!BehaviourGrid.Initialize(GridConfig))
	{
		bRuntimeEnabled = false;
		UE_LOG(LogAdaptiveEnv, Error, TEXT("Behaviour grid initialization failed."));
	}
	if (!ExposureGrid.Initialize(GridConfig))
	{
		bRuntimeEnabled = false;
		UE_LOG(LogAdaptiveEnv, Error, TEXT("M3 grid initialization failed."));
	}

	// Load one atomic M3/M4 bundle without disabling the validated M1 pipeline when absent.
	bM3Enabled = false;
	bM4Enabled = false;
	if (bRuntimeEnabled && (Settings->bEnableM3 || Settings->bEnableM4))
	{
		UAEPublishedParameterBundleAsset* Bundle = Settings->ParameterBundle.LoadSynchronous();
		if (Bundle != nullptr)
		{
			FString Error;
			if (!ApplyParameterBundle(Bundle, Error))
			{
				UE_LOG(LogAdaptiveEnv, Error, TEXT("Parameter bundle initialization failed. World=%s Error=%s"), *GetNameSafe(GetWorld()), *Error);
			}
			else
			{
				bM3Enabled = Settings->bEnableM3;
				bM4Enabled = Settings->bEnableM4;
			}
		}
		else
		{
			UE_LOG(LogAdaptiveEnv, Log, TEXT("M3 and M4 disabled because no parameter bundle is configured. World=%s"), *GetNameSafe(GetWorld()));
		}
	}

	UE_LOG(
		LogAdaptiveEnv,
		Log,
		TEXT("World subsystem initialized. World=%s Instance=%s"),
		*GetNameSafe(GetWorld()),
		*InstanceId.ToString(EGuidFormats::DigitsWithHyphens));
}

// Release all World-owned runtime state in a deterministic order.
void UAEAdaptiveEnvWorldSubsystem::Deinitialize()
{
	// Record final lifecycle diagnostics before state is cleared.
	UE_LOG(
		LogAdaptiveEnv,
		Log,
		TEXT("World subsystem deinitialized. World=%s Instance=%s Ticks=%lld"),
		*GetNameSafe(GetWorld()),
		*InstanceId.ToString(EGuidFormats::DigitsWithHyphens),
		TickCount);

	// Stop ticking and clear registrations, queues, order guards, and grid data.
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
	ExposureGrid.Reset();
	bM3Enabled = false;
	bM4Enabled = false;
	Super::Deinitialize();
}

// Advance registration, fixed-step sampling, aggregation, and debug output.
void UAEAdaptiveEnvWorldSubsystem::Tick(float DeltaTime)
{
	// Apply deferred registrations before any service iterates active arrays.
	++TickCount;
	ApplyPendingRegistrations();

	// Convert render time into a bounded number of fixed behaviour steps.
	const double SafeDeltaTime = FMath::Max(static_cast<double>(DeltaTime), 0.0);
	BehaviourAccumulator += SafeDeltaTime;
	const int32 AvailableSteps = FMath::FloorToInt((BehaviourAccumulator + UE_DOUBLE_SMALL_NUMBER) / BehaviourStepSeconds);
	const int32 StepsToRun = FMath::Min(AvailableSteps, MaxBehaviourSubstepsPerFrame);
	// Drop excess whole steps after an overrun while preserving fractional time.
	if (AvailableSteps > MaxBehaviourSubstepsPerFrame)
	{
		++SchedulerOverrunCount;
		BehaviourAccumulator = FMath::Fmod(BehaviourAccumulator, static_cast<double>(BehaviourStepSeconds));
	}
	else
	{
		BehaviourAccumulator -= static_cast<double>(StepsToRun) * BehaviourStepSeconds;
	}

	// Sample and aggregate each fixed step before visual consumers run.
	for (int32 StepIndex = 0; StepIndex < StepsToRun; ++StepIndex)
	{
		BehaviourTimeSeconds += BehaviourStepSeconds;
		SampleRegisteredTrackers(BehaviourStepSeconds);
		ProcessPendingSamples();
		UpdateM3(BehaviourStepSeconds);
		++ProcessedBehaviourStepCount;
	}

	// Draw the completed state, then clear only per-tick dirty markers.
	UpdateDebugRenderers(DeltaTime);
	BehaviourGrid.ClearDirtyCells();
}

// Tick only after successful initialization when runtime is enabled.
bool UAEAdaptiveEnvWorldSubsystem::IsTickable() const
{
	return bRuntimeEnabled && IsInitialized();
}

// Register this tickable object with Unreal performance statistics.
TStatId UAEAdaptiveEnvWorldSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UAEAdaptiveEnvWorldSubsystem, STATGROUP_Tickables);
}

// Create the subsystem only for playable World types.
bool UAEAdaptiveEnvWorldSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game
		|| WorldType == EWorldType::PIE
		|| WorldType == EWorldType::GamePreview;
}

// Validate ordering and append one sample to the producer queue.
EAEBehaviourSubmitResult UAEAdaptiveEnvWorldSubsystem::SubmitBehaviourSample(const FAEBehaviourSample& Sample)
{
	// All UObject access and queue mutation remain on the Game Thread.
	check(IsInGameThread());

	// Reject malformed samples before touching per-agent ordering state.
	EAEBehaviourSubmitResult Result = EAEBehaviourSubmitResult::Accepted;
	if (!ValidateQueuedSample(Sample, Result))
	{
		BehaviourGrid.RecordRejectedSample(Result);
		return Result;
	}

	// Reject duplicate or decreasing sequence numbers within the pending batch.
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
	// Reject timestamps older than the latest queued sample for this agent.
	if (const double* LastTimestamp = LastQueuedTimestampByAgent.Find(Sample.AgentId))
	{
		if (Sample.Timestamp < *LastTimestamp)
		{
			BehaviourGrid.RecordRejectedSample(EAEBehaviourSubmitResult::OutOfOrder);
			return EAEBehaviourSubmitResult::OutOfOrder;
		}
	}

	// Commit ordering guards and queue the validated sample.
	LastQueuedSequenceByAgent.Add(Sample.AgentId, Sample.SequenceNumber);
	LastQueuedTimestampByAgent.Add(Sample.AgentId, Sample.Timestamp);
	PendingSamples.Add(Sample);
	return EAEBehaviourSubmitResult::Accepted;
}

// Defer tracker registration until the next safe tick boundary.
void UAEAdaptiveEnvWorldSubsystem::RegisterBehaviourTracker(UAEBehaviourTrackerComponent* Tracker)
{
	if (IsValid(Tracker))
	{
		PendingTrackerAdds.AddUnique(Tracker);
	}
}

// Defer tracker removal until the next safe tick boundary.
void UAEAdaptiveEnvWorldSubsystem::UnregisterBehaviourTracker(UAEBehaviourTrackerComponent* Tracker)
{
	if (Tracker != nullptr)
	{
		PendingTrackerRemoves.AddUnique(Tracker);
	}
}

// Defer renderer registration until the next safe tick boundary.
void UAEAdaptiveEnvWorldSubsystem::RegisterHeatmapRenderer(UAEHeatmapRendererComponent* Renderer)
{
	if (IsValid(Renderer))
	{
		PendingRendererAdds.AddUnique(Renderer);
	}
}

// Defer renderer removal until the next safe tick boundary.
void UAEAdaptiveEnvWorldSubsystem::UnregisterHeatmapRenderer(UAEHeatmapRendererComponent* Renderer)
{
	if (Renderer != nullptr)
	{
		PendingRendererRemoves.AddUnique(Renderer);
	}
}

// Forward a world-position cell query to the owned behaviour grid.
bool UAEAdaptiveEnvWorldSubsystem::GetBehaviourCellAtWorldLocation(const FVector& Location, FAEBehaviourCellSnapshot& OutSnapshot) const
{
	return BehaviourGrid.GetCellSnapshotAtWorldLocation(Location, OutSnapshot);
}

// Forward a coordinate cell query to the owned behaviour grid.
bool UAEAdaptiveEnvWorldSubsystem::GetBehaviourCell(const FIntPoint& Coordinate, FAEBehaviourCellSnapshot& OutSnapshot) const
{
	return BehaviourGrid.GetCellSnapshot(Coordinate, OutSnapshot);
}

// Return configured grid dimensions in cells.
FIntPoint UAEAdaptiveEnvWorldSubsystem::GetGridDimensions() const
{
	return BehaviourGrid.GetConfig().Dimensions;
}

// Return configured half-open world XY bounds.
FBox2D UAEAdaptiveEnvWorldSubsystem::GetGridWorldBounds() const
{
	return BehaviourGrid.GetWorldBounds();
}

// Return the current raw behaviour data revision.
int64 UAEAdaptiveEnvWorldSubsystem::GetBehaviourRevision() const
{
	return static_cast<int64>(BehaviourGrid.GetBehaviourRevision());
}

// Return the current per-tick dirty cell count.
int32 UAEAdaptiveEnvWorldSubsystem::GetDirtyCellCount() const
{
	return BehaviourGrid.GetDirtyCellIndices().Num();
}

// Return aggregate sample processing statistics by value.
FAEBehaviourGridStats UAEAdaptiveEnvWorldSubsystem::GetBehaviourGridStats() const
{
	return BehaviourGrid.GetStats();
}

// Reset all behaviour state while preserving active registrations.
void UAEAdaptiveEnvWorldSubsystem::ResetBehaviourGrid()
{
	// Clear grid data, queues, ordering guards, clocks, and scheduler counters.
	BehaviourGrid.Reset();
	ExposureGrid.Reset();
	PendingSamples.Reset();
	ProcessingSamples.Reset();
	LastQueuedSequenceByAgent.Reset();
	LastQueuedTimestampByAgent.Reset();
	BehaviourAccumulator = 0.0;
	BehaviourTimeSeconds = 0.0;
	ProcessedBehaviourStepCount = 0;
	SchedulerOverrunCount = 0;
	// Reset each valid tracker so its next observation is treated as the first.
	for (const TWeakObjectPtr<UAEBehaviourTrackerComponent>& Tracker : RegisteredTrackers)
	{
		if (Tracker.IsValid())
		{
			Tracker->ResetSamplingState();
		}
	}
}

/* Validates and atomically applies one complete M3/M4 published parameter bundle. */
bool UAEAdaptiveEnvWorldSubsystem::ApplyParameterBundle(UAEPublishedParameterBundleAsset* Bundle, FString& OutError)
{
	check(IsInGameThread());
	OutError.Reset();
	if (!IsValid(Bundle))
	{
		OutError = TEXT("Published parameter bundle is null or invalid.");
		return false;
	}

	// Validate the complete transport contract before constructing either model snapshot.
	const FAEParameterBundleValidationResult BundleResult = FAEParameterBundleService::ValidateBundle(*Bundle);
	if (!BundleResult.IsValid())
	{
		OutError = BundleResult.ToString();
		return false;
	}
	FAEParameterBlockView M3Block;
	FAEParameterBlockView M4Block;
	FAEParameterBundleService::FindBlock(*Bundle, FAEParameterBundleService::M3ModelContract(), M3Block);
	FAEParameterBundleService::FindBlock(*Bundle, FAEParameterBundleService::M4ModelContract(), M4Block);
	FAEActiveParameterSnapshot Candidate;
	Candidate.BundleIdentity = { Bundle->BundleId, Bundle->SemanticVersion, Bundle->ContentHash };
	const FAEM3ValidationResult M3Result = FAEM3ParameterService::BuildParameterSet(M3Block, Candidate.BundleIdentity, Candidate.M3);
	const FAEM4ValidationResult M4Result = FAEM4ParameterService::BuildParameterSet(M4Block, Candidate.BundleIdentity, Candidate.M4);
	if (!M3Result.IsValid() || !M4Result.IsValid())
	{
		OutError = FString::Printf(TEXT("M3=[%s] M4=[%s]"), *M3Result.ToString(), *M4Result.ToString());
		return false;
	}
	Candidate.M3BlockId = M3Block.Block->BlockId;
	Candidate.M3BlockVersion = M3Block.Block->BlockVersion;
	Candidate.M3BlockHash = M3Block.Block->BlockHash;
	Candidate.M4BlockId = M4Block.Block->BlockId;
	Candidate.M4BlockVersion = M4Block.Block->BlockVersion;
	Candidate.M4BlockHash = M4Block.Block->BlockHash;

	// Commit both model parameter sets together at the Game Thread boundary.
	const FString PreviousHash = ActiveParameters.BundleIdentity.ContentHash;
	ActiveParameters = MoveTemp(Candidate);
	bM3Enabled = true;
	bM4Enabled = true;
	RebuildM3FromCurrentRawGrid();
	UE_LOG(
		LogAdaptiveEnv,
		Log,
		TEXT("Published parameter bundle applied. World=%s OldHash=%s NewVersion=%s NewHash=%s"),
		*GetNameSafe(GetWorld()),
		*PreviousHash,
		*ActiveParameters.BundleIdentity.SemanticVersion,
		*ActiveParameters.BundleIdentity.ContentHash);
	return true;
}

/* Forward a coordinate query to the World-owned M3 Grid. */
bool UAEAdaptiveEnvWorldSubsystem::GetM3Cell(const FIntPoint& Coordinate, FAEM3CellSnapshot& OutSnapshot) const
{
	return bM3Enabled && ExposureGrid.GetCellSnapshot(Coordinate, OutSnapshot);
}

/* Forward a world-position query to the World-owned M3 Grid. */
bool UAEAdaptiveEnvWorldSubsystem::GetM3CellAtWorldLocation(const FVector& Location, FAEM3CellSnapshot& OutSnapshot) const
{
	return bM3Enabled && ExposureGrid.GetCellSnapshotAtWorldLocation(Location, OutSnapshot);
}

// Forward a bounded debug-cell query to the behaviour grid.
void UAEAdaptiveEnvWorldSubsystem::GetDebugCells(const FVector& Location, const float RadiusCm, const int32 MaxCells, TArray<FAEBehaviourCellSnapshot>& OutCells) const
{
	BehaviourGrid.GetNonEmptyCellsInRadius(Location, RadiusCm, MaxCells, OutCells);
}

/* Forward a bounded active-cell query to the World-owned M3 Grid. */
void UAEAdaptiveEnvWorldSubsystem::GetM3DebugCells(
	const FVector& Location,
	const float RadiusCm,
	const int32 MaxCells,
	TArray<FAEM3CellSnapshot>& OutCells) const
{
	if (!bM3Enabled)
	{
		OutCells.Reset();
		return;
	}
	ExposureGrid.GetActiveCellsInRadius(Location, RadiusCm, MaxCells, OutCells);
}

/* Resolve a stable parameter-aware normalization maximum for M3 debug colour. */
float UAEAdaptiveEnvWorldSubsystem::GetM3DebugMaximumValue(const EAEHeatmapDebugMode Mode) const
{
	switch (Mode)
	{
	case EAEHeatmapDebugMode::CurrentExposure:
		return static_cast<float>(ActiveParameters.M3.ExposureDynamics.Maximum);
	case EAEHeatmapDebugMode::DamageRate:
		return static_cast<float>(ActiveParameters.M3.DamageResponse.MaximumRatePerSimulationHour);
	case EAEHeatmapDebugMode::RecoveryRate:
		return static_cast<float>(ActiveParameters.M3.RecoveryResponse.BaseRatePerSimulationHour);
	case EAEHeatmapDebugMode::PassExposure:
	case EAEHeatmapDebugMode::TravelExposure:
	case EAEHeatmapDebugMode::DwellExposure:
	case EAEHeatmapDebugMode::SprintExposure:
	case EAEHeatmapDebugMode::CollectExposure:
	case EAEHeatmapDebugMode::CombatExposure:
	case EAEHeatmapDebugMode::EcologicalDamage:
		return 1.0f;
	default:
		return 0.0f;
	}
}

// Apply deferred removals before additions for trackers and renderers.
void UAEAdaptiveEnvWorldSubsystem::ApplyPendingRegistrations()
{
	// Remove requested and expired tracker references before adding new ones.
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

	// Remove requested and expired renderer references before adding new ones.
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

// Pull one sample from each valid tracker at the shared fixed time.
void UAEAdaptiveEnvWorldSubsystem::SampleRegisteredTrackers(const float StepSeconds)
{
	// Skip expired weak references without mutating the active array.
	for (const TWeakObjectPtr<UAEBehaviourTrackerComponent>& Tracker : RegisteredTrackers)
	{
		if (!Tracker.IsValid())
		{
			continue;
		}
		// Submit only complete samples produced by the tracker.
		FAEBehaviourSample Sample;
		if (Tracker->CaptureBehaviourSample(BehaviourTimeSeconds, StepSeconds, Sample))
		{
			SubmitBehaviourSample(Sample);
		}
	}
}

// Freeze, sort, and aggregate the current sample batch deterministically.
void UAEAdaptiveEnvWorldSubsystem::ProcessPendingSamples()
{
	// Swap producer and consumer buffers so new submissions remain isolated.
	Swap(PendingSamples, ProcessingSamples);
	PendingSamples.Reset();
	// Sort by timestamp, agent identity, and sequence for stable replay results.
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

	// Aggregate the stable batch on the Game Thread, then release it.
	for (const FAEBehaviourSample& Sample : ProcessingSamples)
	{
		BehaviourGrid.AccumulateSample(Sample);
	}
	ProcessingSamples.Reset();
}

/* Advance M3 immediately after one fixed-step raw aggregation stage. */
void UAEAdaptiveEnvWorldSubsystem::UpdateM3(const float StepSeconds)
{
	if (!bM3Enabled)
	{
		return;
	}

	// Convert the shared fixed real step into the only M3 simulation-time basis.
	const double DeltaSimulationHours = static_cast<double>(StepSeconds) * SimulationHoursPerRealSecond;
	const double SimulationTimeHours = BehaviourTimeSeconds * SimulationHoursPerRealSecond;
	if (!ExposureGrid.Update(
		BehaviourGrid,
		BehaviourGrid.GetDirtyCellIndices(),
		SimulationTimeHours,
		DeltaSimulationHours,
		BehaviourGrid.GetBehaviourRevision(),
		ActiveParameters.M3))
	{
		bM3Enabled = false;
		UE_LOG(LogAdaptiveEnv, Error, TEXT("M3 update failed and was disabled. World=%s BehaviourRevision=%llu"), *GetNameSafe(GetWorld()), BehaviourGrid.GetBehaviourRevision());
	}
}

/* Rebuild M3 deterministically from all current cumulative raw Cell totals. */
void UAEAdaptiveEnvWorldSubsystem::RebuildM3FromCurrentRawGrid()
{
	ExposureGrid.Reset();
	if (!bM3Enabled)
	{
		return;
	}

	// Treat every row-major Cell as dirty so current raw totals are consumed once under the new bundle.
	const FIntPoint Dimensions = BehaviourGrid.GetConfig().Dimensions;
	const int32 CellCount = Dimensions.X * Dimensions.Y;
	TArray<int32> AllCellIndices;
	AllCellIndices.Reserve(CellCount);
	for (int32 Index = 0; Index < CellCount; ++Index)
	{
		AllCellIndices.Add(Index);
	}
	const double SimulationTimeHours = BehaviourTimeSeconds * SimulationHoursPerRealSecond;
	if (!ExposureGrid.Update(
		BehaviourGrid,
		AllCellIndices,
		SimulationTimeHours,
		0.0,
		BehaviourGrid.GetBehaviourRevision(),
		ActiveParameters.M3))
	{
		bM3Enabled = false;
	}
}

// Refresh registered debug renderers at an independent configured rate.
void UAEAdaptiveEnvWorldSubsystem::UpdateDebugRenderers(const float DeltaTime)
{
	// Accumulate safe render time until one debug refresh is due.
	const UAdaptiveEnvSettings* Settings = GetDefault<UAdaptiveEnvSettings>();
	const double RefreshStep = 1.0 / FMath::Max(Settings->DebugRefreshRateHz, 0.1f);
	DebugAccumulator += FMath::Max(static_cast<double>(DeltaTime), 0.0);
	if (DebugAccumulator < RefreshStep)
	{
		return;
	}
	DebugAccumulator = FMath::Fmod(DebugAccumulator, RefreshStep);

	// Render only through valid weak registrations.
	for (const TWeakObjectPtr<UAEHeatmapRendererComponent>& Renderer : RegisteredRenderers)
	{
		if (Renderer.IsValid())
		{
			Renderer->RenderDebug(*this, static_cast<float>(RefreshStep));
		}
	}
}

// Validate sample identity, finite values, ranges, and supported tags.
bool UAEAdaptiveEnvWorldSubsystem::ValidateQueuedSample(const FAEBehaviourSample& Sample, EAEBehaviourSubmitResult& OutResult) const
{
	// Require a stable agent identity.
	if (!Sample.AgentId.IsValid())
	{
		OutResult = EAEBehaviourSubmitResult::InvalidAgent;
		return false;
	}
	// Reject invalid spatial inputs before distance or grid operations.
	if (Sample.WorldLocation.ContainsNaN()
		|| Sample.Velocity.ContainsNaN()
		|| (Sample.bHasPreviousLocation && Sample.PreviousWorldLocation.ContainsNaN()))
	{
		OutResult = EAEBehaviourSubmitResult::InvalidLocation;
		return false;
	}
	// Reject non-finite, negative, or invalid ordering values.
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

	// Accept only native behaviour tags understood by the raw grid.
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
