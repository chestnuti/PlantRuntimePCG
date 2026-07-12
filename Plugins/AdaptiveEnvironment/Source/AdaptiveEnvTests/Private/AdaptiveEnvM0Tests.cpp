#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AdaptiveEnvDataAssets.h"
#include "AdaptiveEnvGameplayTags.h"
#include "AdaptiveEnvSettings.h"
#include "AdaptiveEnvWorldSubsystem.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAEGameplayTagsTest,
	"AdaptiveEnv.M0.GameplayTags",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAEGameplayTagsTest::RunTest(const FString& Parameters)
{
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

bool FAESettingsDefaultsTest::RunTest(const FString& Parameters)
{
	const UAdaptiveEnvSettings* Settings = GetDefault<UAdaptiveEnvSettings>();

	TestNotNull(TEXT("Settings object"), Settings);
	TestTrue(TEXT("Runtime enabled"), Settings->bEnableRuntime);
	TestTrue(TEXT("Sample rate is positive"), Settings->BehaviourSampleRateHz > 0.0f);
	TestTrue(TEXT("Grid width is positive"), Settings->GridWidth > 0);
	TestTrue(TEXT("Grid height is positive"), Settings->GridHeight > 0);
	TestTrue(TEXT("Cell size is positive"), Settings->CellSizeCm > 0.0f);
	TestEqual(TEXT("Settings schema"), Settings->SettingsSchemaVersion, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAEDataAssetSchemaTest,
	"AdaptiveEnv.M0.DataAssetSchema",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAEDataAssetSchemaTest::RunTest(const FString& Parameters)
{
	const UAELiteratureEvidenceAsset* Evidence = NewObject<UAELiteratureEvidenceAsset>();
	const UAEParameterSynthesisAsset* ParametersAsset = NewObject<UAEParameterSynthesisAsset>();
	const UAEExperimentConfig* Experiment = NewObject<UAEExperimentConfig>();

	TestEqual(TEXT("Evidence schema"), Evidence->SchemaVersion, 1);
	TestEqual(TEXT("Parameter schema"), ParametersAsset->Metadata.SchemaVersion, 1);
	TestEqual(TEXT("Experiment schema"), Experiment->SchemaVersion, 1);
	TestEqual(TEXT("Semantic version"), ParametersAsset->Metadata.SemanticVersion, FString(TEXT("1.0.0")));
	TestEqual(TEXT("Default seed"), Experiment->RandomSeed, 1337);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAEWorldSubsystemSingletonTest,
	"AdaptiveEnv.M0.WorldSubsystemSingleton",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAEWorldSubsystemSingletonTest::RunTest(const FString& Parameters)
{
	const FName WorldName = MakeUniqueObjectName(GetTransientPackage(), UWorld::StaticClass(), TEXT("AE_M0_TestWorld"));
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false, WorldName, GetTransientPackage(), true);
	TestNotNull(TEXT("Temporary world"), TestWorld);

	if (TestWorld == nullptr)
	{
		return false;
	}

	UAEAdaptiveEnvWorldSubsystem* First = TestWorld->GetSubsystem<UAEAdaptiveEnvWorldSubsystem>();
	UAEAdaptiveEnvWorldSubsystem* Second = TestWorld->GetSubsystem<UAEAdaptiveEnvWorldSubsystem>();

	TestNotNull(TEXT("Adaptive subsystem"), First);
	TestTrue(TEXT("One subsystem per world"), First == Second);
	TestTrue(TEXT("Instance ID is valid"), First != nullptr && First->GetInstanceId().IsValid());

	TestWorld->DestroyWorld(false);
	TestWorld->RemoveFromRoot();
	return true;
}

#endif
