#include "AEExperimentOrchestrator.h"

#include "AdaptiveEnvWorldSubsystem.h"
#include "AEAdaptiveEnvironmentProfile.h"
#include "AELSystemPlantComponent.h"
#include "AERepresentativePlantManagerComponent.h"
#include "AEVegetationDistributionComponent.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "JsonObjectConverter.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Internationalization/Regex.h"
#include "PlayerReplayManager.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"

namespace AEExperimentPrivate
{
    const TCHAR* CellHeader =
        TEXT("RunId,Condition,Repeat,Seed,TimeSeconds,SimulationStep,CellX,CellY,")
        TEXT("PassCount,TravelDistanceMeters,DwellSeconds,SprintDistanceMeters,CollectEventCount,CombatEventCount,")
        TEXT("FlowX,FlowY,FlowMagnitude,BehaviourRevision,ConfigRevision,")
        TEXT("PassExposure,TravelExposure,DwellExposure,SprintExposure,CollectExposure,CombatExposure,CurrentExposure,ExposureRevision,")
        TEXT("SlopeDegrees,MoistureRatio,ConstraintPressureRatio,HabitatSuitabilityRatio,EnvironmentState,ConstraintRevision,")
        TEXT("EffectiveImpactRatio,DamageRatio,RecoveryRatio,DamageRatePerSimulationHour,RecoveryRatePerSimulationHour,")
        TEXT("SourceExposureRevision,SourceConstraintRevision,ResponseRevision,PathIntensity,PathFlowX,PathFlowY,PathVisualRevision");

    const TCHAR* PlantHeader =
        TEXT("RunId,StablePointId,SpeciesId,CellX,CellY,WorldX,WorldY,WorldZ,HealthRatio,DistributionRatio,")
        TEXT("EnvironmentSuitabilityRatio,RecoveryRatePerSimulationHour,Visible,LifecycleState,LifecycleProgressRatio,DeathFadeRatio,SimulationStep");

    const TCHAR* LSystemHeader =
        TEXT("RunId,Actor,SourceStablePointId,Generated,BranchSegments,LeafEmitters,LeafInstances,LiveDetachedBranches,ContentHash");

    const TCHAR* PerformanceHeader =
        TEXT("RunId,Condition,Repeat,SampleIndex,TimeSeconds,SimulationStep,FrameTimeMs,SubsystemTickTimeMs,")
        TEXT("SchedulerOverrunCount,DirtyCellCount,M7PlantCount,M8ActivePlantCount");

    const TCHAR* EventHeader = TEXT("RunId,TimeSeconds,SimulationStep,Event,Detail");
}

AAEExperimentOrchestrator::AAEExperimentOrchestrator()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostUpdateWork;
}

void AAEExperimentOrchestrator::BeginPlay()
{
    Super::BeginPlay();
    if (bAutoStartOnBeginPlay)
    {
        StartExperiment();
    }
}

void AAEExperimentOrchestrator::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (IConsoleVariable* MaxFps = IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS")))
    {
        MaxFps->Set(PreviousMaxFps, ECVF_SetByCode);
    }
    if (Environment != nullptr)
    {
        Environment->SetObservationPaused(bPreviousObservationPaused);
    }
    Super::EndPlay(EndPlayReason);
}

void AAEExperimentOrchestrator::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (State == EAEExperimentState::Idle || State == EAEExperimentState::Completed || State == EAEExperimentState::Failed)
    {
        return;
    }

    const float SafeDeltaSeconds = FMath::Max(DeltaSeconds, 0.0f);
    RunElapsedSeconds += SafeDeltaSeconds;
    StateElapsedSeconds += SafeDeltaSeconds;
    CollectSnapshot(SafeDeltaSeconds);
    AdvanceState(SafeDeltaSeconds);
}

bool AAEExperimentOrchestrator::StartExperiment()
{
    if (State != EAEExperimentState::Idle)
    {
        return false;
    }

    State = EAEExperimentState::Validating;
    if (!ValidateAndPrepare())
    {
        return false;
    }

    State = EAEExperimentState::WarmingUp;
    StateElapsedSeconds = 0.0;
    AppendEvent(TEXT("Ready"));
    UE_LOG(LogTemp, Display, TEXT("AE_EXP_READY RunId=%s"), *RunSpec.RunId);
    return true;
}

