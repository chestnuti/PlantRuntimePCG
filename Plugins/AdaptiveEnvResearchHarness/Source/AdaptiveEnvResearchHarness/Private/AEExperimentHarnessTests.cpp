#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AEExperimentOrchestrator.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAEExperimentHarnessRejectsMissingRuntimeTest,
    "AdaptiveEnv.ResearchHarness.Validation.MissingRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAEExperimentHarnessRejectsMissingRuntimeTest::RunTest(const FString& Parameters)
{
    const FName WorldName = MakeUniqueObjectName(GetTransientPackage(), UWorld::StaticClass(), TEXT("AEHarnessTestWorld"));
    UWorld* World = UWorld::CreateWorld(EWorldType::None, false, WorldName, GetTransientPackage(), true);
    TestNotNull(TEXT("Temporary World"), World);
    if (World == nullptr)
    {
        return false;
    }

    AAEExperimentOrchestrator* Harness = World->SpawnActor<AAEExperimentOrchestrator>();
    TestNotNull(TEXT("Harness actor"), Harness);
    if (Harness != nullptr)
    {
        Harness->bAutoStartOnBeginPlay = false;
        AddExpectedError(
            TEXT("AE_EXP_FAILED RunId=E0_PILOT_S1337_R00 Reason=Adaptive Environment World Subsystem is unavailable."),
            EAutomationExpectedErrorFlags::Contains,
            1);
        TestFalse(TEXT("Experiment rejects a world without the runtime subsystem"), Harness->StartExperiment());
        TestEqual(TEXT("Harness reports failure"), Harness->State, EAEExperimentState::Failed);
        TestFalse(TEXT("Failure reason is populated"), Harness->FailureReason.IsEmpty());
    }

    World->DestroyWorld(false);
    World->RemoveFromRoot();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAEExperimentRunSpecDefaultsTest,
    "AdaptiveEnv.ResearchHarness.RunSpec.Defaults",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAEExperimentRunSpecDefaultsTest::RunTest(const FString& Parameters)
{
    const FAEExperimentRunSpec Spec;
    TestFalse(TEXT("Default RunId is usable"), Spec.RunId.IsEmpty());
    TestTrue(TEXT("Snapshot interval is positive"), Spec.SnapshotIntervalSeconds > 0.0f);
    TestTrue(TEXT("Default frame rate is non-negative"), Spec.TargetFrameRate >= 0);
    TestEqual(TEXT("Default experiment is E0"), Spec.Experiment, EAEExperimentType::E0);
    TestTrue(TEXT("Replay is enabled by default"), Spec.bReplayEnabled);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAEExperimentRunIdContractTest,
    "AdaptiveEnv.ResearchHarness.RunSpec.RunIdContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAEExperimentRunIdContractTest::RunTest(const FString& Parameters)
{
    const FString RunId = AAEExperimentOrchestrator::BuildRunId(EAEExperimentType::E2, TEXT("C1"), 1337, 2);
    TestEqual(TEXT("Canonical RunId"), RunId, FString(TEXT("E2_C1_S1337_R02")));
    TestTrue(TEXT("Canonical RunId is accepted"), AAEExperimentOrchestrator::IsValidRunId(RunId));
    TestTrue(TEXT("E0 pilot RunId is accepted"), AAEExperimentOrchestrator::IsValidRunId(TEXT("E0_PILOT_S1337_R00")));
    TestFalse(TEXT("Legacy RunId is rejected"), AAEExperimentOrchestrator::IsValidRunId(TEXT("P00_Pilot_S1337_R00")));
    TestFalse(TEXT("Unsafe RunId is rejected"), AAEExperimentOrchestrator::IsValidRunId(TEXT("E2_../Bad_S1337_R00")));
    return true;
}

#endif
