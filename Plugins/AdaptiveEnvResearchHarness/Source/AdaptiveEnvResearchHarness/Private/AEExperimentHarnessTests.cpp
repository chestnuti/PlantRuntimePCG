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
            TEXT("AE_EXP_FAILED RunId=P00_Pilot_S1337_R00 Reason=Adaptive Environment World Subsystem is unavailable."),
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
    return true;
}

#endif
