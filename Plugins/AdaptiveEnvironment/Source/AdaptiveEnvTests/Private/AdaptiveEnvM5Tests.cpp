#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AEM5ParameterService.h"

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

#endif
