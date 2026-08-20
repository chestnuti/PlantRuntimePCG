#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AEAdaptiveEnvironmentProfile.h"
#include "AdaptiveEnvGameplayTags.h"
#include "AdaptiveEnvSettings.h"
#include "AdaptiveEnvWorldSubsystem.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAEGameplayTagsTest,
	"AdaptiveEnv.M0.GameplayTags",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Verify that all native behaviour tags resolve to stable names.
bool FAEGameplayTagsTest::RunTest(const FString& Parameters)
{
	// Compare every exported tag against its serialized identifier.
	TestEqual(TEXT("Move tag"), AdaptiveEnvGameplayTags::Behaviour_Move.GetTag().ToString(), FString(TEXT("Behaviour.Move")));
	TestEqual(TEXT("Dwell tag"), AdaptiveEnvGameplayTags::Behaviour_Dwell.GetTag().ToString(), FString(TEXT("Behaviour.Dwell")));
	TestEqual(TEXT("Sprint tag"), AdaptiveEnvGameplayTags::Behaviour_Sprint.GetTag().ToString(), FString(TEXT("Behaviour.Sprint")));
	TestEqual(TEXT("Collect tag"), AdaptiveEnvGameplayTags::Behaviour_Collect.GetTag().ToString(), FString(TEXT("Behaviour.Collect")));
	TestEqual(TEXT("Combat tag"), AdaptiveEnvGameplayTags::Behaviour_Combat.GetTag().ToString(), FString(TEXT("Behaviour.Combat")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAESettingsDefaultsTest,
	"AdaptiveEnv.M0.SettingsDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Verify that runtime settings expose valid baseline defaults.
bool FAESettingsDefaultsTest::RunTest(const FString& Parameters)
{
	// Load the immutable class default settings object.
	const UAdaptiveEnvSettings* Settings = GetDefault<UAdaptiveEnvSettings>();

	// Assert availability, positive spatial values, and the expected schema.
	TestNotNull(TEXT("Settings object"), Settings);
	TestTrue(TEXT("Runtime enabled"), Settings->bEnableRuntime);
	TestTrue(TEXT("Sample rate is positive"), Settings->BehaviourSampleRateHz > 0.0f);
	TestTrue(TEXT("Grid width is positive"), Settings->GridWidth > 0);
	TestTrue(TEXT("Grid height is positive"), Settings->GridHeight > 0);
	TestTrue(TEXT("Cell size is positive"), Settings->CellSizeCm > 0.0f);
	TestTrue(TEXT("Adaptive ecology enabled by default"), Settings->bEnableAdaptiveEcology);
	TestTrue(TEXT("M6 enabled by default"), Settings->bEnableM6);
	TestTrue(
		TEXT("Optional environment profile path is syntactically valid"),
		Settings->EnvironmentProfile.IsNull() || Settings->EnvironmentProfile.ToSoftObjectPath().IsValid());
	TestEqual(TEXT("Settings schema"), Settings->SettingsSchemaVersion, 14);
	TestEqual(TEXT("M6 visible threshold"), Settings->M6VisibleDamageThresholdRatio, 0.15f);
	TestEqual(TEXT("M6 full-path threshold"), Settings->M6FullPathDamageThresholdRatio, 0.65f);
	TestEqual(TEXT("M6 formation rate"), Settings->M6FormationRatePerSimulationHour, 0.20f);
	TestEqual(TEXT("M6 fade rate"), Settings->M6FadeRatePerSimulationHour, 0.08f);
	TestEqual(TEXT("M6 apply rate"), Settings->M6VisualApplyRateHz, 5.0f);
	TestEqual(TEXT("M6 command budget"), Settings->M6MaxVisualCommandsPerFrame, 1024);
	TestTrue(TEXT("Debug text budget reserves engine capacity"), Settings->MaxDebugTextLabels >= 0 && Settings->MaxDebugTextLabels <= 96);
	TestEqual(TEXT("Debug activity neighbourhood"), Settings->DebugActiveNeighbourRadiusCells, 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAEDataAssetSchemaTest,
	"AdaptiveEnv.M0.DataAssetSchema",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Verify default product Profile identity, version, and tuning values.
bool FAEDataAssetSchemaTest::RunTest(const FString& Parameters)
{
	const UAEAdaptiveEnvironmentProfile* Profile = NewObject<UAEAdaptiveEnvironmentProfile>();
	TestEqual(TEXT("Default Profile identity"), Profile->ProfileId, FName(TEXT("Default")));
	TestEqual(TEXT("Profile contract version"), Profile->ConfigVersion, 2);
	TestEqual(TEXT("Default M3 maximum"), Profile->M3.MaximumExposure, 1.0);
	TestEqual(TEXT("Default M4 unsuitable slope"), Profile->M4.SlopeUnsuitableDegrees, 45.0);
	TestEqual(TEXT("Default M5 Damage rate"), Profile->M5.DamageMaximumRatePerSimulationHour, 0.20);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAEWorldSubsystemSingletonTest,
	"AdaptiveEnv.M0.WorldSubsystemSingleton",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Verify that each supported World owns exactly one adaptive subsystem.
bool FAEWorldSubsystemSingletonTest::RunTest(const FString& Parameters)
{
	// Create an isolated playable World for subsystem lookup.
	const FName WorldName = MakeUniqueObjectName(GetTransientPackage(), UWorld::StaticClass(), TEXT("AE_M0_TestWorld"));
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false, WorldName, GetTransientPackage(), true);
	TestNotNull(TEXT("Temporary world"), TestWorld);

	if (TestWorld == nullptr)
	{
		return false;
	}

	// Request the subsystem twice and compare object identity.
	UAEAdaptiveEnvWorldSubsystem* First = TestWorld->GetSubsystem<UAEAdaptiveEnvWorldSubsystem>();
	UAEAdaptiveEnvWorldSubsystem* Second = TestWorld->GetSubsystem<UAEAdaptiveEnvWorldSubsystem>();

	TestNotNull(TEXT("Adaptive subsystem"), First);
	TestTrue(TEXT("One subsystem per world"), First == Second);
	TestTrue(TEXT("Instance ID is valid"), First != nullptr && First->GetInstanceId().IsValid());
	TestFalse(TEXT("Observation pause is disabled initially"), First != nullptr && First->IsObservationPaused());
	if (First != nullptr)
	{
		const double TimeBeforePause = First->GetBehaviourTimeSeconds();
		First->SetObservationPaused(true);
		TestTrue(TEXT("Observation pause can freeze the scheduler"), First->IsObservationPaused());
		TestTrue(TEXT("Subsystem remains tickable for paused debug rendering"), First->IsTickable());
		First->Tick(1.0f);
		TestEqual(TEXT("Observation pause does not advance behaviour time"), First->GetBehaviourTimeSeconds(), TimeBeforePause);
		First->SetObservationPaused(false);
		TestFalse(TEXT("Observation pause can resume the scheduler"), First->IsObservationPaused());
	}

	// Destroy the transient World after all assertions complete.
	TestWorld->DestroyWorld(false);
	TestWorld->RemoveFromRoot();
	return true;
}

#endif