void AAEExperimentOrchestrator::AbortExperiment(const FString& Reason)
{
    if (State != EAEExperimentState::Completed && State != EAEExperimentState::Failed)
    {
        FailExperiment(Reason.IsEmpty() ? TEXT("Experiment aborted.") : Reason);
    }
}

FString AAEExperimentOrchestrator::BuildRunId(
    const EAEExperimentType Experiment,
    const FName ConditionId,
    const int32 Seed,
    const int32 RepeatIndex)
{
    const FString ExperimentName = StaticEnum<EAEExperimentType>()->GetNameStringByValue(static_cast<int64>(Experiment));
    FString SafeCondition = ConditionId.IsNone() ? TEXT("C0") : ConditionId.ToString();
    SafeCondition.ReplaceCharInline(TEXT(' '), TEXT('_'));
    return FString::Printf(TEXT("%s_%s_S%d_R%02d"), *ExperimentName, *SafeCondition, Seed, FMath::Max(RepeatIndex, 0));
}

bool AAEExperimentOrchestrator::IsValidRunId(const FString& RunId)
{
    const FRegexPattern Pattern(TEXT("^E[0-5]_[A-Za-z0-9][A-Za-z0-9_-]*_S-?[0-9]+_R[0-9]{2,}$"));
    FRegexMatcher Matcher(Pattern, RunId);
    return Matcher.FindNext() && Matcher.GetMatchBeginning() == 0 && Matcher.GetMatchEnding() == RunId.Len();
}

bool AAEExperimentOrchestrator::ValidateAndPrepare()
{
    if (!IsValidRunId(RunSpec.RunId))
    {
        FailExperiment(TEXT("RunId must use E{0-5}_{Condition}_S{Seed}_R{Repeat} and contain only safe characters."));
        return false;
    }
    if (!FMath::IsFinite(RunSpec.WarmupSeconds) || RunSpec.WarmupSeconds < 0.0f
        || !FMath::IsFinite(RunSpec.RecoverySeconds) || RunSpec.RecoverySeconds < 0.0f
        || !FMath::IsFinite(RunSpec.SnapshotIntervalSeconds) || RunSpec.SnapshotIntervalSeconds <= 0.0f)
    {
        FailExperiment(TEXT("Run timing values are invalid."));
        return false;
    }

    UWorld* World = GetWorld();
    Environment = World != nullptr ? World->GetSubsystem<UAEAdaptiveEnvWorldSubsystem>() : nullptr;
    if (Environment == nullptr)
    {
        FailExperiment(TEXT("Adaptive Environment World Subsystem is unavailable."));
        return false;
    }

    OutputDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("AdaptiveEnvResearch"), RunSpec.RunId);
    if (!RunSpec.bAllowOverwrite && IFileManager::Get().DirectoryExists(*OutputDirectory))
    {
        const FString ConflictingRunId = RunSpec.RunId;
        OutputDirectory.Reset();
        FailExperiment(FString::Printf(TEXT("Output directory already exists for RunId %s."), *ConflictingRunId));
        return false;
    }

    for (TActorIterator<APlayerReplayManager> It(World); It; ++It)
    {
        ReplayManager = *It;
        break;
    }
    if (ReplayManager == nullptr)
    {
        FailExperiment(TEXT("Player Replay Manager is unavailable."));
        return false;
    }

    ReplayManager->SaveSlotName = RunSpec.ReplaySlotName;
    if (!ReplayManager->LoadRecording())
    {
        FailExperiment(FString::Printf(TEXT("Replay load failed: %s"), *ReplayManager->LastPersistenceError));
        return false;
    }
    if (RunSpec.Profile != nullptr)
    {
        FString ProfileError;
        if (!Environment->ApplyEnvironmentProfile(RunSpec.Profile, ProfileError))
        {
            FailExperiment(FString::Printf(TEXT("Profile apply failed: %s"), *ProfileError));
            return false;
        }
    }

    if (RunSpec.bApplySeedToRuntime)
    {
        FString SeedError;
        if (!ApplyExperimentSeed(SeedError))
        {
            FailExperiment(SeedError);
            return false;
        }
    }

    bPreviousObservationPaused = Environment->IsObservationPaused();
    Environment->SetObservationPaused(!RunSpec.bRuntimeEnabled);

    if (IConsoleVariable* MaxFps = IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS")))
    {
        PreviousMaxFps = MaxFps->GetFloat();
        MaxFps->Set(FMath::Max(RunSpec.TargetFrameRate, 0), ECVF_SetByCode);
    }

    IFileManager::Get().MakeDirectory(*OutputDirectory, true);
    CellRows = {AEExperimentPrivate::CellHeader};
    PlantRows = {AEExperimentPrivate::PlantHeader};
    LSystemRows = {AEExperimentPrivate::LSystemHeader};
    PerformanceRows = {AEExperimentPrivate::PerformanceHeader};
    EventRows = {AEExperimentPrivate::EventHeader};
    AppendEvent(TEXT("Prepared"), FString::Printf(TEXT("ReplaySlot=%s"), *RunSpec.ReplaySlotName));
    return true;
}

