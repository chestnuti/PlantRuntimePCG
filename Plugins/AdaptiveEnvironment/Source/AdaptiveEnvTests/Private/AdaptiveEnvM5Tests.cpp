#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AEM5ParameterService.h"
#include "AEEcologicalResponseGrid.h"

namespace AdaptiveEnvM5Tests
{
	/* Creates one valid test-only M5 parameter set. */
	FAEM5ParameterSet MakeParameters()
	{
		FAEM5ParameterSet Parameters;
		Parameters.Fusion.ConstraintSensitivity = 0.5;
		Parameters.Damage.ActivationImpact = 0.25;
		Parameters.Damage.SaturationImpact = 0.75;
		Parameters.Damage.MaximumRatePerSimulationHour = 0.2;
		Parameters.Recovery.ActivationExposure = 0.2;
		Parameters.Recovery.DelaySimulationHours = 1.0;
		Parameters.Recovery.BaseRatePerSimulationHour = 0.2;
		return Parameters;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAEM5DamageTest, "AdaptiveEnv.M5.Response.DamageOnly", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verifies M5 integrates Damage from independent M3 and M4 scalar inputs. */
bool FAEM5DamageTest::RunTest(const FString& Parameters)
{
	FAEM5StateMemory State;
	const FAEEcologicalResponseSnapshot Result = FAEM5ParameterService::EvaluateResponse(0.5, 1.0, 0.0, 1.0, 1.0, AdaptiveEnvM5Tests::MakeParameters(), State);
	TestTrue(TEXT("Midpoint Damage rate is linear"), FMath::IsNearlyEqual(Result.DamageRatePerSimulationHour, 0.1f, 1.0e-6f));
	TestTrue(TEXT("Damage integrates once"), FMath::IsNearlyEqual(Result.DamageRatio, 0.1f, 1.0e-6f));
	TestTrue(TEXT("Recovery is mutually exclusive"), FMath::IsNearlyZero(Result.RecoveryRatePerSimulationHour));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAEM5EnvironmentDifferentiationTest, "AdaptiveEnv.M5.Response.EnvironmentDifferentiation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verifies equal Exposure produces a larger impact under higher environment pressure. */
bool FAEM5EnvironmentDifferentiationTest::RunTest(const FString& Parameters)
{
	FAEM5StateMemory SuitableState;
	FAEM5StateMemory StressedState;
	const FAEM5ParameterSet Values = AdaptiveEnvM5Tests::MakeParameters();
	const FAEEcologicalResponseSnapshot Suitable = FAEM5ParameterService::EvaluateResponse(0.5, 1.0, 0.0, 1.0, 0.0, Values, SuitableState);
	const FAEEcologicalResponseSnapshot Stressed = FAEM5ParameterService::EvaluateResponse(0.5, 1.0, 1.0, 0.0, 0.0, Values, StressedState);
	TestTrue(TEXT("Environment pressure increases effective impact"), Stressed.EffectiveImpactRatio > Suitable.EffectiveImpactRatio);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAEM5RecoveryTest, "AdaptiveEnv.M5.Response.DelayedRecoveryOnly", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verifies Recovery begins only after the continuous low-Exposure delay. */
bool FAEM5RecoveryTest::RunTest(const FString& Parameters)
{
	FAEM5StateMemory State;
	State.DamageRatio = 0.5;
	const FAEM5ParameterSet Values = AdaptiveEnvM5Tests::MakeParameters();
	const FAEEcologicalResponseSnapshot Before = FAEM5ParameterService::EvaluateResponse(0.0, 1.0, 0.0, 1.0, 0.5, Values, State);
	const FAEEcologicalResponseSnapshot After = FAEM5ParameterService::EvaluateResponse(0.0, 1.0, 0.0, 1.0, 0.5, Values, State);
	TestTrue(TEXT("Recovery waits for delay"), FMath::IsNearlyZero(Before.RecoveryRatePerSimulationHour));
	TestTrue(TEXT("Recovery starts at delay"), FMath::IsNearlyEqual(After.RecoveryRatePerSimulationHour, 0.2f));
	TestTrue(TEXT("Recovery reduces Damage"), After.DamageRatio < Before.DamageRatio);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAEM5GridVersionGateTest, "AdaptiveEnv.M5.Grid.VersionGateAndRevision", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verifies M5 commits matching bundle inputs and rejects stale or mixed identities. */
bool FAEM5GridVersionGateTest::RunTest(const FString& Parameters)
{
	FAEHeatmapGridConfig Config;
	Config.Dimensions = FIntPoint(1, 1);
	Config.CellSizeCm = 100.0f;
	FAEEcologicalResponseGrid Grid;
	TestTrue(TEXT("M5 Grid initializes"), Grid.Initialize(Config));
	FAEParameterBundleIdentity Identity{FGuid(0xAE000005, 0, 0, 1), TEXT("1.0.0"), FString::ChrN(64, TEXT('a'))};
	FAEM5InputSnapshot Input;
	Input.Coordinate = FIntPoint::ZeroValue;
	Input.Exposure = 0.5;
	Input.ExposureMaximum = 1.0;
	Input.ExposureRevision = 1;
	Input.ConstraintRevision = 1;
	Input.SimulationStep = 1;
	Input.BundleIdentity = Identity;
	TestTrue(TEXT("Compatible input commits"), Grid.Update({Input}, 1.0, AdaptiveEnvM5Tests::MakeParameters(), Identity));
	FAEEcologicalResponseSnapshot First;
	TestTrue(TEXT("Committed M5 Cell is queryable"), Grid.GetCellSnapshot(FIntPoint::ZeroValue, First));
	TestEqual(TEXT("First response revision is one"), Grid.GetResponseRevision(), static_cast<uint64>(1));
	Input.BundleIdentity.ContentHash = FString::ChrN(64, TEXT('b'));
	Grid.Update({Input}, 1.0, AdaptiveEnvM5Tests::MakeParameters(), Identity);
	TestEqual(TEXT("Mixed bundle input is rejected"), Grid.GetRejectedInputCount(), static_cast<uint64>(1));
	TestEqual(TEXT("Rejected input does not advance revision"), Grid.GetResponseRevision(), static_cast<uint64>(1));
	return true;
}

#endif
