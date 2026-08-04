#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AEAdaptiveEnvironmentProfile.h"
#include "AEActiveEnvironmentConfig.h"
#include "AEM2ConfigService.h"
#include "AdaptiveEnvWorldSubsystem.h"
#include "Engine/World.h"

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
	TestTrue(TEXT("M5 damage setting maps"), FMath::IsNearlyEqual(Config.M5.Damage.MaximumRatePerSimulationHour, 0.20));
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