bool AAEExperimentOrchestrator::ApplyExperimentSeed(FString& OutError)
{
    AppliedM7DistributionCount = 0;
    AppliedM8PlantCount = 0;
    UWorld* World = GetWorld();
    if (World == nullptr)
    {
        OutError = TEXT("Cannot apply experiment Seed without a World.");
        return false;
    }

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        TInlineComponentArray<UAEVegetationDistributionComponent*> Distributions(*It);
        for (UAEVegetationDistributionComponent* Distribution : Distributions)
        {
            if (IsValid(Distribution))
            {
                Distribution->DistributionSeed = RunSpec.Seed;
                if (!Distribution->RebuildStructuralDistribution())
                {
                    OutError = FString::Printf(TEXT("M7 distribution rebuild failed after applying Seed to %s."), *GetNameSafe(Distribution));
                    return false;
                }
                ++AppliedM7DistributionCount;
            }
        }

        TInlineComponentArray<UAELSystemPlantComponent*> Plants(*It);
        for (UAELSystemPlantComponent* Plant : Plants)
        {
            if (IsValid(Plant))
            {
                Plant->GenerationSeed = RunSpec.Seed;
                ++AppliedM8PlantCount;
            }
        }
    }

    if (AppliedM7DistributionCount == 0)
    {
        OutError = TEXT("Experiment Seed could not be applied because no M7 distribution component was found.");
        return false;
    }
    return true;
}

void AAEExperimentOrchestrator::AdvanceState(const float DeltaSeconds)
{
    if (State == EAEExperimentState::WarmingUp && StateElapsedSeconds >= RunSpec.WarmupSeconds)
    {
        if (!ReplayManager->StartReplay())
        {
            FailExperiment(TEXT("Replay failed to start."));
            return;
        }
        bReplayStarted = true;
        bPreviousReplayState = true;
        State = EAEExperimentState::Replaying;
        StateElapsedSeconds = 0.0;
        AppendEvent(TEXT("ReplayStarted"));
        UE_LOG(LogTemp, Display, TEXT("AE_EXP_REPLAY_STARTED RunId=%s"), *RunSpec.RunId);
        return;
    }

    if (State == EAEExperimentState::Replaying)
    {
        const bool bIsReplaying = ReplayManager->IsReplaying();
        if (bReplayStarted && bPreviousReplayState && !bIsReplaying)
        {
            State = EAEExperimentState::Recovering;
            StateElapsedSeconds = 0.0;
            AppendEvent(TEXT("ReplayCompleted"));
        }
        bPreviousReplayState = bIsReplaying;
    }

    if (State == EAEExperimentState::Recovering && StateElapsedSeconds >= RunSpec.RecoverySeconds)
    {
        CompleteExperiment();
    }
}

