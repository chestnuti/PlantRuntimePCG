#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AEAdaptiveEnvironmentProfile.h"
#include "AEActiveEnvironmentConfig.h"
#include "AEM2ConfigService.h"
#include "AEPlantBiomeMapAsset.h"
#include "AdaptiveEnvWorldSubsystem.h"
#include "AdaptiveEnvSettings.h"
#include "AEWorldScalarFieldAsset.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAEM2DefaultProfileTest, "AdaptiveEnv.M2.Profile.DefaultMapping", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAEM2DefaultProfileTest::RunTest(const FString& Parameters)
{
	const UAEAdaptiveEnvironmentProfile* Profile = NewObject<UAEAdaptiveEnvironmentProfile>();
	FAEActiveEnvironmentConfig Config;
	const FAEM2ValidationResult Result = FAEM2ConfigService::BuildActiveConfig(*Profile, 1, Config);
	TestTrue(TEXT("Default product profile validates"), Result.IsValid());
	TestEqual(TEXT("Profile identity maps"), Config.ProfileId, FName(TEXT("Default")));
	TestEqual(TEXT("Runtime revision maps"), Config.RuntimeRevision, static_cast<uint32>(1));
	TestTrue(TEXT("Named Pass channel maps"), FMath::IsNearlyEqual(Config.M3.Channel(EAEExposureChannel::Pass).Weight, 0.20));
	TestTrue(TEXT("M4 terrain setting maps"), FMath::IsNearlyEqual(Config.M4.ConstraintResponse.SlopeUnsuitableDegrees, 45.0));
	TestTrue(TEXT("M4 default moisture maps"), FMath::IsNearlyEqual(Config.DefaultMoistureRatio, 0.5));
	TestTrue(TEXT("M5 damage setting maps"), FMath::IsNearlyEqual(Config.M5.Damage.MaximumRatePerSimulationHour, 0.20));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAEM2ConfiguredProfileLoadTest, "AdaptiveEnv.M2.Profile.ConfiguredAssetLoads", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAEM2ConfiguredProfileLoadTest::RunTest(const FString& Parameters)
{
	const UAdaptiveEnvSettings* Settings = GetDefault<UAdaptiveEnvSettings>();
	UAEAdaptiveEnvironmentProfile* Profile = Settings->EnvironmentProfile.LoadSynchronous();
	TestNotNull(TEXT("Configured product profile loads"), Profile);
	if (Profile == nullptr) return false;
	FAEActiveEnvironmentConfig Config;
	const FAEM2ValidationResult Result = FAEM2ConfigService::BuildActiveConfig(*Profile, 1, Config);
	TestTrue(TEXT("Configured product profile validates"), Result.IsValid());
	TestEqual(TEXT("Configured profile uses current schema"), Profile->ConfigVersion, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAEM2MoistureFieldValidationTest, "AdaptiveEnv.M2.Profile.MoistureFieldValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAEM2MoistureFieldValidationTest::RunTest(const FString& Parameters)
{
	UAEAdaptiveEnvironmentProfile* Profile = NewObject<UAEAdaptiveEnvironmentProfile>();
	Profile->M4.MoistureTexture = NewObject<UAEMoistureTextureAsset>(Profile);
	FAEActiveEnvironmentConfig Preserved;
	Preserved.ProfileId = TEXT("Preserved");
	Preserved.RuntimeRevision = 4;
	const FAEM2ValidationResult Invalid = FAEM2ConfigService::BuildActiveConfig(*Profile, 5, Preserved);
	TestFalse(TEXT("Unbaked moisture field rejects the complete profile"), Invalid.IsValid());
	TestEqual(TEXT("Rejected field preserves active profile"), Preserved.ProfileId, FName(TEXT("Preserved")));
	Profile->M4.MoistureTexture->BakedDimensions = FIntPoint(1, 1);
	Profile->M4.MoistureTexture->BakedSamples = {32768};
	FAEActiveEnvironmentConfig Valid;
	TestTrue(TEXT("Valid baked moisture field commits"), FAEM2ConfigService::BuildActiveConfig(*Profile, 5, Valid).IsValid());
	TestTrue(TEXT("Committed field identity maps"), Valid.MoistureTexture.Get() == Profile->M4.MoistureTexture);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAEDedicatedTextureInputTypesTest, "AdaptiveEnv.M2.Profile.DedicatedTextureInputTypes", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAEDedicatedTextureInputTypesTest::RunTest(const FString& Parameters)
{
	const FObjectProperty* MoistureProperty = FindFProperty<FObjectProperty>(
		FAEM4UserConfig::StaticStruct(),
		GET_MEMBER_NAME_CHECKED(FAEM4UserConfig, MoistureTexture));
	const FObjectProperty* BiomeProperty = FindFProperty<FObjectProperty>(
		UAEPlantBiomeMapAsset::StaticClass(),
		GET_MEMBER_NAME_CHECKED(UAEPlantBiomeMapAsset, BiomeTexture));
	TestNotNull(TEXT("M4 exposes a dedicated moisture texture input"), MoistureProperty);
	TestNotNull(TEXT("M7 exposes a dedicated biome texture input"), BiomeProperty);
	if (MoistureProperty != nullptr)
	{
		TestEqual(TEXT("M4 accepts only moisture texture assets"), MoistureProperty->PropertyClass.Get(), UAEMoistureTextureAsset::StaticClass());
	}
	if (BiomeProperty != nullptr)
	{
		TestEqual(TEXT("M7 accepts only biome texture assets"), BiomeProperty->PropertyClass.Get(), UAEBiomeTextureAsset::StaticClass());
	}
	TestFalse(TEXT("Moisture and biome texture asset types are mutually exclusive"),
		UAEMoistureTextureAsset::StaticClass()->IsChildOf(UAEBiomeTextureAsset::StaticClass())
		|| UAEBiomeTextureAsset::StaticClass()->IsChildOf(UAEMoistureTextureAsset::StaticClass()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAEM2AtomicValidationTest, "AdaptiveEnv.M2.Profile.AtomicValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAEM2AtomicValidationTest::RunTest(const FString& Parameters)
{
	UAEAdaptiveEnvironmentProfile* Profile = NewObject<UAEAdaptiveEnvironmentProfile>();
	FAEActiveEnvironmentConfig Config;
	Config.ProfileId = TEXT("Preserved");
	Config.RuntimeRevision = 7;
	Profile->M5.DamageSaturationImpact = Profile->M5.DamageActivationImpact;
	const FAEM2ValidationResult Result = FAEM2ConfigService::BuildActiveConfig(*Profile, 8, Config);
	TestFalse(TEXT("Invalid stage rejects complete profile"), Result.IsValid());
	TestEqual(TEXT("Rejected candidate preserves identity"), Config.ProfileId, FName(TEXT("Preserved")));
	TestEqual(TEXT("Rejected candidate preserves revision"), Config.RuntimeRevision, static_cast<uint32>(7));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAEM2WorldProfileSwitchTest, "AdaptiveEnv.M2.Profile.WorldSwitch", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAEM2WorldProfileSwitchTest::RunTest(const FString& Parameters)
{
	const FName WorldName = MakeUniqueObjectName(GetTransientPackage(), UWorld::StaticClass(), TEXT("AE_ProfileSwitchWorld"));
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, WorldName, GetTransientPackage(), true);
	TestNotNull(TEXT("Temporary World"), World);
	if (World == nullptr) return false;
	UAEAdaptiveEnvWorldSubsystem* Subsystem = World->GetSubsystem<UAEAdaptiveEnvWorldSubsystem>();
	TestNotNull(TEXT("Adaptive subsystem"), Subsystem);
	if (Subsystem != nullptr)
	{
		FString Error;
		UAEAdaptiveEnvironmentProfile* Valid = NewObject<UAEAdaptiveEnvironmentProfile>();
		TestTrue(TEXT("Valid profile commits"), Subsystem->ApplyEnvironmentProfile(Valid, Error));
		TestTrue(TEXT("M3 commits with profile"), Subsystem->IsM3Enabled());
		TestTrue(TEXT("M4 commits with profile"), Subsystem->IsM4Enabled());
		TestTrue(TEXT("M5 commits with profile"), Subsystem->IsM5Enabled());
		const int64 CommittedRevision = Subsystem->GetEnvironmentConfigRevision();
		UAEAdaptiveEnvironmentProfile* Invalid = NewObject<UAEAdaptiveEnvironmentProfile>();
		Invalid->M3.Pass.Weight = 0.5;
		TestFalse(TEXT("Invalid profile is rejected"), Subsystem->ApplyEnvironmentProfile(Invalid, Error));
		TestEqual(TEXT("Rejected profile preserves revision"), Subsystem->GetEnvironmentConfigRevision(), CommittedRevision);
	}
	World->DestroyWorld(false);
	return true;
}

#endif
