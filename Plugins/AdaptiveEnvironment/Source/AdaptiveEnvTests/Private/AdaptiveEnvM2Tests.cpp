#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AEParameterBundleService.h"
#include "AEParameterBundleFactory.h"
#include "AEPublishedParameterBundleAsset.h"
#include "AdaptiveEnvWorldSubsystem.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace AdaptiveEnvM2Tests
{
	/* Adds one deterministic test-only parameter without claiming literature provenance. */
	void AddParameter(FAEParameterBlock& Block, const TCHAR* Name, const TCHAR* Unit, const double Value, const uint32 Id)
	{
		FAEPublishedParameter& Parameter = Block.Parameters.AddDefaulted_GetRef();
		Parameter.ParameterId = FGuid(0xAE000002, static_cast<int32>(Id), 0, 1);
		Parameter.Name = FName(Name);
		Parameter.Unit = Unit;
		Parameter.EvidenceBasedValue = Value;
		Parameter.EffectiveValue = Value;
		Parameter.PlausibleMinimum = FMath::Min(0.0, Value);
		Parameter.PlausibleMaximum = FMath::Max(1.0, Value);
		Parameter.ParameterVersion = TEXT("1.0.0");
		Parameter.OriginLayer = EAEParameterOriginLayer::ExperimentOverride;
	}

	/* Creates one valid exact M3 block in canonical parameter-name order. */
	FAEParameterBlock MakeM3Block()
	{
		FAEParameterBlock Block;
		Block.BlockId = FGuid(0xAE000002, 3, 0, 1);
		Block.ModelContract = FAEParameterBundleService::M3ModelContract();
		Block.BlockVersion = TEXT("1.0.0");
		uint32 Id = 1;
		AddParameter(Block, TEXT("DamageActivationExposure"), TEXT("ratio"), 0.5, Id++);
		AddParameter(Block, TEXT("DamageMaximumRatePerSimulationHour"), TEXT("ratio/h"), 0.2, Id++);
		AddParameter(Block, TEXT("DamageSaturationExposure"), TEXT("ratio"), 0.8, Id++);
		AddParameter(Block, TEXT("ExposureCollectEventReferenceCount"), TEXT("count"), 1.0, Id++);
		AddParameter(Block, TEXT("ExposureCollectEventWeight"), TEXT("ratio"), 1.0 / 6.0, Id++);
		AddParameter(Block, TEXT("ExposureCombatEventReferenceCount"), TEXT("count"), 1.0, Id++);
		AddParameter(Block, TEXT("ExposureCombatEventWeight"), TEXT("ratio"), 1.0 / 6.0, Id++);
		AddParameter(Block, TEXT("ExposureDwellReferenceSeconds"), TEXT("s"), 1.0, Id++);
		AddParameter(Block, TEXT("ExposureDwellWeight"), TEXT("ratio"), 1.0 / 6.0, Id++);
		AddParameter(Block, TEXT("ExposureHalfLifeSimulationHours"), TEXT("h"), 1.0, Id++);
		AddParameter(Block, TEXT("ExposureMaximum"), TEXT("ratio"), 1.0, Id++);
		AddParameter(Block, TEXT("ExposurePassReferenceCount"), TEXT("count"), 1.0, Id++);
		AddParameter(Block, TEXT("ExposurePassWeight"), TEXT("ratio"), 1.0 / 6.0, Id++);
		AddParameter(Block, TEXT("ExposureSprintDistanceReferenceMeters"), TEXT("m"), 1.0, Id++);
		AddParameter(Block, TEXT("ExposureSprintWeight"), TEXT("ratio"), 1.0 / 6.0, Id++);
		AddParameter(Block, TEXT("ExposureTravelDistanceReferenceMeters"), TEXT("m"), 1.0, Id++);
		AddParameter(Block, TEXT("ExposureTravelDistanceWeight"), TEXT("ratio"), 1.0 / 6.0, Id++);
		AddParameter(Block, TEXT("RecoveryActivationExposure"), TEXT("ratio"), 0.25, Id++);
		AddParameter(Block, TEXT("RecoveryBaseRatePerSimulationHour"), TEXT("ratio/h"), 0.1, Id++);
		AddParameter(Block, TEXT("RecoveryDelaySimulationHours"), TEXT("h"), 1.0, Id++);
		FString Error;
		Block.BlockHash = FAEParameterBundleService::ComputeBlockHash(Block, Error);
		return Block;
	}

	/* Creates one valid exact M4 block in canonical parameter-name order. */
	FAEParameterBlock MakeM4Block()
	{
		FAEParameterBlock Block;
		Block.BlockId = FGuid(0xAE000002, 4, 0, 1);
		Block.ModelContract = FAEParameterBundleService::M4ModelContract();
		Block.BlockVersion = TEXT("1.0.0");
		uint32 Id = 101;
		AddParameter(Block, TEXT("ActiveThreshold"), TEXT("ratio"), 0.25, Id++);
		AddParameter(Block, TEXT("ConstraintStressSensitivity"), TEXT("ratio"), 0.5, Id++);
		AddParameter(Block, TEXT("HysteresisWidth"), TEXT("ratio"), 0.1, Id++);
		AddParameter(Block, TEXT("MoistureOptimalMaximumRatio"), TEXT("ratio"), 0.7, Id++);
		AddParameter(Block, TEXT("MoistureOptimalMinimumRatio"), TEXT("ratio"), 0.3, Id++);
		AddParameter(Block, TEXT("MoistureToleranceWidthRatio"), TEXT("ratio"), 0.2, Id++);
		AddParameter(Block, TEXT("OverusedThreshold"), TEXT("ratio"), 0.75, Id++);
		AddParameter(Block, TEXT("SlopeFullySuitableDegrees"), TEXT("degree"), 10.0, Id++);
		AddParameter(Block, TEXT("SlopeUnsuitableDegrees"), TEXT("degree"), 45.0, Id++);
		AddParameter(Block, TEXT("TransitionDebounceSimulationHours"), TEXT("h"), 0.5, Id++);
		FString Error;
		Block.BlockHash = FAEParameterBundleService::ComputeBlockHash(Block, Error);
		return Block;
	}

	/* Creates and seals one complete test-only Published Parameter Bundle v1. */
	UAEPublishedParameterBundleAsset* MakeBundle()
	{
		UAEPublishedParameterBundleAsset* Bundle = NewObject<UAEPublishedParameterBundleAsset>();
		Bundle->BundleId = FGuid(0xAE000002, 0, 0, 1);
		Bundle->SemanticVersion = TEXT("1.0.0");
		Bundle->SourceAuditHash = FString::ChrN(64, TEXT('a'));
		Bundle->GeneratorVersion = TEXT("adaptive-env-m2-tests/1.0.0");
		Bundle->Blocks = { MakeM3Block(), MakeM4Block() };
		FString Error;
		Bundle->ContentHash = FAEParameterBundleService::ComputeContentHash(*Bundle, Error);
		return Bundle;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAEBundleValidTest, "AdaptiveEnv.M2.Bundle.Valid", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verifies one exact two-block, 30-record bundle passes every runtime gate. */
bool FAEBundleValidTest::RunTest(const FString& Parameters)
{
	const UAEPublishedParameterBundleAsset* Bundle = AdaptiveEnvM2Tests::MakeBundle();
	const FAEParameterBundleValidationResult Result = FAEParameterBundleService::ValidateBundle(*Bundle);
	TestTrue(TEXT("Complete bundle validates"), Result.IsValid());
	TestEqual(TEXT("Exactly two blocks"), Bundle->Blocks.Num(), 2);
	TestEqual(TEXT("Exactly thirty parameters"), Bundle->Blocks[0].Parameters.Num() + Bundle->Blocks[1].Parameters.Num(), 30);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAEBundleHeaderExactTest, "AdaptiveEnv.M2.Bundle.HeaderExact", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verifies fixed header values and canonical header field emission. */
bool FAEBundleHeaderExactTest::RunTest(const FString& Parameters)
{
	UAEPublishedParameterBundleAsset* Bundle = AdaptiveEnvM2Tests::MakeBundle();
	FString Json;
	FString Error;
	TestTrue(TEXT("Canonical JSON builds"), FAEParameterBundleService::BuildCanonicalBundleJson(*Bundle, true, Json, Error));
	const TCHAR* OrderedFields[] = { TEXT("\"Format\""), TEXT("\"SchemaVersion\""), TEXT("\"BundleId\""), TEXT("\"SemanticVersion\""), TEXT("\"ParentContentHash\""), TEXT("\"SourceAuditHash\""), TEXT("\"GeneratorVersion\""), TEXT("\"Blocks\""), TEXT("\"ContentHash\"") };
	int32 Previous = -1;
	for (const TCHAR* Field : OrderedFields)
	{
		const int32 Index = Json.Find(Field);
		TestTrue(FString::Printf(TEXT("Header field %s is ordered"), Field), Index > Previous);
		Previous = Index;
	}
	Bundle->Format = TEXT("Wrong.Format");
	TestFalse(TEXT("Wrong format fails closed"), FAEParameterBundleService::ValidateBundle(*Bundle).IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAEBundleInvalidHashTest, "AdaptiveEnv.M2.Bundle.InvalidSchemaAndHash", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verifies unsupported schemas and nested content tampering fail closed. */
bool FAEBundleInvalidHashTest::RunTest(const FString& Parameters)
{
	UAEPublishedParameterBundleAsset* Bundle = AdaptiveEnvM2Tests::MakeBundle();
	Bundle->SchemaVersion = 2;
	TestFalse(TEXT("Unsupported schema fails"), FAEParameterBundleService::ValidateBundle(*Bundle).IsValid());
	Bundle = AdaptiveEnvM2Tests::MakeBundle();
	Bundle->Blocks[0].Parameters[0].EffectiveValue += 0.01;
	TestFalse(TEXT("Tampered block fails"), FAEParameterBundleService::ValidateBundle(*Bundle).IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAEBundleDuplicateUnitTest, "AdaptiveEnv.M2.Bundle.DuplicateAndUnits", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verifies duplicate identities and exact-unit drift are rejected. */
bool FAEBundleDuplicateUnitTest::RunTest(const FString& Parameters)
{
	UAEPublishedParameterBundleAsset* Bundle = AdaptiveEnvM2Tests::MakeBundle();
	Bundle->Blocks[0].Parameters[1].ParameterId = Bundle->Blocks[0].Parameters[0].ParameterId;
	TestFalse(TEXT("Duplicate ParameterId fails"), FAEParameterBundleService::ValidateBundle(*Bundle).IsValid());
	Bundle = AdaptiveEnvM2Tests::MakeBundle();
	Bundle->Blocks[1].Parameters[7].Unit = TEXT("radian");
	TestFalse(TEXT("Changed unit fails"), FAEParameterBundleService::ValidateBundle(*Bundle).IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAEBundleGoldenImportTest, "AdaptiveEnv.M2.Import.GoldenBundle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verifies canonical JSON imports into a field-identical bundle asset. */
bool FAEBundleGoldenImportTest::RunTest(const FString& Parameters)
{
	UAEPublishedParameterBundleAsset* Source = AdaptiveEnvM2Tests::MakeBundle();
	FString Json;
	FString Error;
	TestTrue(TEXT("Canonical source JSON builds"), FAEParameterBundleService::BuildCanonicalBundleJson(*Source, true, Json, Error));
	const FString Filename = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("GoldenBundle.aeparams.json"));
	TestTrue(TEXT("Golden JSON writes"), FFileHelper::SaveStringToFile(Json, *Filename, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));

	UAEParameterBundleFactory* Factory = NewObject<UAEParameterBundleFactory>();
	bool bCanceled = false;
	UAEPublishedParameterBundleAsset* Imported = Cast<UAEPublishedParameterBundleAsset>(Factory->FactoryCreateFile(
		UAEPublishedParameterBundleAsset::StaticClass(), GetTransientPackage(), TEXT("AE_GoldenBundle"), RF_Transient, Filename, nullptr, GWarn, bCanceled));
	TestNotNull(TEXT("Golden bundle imports"), Imported);
	if (Imported != nullptr)
	{
		TestEqual(TEXT("Imported content hash"), Imported->ContentHash, Source->ContentHash);
		TestEqual(TEXT("Imported block count"), Imported->Blocks.Num(), 2);
		TestEqual(TEXT("Imported record count"), Imported->Blocks[0].Parameters.Num() + Imported->Blocks[1].Parameters.Num(), 30);
	}
	IFileManager::Get().Delete(*Filename, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAEBundleAtomicReimportTest, "AdaptiveEnv.M2.Import.FailedReimportAtomic", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verifies failed reimport leaves every previously committed bundle field unchanged. */
bool FAEBundleAtomicReimportTest::RunTest(const FString& Parameters)
{
	UAEPublishedParameterBundleAsset* Source = AdaptiveEnvM2Tests::MakeBundle();
	FString Json;
	FString Error;
	FAEParameterBundleService::BuildCanonicalBundleJson(*Source, true, Json, Error);
	const FString Filename = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("AtomicBundle.aeparams.json"));
	FFileHelper::SaveStringToFile(Json, *Filename, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	UAEParameterBundleFactory* Factory = NewObject<UAEParameterBundleFactory>();
	bool bCanceled = false;
	UAEPublishedParameterBundleAsset* Imported = Cast<UAEPublishedParameterBundleAsset>(Factory->FactoryCreateFile(
		UAEPublishedParameterBundleAsset::StaticClass(), GetTransientPackage(), TEXT("AE_AtomicBundle"), RF_Transient, Filename, nullptr, GWarn, bCanceled));
	TestNotNull(TEXT("Initial bundle imports"), Imported);
	if (Imported != nullptr)
	{
		const FString OriginalHash = Imported->ContentHash;
		FFileHelper::SaveStringToFile(TEXT("{\"Format\":\"tampered\"}"), *Filename, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		TestEqual(TEXT("Invalid reimport fails"), Factory->Reimport(Imported), EReimportResult::Failed);
		TestEqual(TEXT("Failed reimport preserves content hash"), Imported->ContentHash, OriginalHash);
	}
	IFileManager::Get().Delete(*Filename, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAEBundleWorldSwitchTest, "AdaptiveEnv.M3.Integration.BundleSwitch", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verifies one World commits M3/M4 together and preserves them after a rejected candidate. */
bool FAEBundleWorldSwitchTest::RunTest(const FString& Parameters)
{
	const FName WorldName = MakeUniqueObjectName(GetTransientPackage(), UWorld::StaticClass(), TEXT("AE_BundleSwitchWorld"));
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, WorldName, GetTransientPackage(), true);
	TestNotNull(TEXT("Temporary World"), World);
	if (World == nullptr) return false;
	UAEAdaptiveEnvWorldSubsystem* Subsystem = World->GetSubsystem<UAEAdaptiveEnvWorldSubsystem>();
	TestNotNull(TEXT("Adaptive subsystem"), Subsystem);
	if (Subsystem != nullptr)
	{
		FString Error;
		UAEPublishedParameterBundleAsset* Valid = AdaptiveEnvM2Tests::MakeBundle();
		TestTrue(TEXT("Valid bundle commits"), Subsystem->ApplyParameterBundle(Valid, Error));
		TestTrue(TEXT("M3 commits with bundle"), Subsystem->IsM3Enabled());
		TestTrue(TEXT("M4 commits with bundle"), Subsystem->IsM4Enabled());
		UAEPublishedParameterBundleAsset* Invalid = AdaptiveEnvM2Tests::MakeBundle();
		Invalid->Blocks[0].Parameters[0].EffectiveValue += 0.1;
		TestFalse(TEXT("Tampered candidate is rejected"), Subsystem->ApplyParameterBundle(Invalid, Error));
		TestTrue(TEXT("Rejected candidate preserves M3"), Subsystem->IsM3Enabled());
		TestTrue(TEXT("Rejected candidate preserves M4"), Subsystem->IsM4Enabled());
	}
	World->DestroyWorld(false);
	return true;
}

#endif