void AAEExperimentOrchestrator::CollectSnapshot(const float DeltaSeconds)
{
    if (Environment == nullptr || State == EAEExperimentState::Validating || State == EAEExperimentState::Preparing)
    {
        return;
    }

    const int64 CurrentStep = Environment->GetProcessedBehaviourStepCount();
    SnapshotAccumulatorSeconds += DeltaSeconds;
    if (CurrentStep != LastCollectedStep && SnapshotAccumulatorSeconds >= RunSpec.SnapshotIntervalSeconds)
    {
        LastCollectedStep = CurrentStep;
        SnapshotAccumulatorSeconds = 0.0;
        CollectCells();

        TArray<FAEPlantInstanceSnapshot> Plants;
        Environment->GetM7PlantInstanceStates(Plants);
        int32 ActiveM8 = 0;
        if (UWorld* World = GetWorld())
        {
            for (TActorIterator<AActor> It(World); It; ++It)
            {
                if (const UAERepresentativePlantManagerComponent* Manager = It->FindComponentByClass<UAERepresentativePlantManagerComponent>())
                {
                    ActiveM8 += Manager->GetActiveRepresentativePlantCount();
                }
            }
        }
        PerformanceRows.Add(FString::Printf(
            TEXT("%s,%s,%d,%lld,%.6f,%lld,%.6f,%.6f,%lld,%d,%d,%d"),
            *CsvEscape(RunSpec.RunId), *CsvEscape(RunSpec.ConditionId.ToString()), RunSpec.RepeatIndex,
            SampleIndex++, RunElapsedSeconds, CurrentStep, DeltaSeconds * 1000.0f,
            Environment->GetLastTickTimeMilliseconds(),
            Environment->GetSchedulerOverrunCount(), Environment->GetDirtyCellCount(), Plants.Num(), ActiveM8));
    }

    for (const FAEExperimentCapturePoint& Point : RunSpec.CapturePoints)
    {
        if (!Point.CaptureId.IsNone() && CurrentStep >= Point.SimulationStep && !EmittedCapturePoints.Contains(Point.CaptureId))
        {
            EmittedCapturePoints.Add(Point.CaptureId);
            AppendEvent(TEXT("CaptureRequired"), Point.CaptureId.ToString());
            UE_LOG(LogTemp, Display, TEXT("AE_EXP_CAPTURE_REQUIRED RunId=%s CaptureId=%s Step=%lld"),
                *RunSpec.RunId, *Point.CaptureId.ToString(), CurrentStep);
        }
    }
}

void AAEExperimentOrchestrator::CollectCells()
{
    TArray<FIntPoint> Coordinates = RunSpec.ObservedCells;
    if (Coordinates.IsEmpty() && ReplayManager != nullptr && ReplayManager->TargetCharacter != nullptr)
    {
        FAEBehaviourCellSnapshot AtCharacter;
        if (Environment->GetBehaviourCellAtWorldLocation(ReplayManager->TargetCharacter->GetActorLocation(), AtCharacter))
        {
            Coordinates.Add(AtCharacter.Coordinate);
        }
    }

    for (const FIntPoint Coordinate : Coordinates)
    {
        FAEBehaviourCellSnapshot M1;
        FAEM3CellSnapshot M3;
        FAEEnvironmentConstraintSnapshot M4;
        FAEEcologicalResponseSnapshot M5;
        FAEPathHeatmapSnapshot M6;
        Environment->GetBehaviourCell(Coordinate, M1);
        Environment->GetM3Cell(Coordinate, M3);
        Environment->GetM4Cell(Coordinate, M4);
        Environment->GetM5Cell(Coordinate, M5);
        Environment->GetM6Cell(Coordinate, M6);

        CellRows.Add(FString::Printf(
            TEXT("%s,%s,%d,%d,%.6f,%lld,%d,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%lld,%lld,")
            TEXT("%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%lld,%.6f,%.6f,%.6f,%.6f,%d,%lld,")
            TEXT("%.6f,%.6f,%.6f,%.6f,%.6f,%lld,%lld,%lld,%.6f,%.6f,%.6f,%lld"),
            *CsvEscape(RunSpec.RunId), *CsvEscape(RunSpec.ConditionId.ToString()), RunSpec.RepeatIndex, RunSpec.Seed,
            Environment->GetBehaviourTimeSeconds(), Environment->GetProcessedBehaviourStepCount(), Coordinate.X, Coordinate.Y,
            M1.PassCount, M1.TravelDistanceMeters, M1.DwellSeconds, M1.SprintDistanceMeters, M1.CollectEventCount, M1.CombatEventCount,
            M1.FlowDirection.X, M1.FlowDirection.Y, M1.FlowMagnitude, Environment->GetBehaviourRevision(), Environment->GetEnvironmentConfigRevision(),
            M3.PassExposure, M3.TravelExposure, M3.DwellExposure, M3.SprintExposure, M3.CollectExposure, M3.CombatExposure,
            M3.CurrentExposure, M3.ExposureRevision, M4.SlopeDegrees, M4.MoistureRatio, M4.ConstraintPressureRatio,
            M4.HabitatSuitabilityRatio, static_cast<int32>(M4.State), M4.ConstraintRevision,
            M5.EffectiveImpactRatio, M5.DamageRatio, M5.RecoveryRatio,
            M5.DamageRatePerSimulationHour, M5.RecoveryRatePerSimulationHour,
            M5.SourceExposureRevision, M5.SourceConstraintRevision, M5.ResponseRevision,
            M6.PathIntensity, M6.FlowVector.X, M6.FlowVector.Y, M6.PathVisualRevision));
    }
}

void AAEExperimentOrchestrator::CollectPlantsAndLSystems()
{
    TArray<FAEPlantInstanceSnapshot> Plants;
    Environment->GetM7PlantInstanceStates(Plants);
    for (const FAEPlantInstanceSnapshot& Plant : Plants)
    {
        PlantRows.Add(FString::Printf(
            TEXT("%s,%lld,%s,%d,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%d,%d,%.6f,%.6f,%lld"),
            *CsvEscape(RunSpec.RunId), Plant.StablePointId, *CsvEscape(Plant.SpeciesId.ToString()),
            Plant.CellCoordinate.X, Plant.CellCoordinate.Y, Plant.WorldLocation.X, Plant.WorldLocation.Y, Plant.WorldLocation.Z,
            Plant.HealthRatio, Plant.DistributionRatio, Plant.EnvironmentSuitabilityRatio,
            Plant.EffectiveRecoveryRatePerSimulationHour, Plant.bVisible ? 1 : 0, static_cast<int32>(Plant.LifecycleState),
            Plant.LifecycleProgressRatio, Plant.DeathFadeRatio, Plant.SimulationStep));
    }

    if (UWorld* World = GetWorld())
    {
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            if (const UAELSystemPlantComponent* Plant = It->FindComponentByClass<UAELSystemPlantComponent>())
            {
                LSystemRows.Add(FString::Printf(
                    TEXT("%s,%s,%lld,%d,%d,%d,%d,%d,%lld"),
                    *CsvEscape(RunSpec.RunId), *CsvEscape(It->GetName()), Plant->SourceStablePointId,
                    Plant->IsPlantGenerated() ? 1 : 0, Plant->GetBranchSegmentCount(), Plant->GetLeafEmitterCount(),
                    Plant->GetLeafInstanceCount(), Plant->GetLiveDetachedBranchCount(), Plant->GetGeneratedContentHash()));
            }
        }
    }
}

void AAEExperimentOrchestrator::CompleteExperiment()
{
    State = EAEExperimentState::Flushing;
    CollectCells();
    CollectPlantsAndLSystems();
    AppendEvent(TEXT("Completed"));
    FString Error;
    if (!FlushResults(TEXT("Completed"), Error))
    {
        FailExperiment(Error);
        return;
    }
    State = EAEExperimentState::Completed;
    UE_LOG(LogTemp, Display, TEXT("AE_EXP_COMPLETE RunId=%s Output=%s"), *RunSpec.RunId, *OutputDirectory);
}

void AAEExperimentOrchestrator::FailExperiment(const FString& Reason)
{
    FailureReason = Reason;
    State = EAEExperimentState::Failed;
    AppendEvent(TEXT("Failed"), Reason);
    FString Ignored;
    if (!OutputDirectory.IsEmpty())
    {
        FlushResults(TEXT("Failed"), Ignored);
    }
    UE_LOG(LogTemp, Error, TEXT("AE_EXP_FAILED RunId=%s Reason=%s"), *RunSpec.RunId, *Reason);
}

bool AAEExperimentOrchestrator::FlushResults(const FString& Status, FString& OutError)
{
    IFileManager::Get().MakeDirectory(*OutputDirectory, true);
    const auto SaveLines = [this, &OutError](const TCHAR* FileName, const TArray<FString>& Lines)
    {
        const FString Path = FPaths::Combine(OutputDirectory, FileName);
        if (!FFileHelper::SaveStringArrayToFile(Lines, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
        {
            OutError = FString::Printf(TEXT("Failed to write %s."), *Path);
            return false;
        }
        return true;
    };

    if (!FFileHelper::SaveStringToFile(MakeManifestJson(Status), *FPaths::Combine(OutputDirectory, TEXT("manifest.json")),
        FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
        || !SaveLines(TEXT("cell_snapshots.csv"), CellRows)
        || !SaveLines(TEXT("plant_snapshots.csv"), PlantRows)
        || !SaveLines(TEXT("lsystem_snapshots.csv"), LSystemRows)
        || !SaveLines(TEXT("performance.csv"), PerformanceRows)
        || !SaveLines(TEXT("events.csv"), EventRows))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("Failed to write experiment results.");
        }
        return false;
    }
    return true;
}

void AAEExperimentOrchestrator::AppendEvent(const FString& Event, const FString& Detail)
{
    const int64 Step = Environment != nullptr ? Environment->GetProcessedBehaviourStepCount() : 0;
    EventRows.Add(FString::Printf(TEXT("%s,%.6f,%lld,%s,%s"), *CsvEscape(RunSpec.RunId), RunElapsedSeconds,
        Step, *CsvEscape(Event), *CsvEscape(Detail)));
}

FString AAEExperimentOrchestrator::MakeManifestJson(const FString& Status) const
{
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("runId"), RunSpec.RunId);
    Root->SetStringField(TEXT("experiment"), StaticEnum<EAEExperimentType>()->GetNameStringByValue(static_cast<int64>(RunSpec.Experiment)));
    Root->SetStringField(TEXT("condition"), RunSpec.ConditionId.ToString());
    Root->SetNumberField(TEXT("repeat"), RunSpec.RepeatIndex);
    Root->SetNumberField(TEXT("seed"), RunSpec.Seed);
    Root->SetStringField(TEXT("replaySlot"), RunSpec.ReplaySlotName);
    Root->SetStringField(TEXT("map"), GetWorld() != nullptr ? UGameplayStatics::GetCurrentLevelName(this, true) : FString());
    Root->SetStringField(TEXT("profile"), Environment != nullptr ? Environment->GetActiveEnvironmentProfileId().ToString() : FString());
    Root->SetNumberField(TEXT("configRevision"), Environment != nullptr ? Environment->GetEnvironmentConfigRevision() : 0);
    Root->SetNumberField(TEXT("targetFrameRate"), RunSpec.TargetFrameRate);
    Root->SetStringField(TEXT("workload"), RunSpec.WorkloadLevel.ToString());
    Root->SetNumberField(TEXT("warmupSeconds"), RunSpec.WarmupSeconds);
    Root->SetNumberField(TEXT("recoverySeconds"), RunSpec.RecoverySeconds);
    Root->SetNumberField(TEXT("snapshotIntervalSeconds"), RunSpec.SnapshotIntervalSeconds);
    Root->SetBoolField(TEXT("runtimeEnabled"), RunSpec.bRuntimeEnabled);
    Root->SetStringField(TEXT("engineVersion"), FEngineVersion::Current().ToString());
    Root->SetStringField(TEXT("buildConfiguration"), LexToString(FApp::GetBuildConfiguration()));
    Root->SetStringField(TEXT("status"), Status);
    Root->SetStringField(TEXT("failureReason"), FailureReason);
    Root->SetNumberField(TEXT("durationSeconds"), RunElapsedSeconds);
    Root->SetNumberField(TEXT("finalSimulationStep"), Environment != nullptr ? Environment->GetProcessedBehaviourStepCount() : 0);
    Root->SetNumberField(TEXT("appliedM7DistributionCount"), AppliedM7DistributionCount);
    Root->SetNumberField(TEXT("appliedM8PlantCount"), AppliedM8PlantCount);
    Root->SetNumberField(TEXT("meanSubsystemTickTimeMs"), Environment != nullptr ? Environment->GetMeanTickTimeMilliseconds() : 0.0);
    Root->SetNumberField(TEXT("maximumSubsystemTickTimeMs"), Environment != nullptr ? Environment->GetMaximumTickTimeMilliseconds() : 0.0);

    TArray<TSharedPtr<FJsonValue>> Cells;
    for (const FIntPoint Cell : RunSpec.ObservedCells)
    {
        TSharedRef<FJsonObject> CellObject = MakeShared<FJsonObject>();
        CellObject->SetNumberField(TEXT("x"), Cell.X);
        CellObject->SetNumberField(TEXT("y"), Cell.Y);
        Cells.Add(MakeShared<FJsonValueObject>(CellObject));
    }
    Root->SetArrayField(TEXT("observedCells"), Cells);

    FString Json;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
    FJsonSerializer::Serialize(Root, Writer);
    return Json;
}

FString AAEExperimentOrchestrator::CsvEscape(const FString& Value)
{
    FString Escaped = Value;
    Escaped.ReplaceInline(TEXT("\""), TEXT("\"\""));
    return FString::Printf(TEXT("\"%s\""), *Escaped);
}
