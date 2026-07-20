#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AEM4ParameterService.h"

namespace AdaptiveEnvM4Tests
{
	/* Creates one valid test-only M4 parameter set. */
	FAEM4ParameterSet MakeParameters()
	{
		FAEM4ParameterSet Parameters;
		Parameters.ConstraintResponse.SlopeFullySuitableDegrees = 10.0;
		Parameters.ConstraintResponse.SlopeUnsuitableDegrees = 45.0;
		Parameters.ConstraintResponse.MoistureOptimalMinimumRatio = 0.3;
		Parameters.ConstraintResponse.MoistureOptimalMaximumRatio = 0.7;
		Parameters.ConstraintResponse.MoistureToleranceWidthRatio = 0.2;
		Parameters.ConstraintResponse.ConstraintStressSensitivity = 0.5;
		Parameters.RegionState.ActiveThreshold = 0.25;
		Parameters.RegionState.OverusedThreshold = 0.75;
		Parameters.RegionState.HysteresisWidth = 0.1;
		Parameters.RegionState.TransitionDebounceSimulationHours = 0.5;
		return Parameters;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAEM4ParameterContractTest, "AdaptiveEnv.M4.Parameters.ConstraintAndState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verifies valid constraints and shared hysteresis relationships. */
bool FAEM4ParameterContractTest::RunTest(const FString& Parameters)
{
	FAEM4ParameterSet Values = AdaptiveEnvM4Tests::MakeParameters();
	TestTrue(TEXT("Valid M4 parameters pass"), FAEM4ParameterService::ValidateParameterSet(Values).IsValid());
	Values.RegionState.HysteresisWidth = 0.6;
	TestFalse(TEXT("Overlapping hysteresis fails"), FAEM4ParameterService::ValidateParameterSet(Values).IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAEM4EffectivePressureTest, "AdaptiveEnv.M4.State.EffectivePressure", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verifies either disturbance or Damage can dominate effective pressure. */
bool FAEM4EffectivePressureTest::RunTest(const FString& Parameters)
{
	const FAEM4ParameterSet Values = AdaptiveEnvM4Tests::MakeParameters();
	FAEM4StateMemory State;
	FAEM3CellSnapshot M3;
	M3.CurrentExposure = 0.2f;
	M3.EcologicalDamageRatio = 0.9f;
	const FAEM4DecisionSnapshot DamageDriven = FAEM4ParameterService::EvaluateDecision(10.0, 0.5, M3, 1.0, 0.5, Values, State);
	TestTrue(TEXT("Damage dominates pressure"), FMath::IsNearlyEqual(DamageDriven.EffectivePressureRatio, DamageDriven.EffectiveDamageRatio));
	M3.CurrentExposure = 1.0f;
	M3.EcologicalDamageRatio = 0.1f;
	const FAEM4DecisionSnapshot ExposureDriven = FAEM4ParameterService::EvaluateDecision(10.0, 0.5, M3, 1.0, 0.5, Values, State);
	TestTrue(TEXT("Exposure dominates pressure"), FMath::IsNearlyEqual(ExposureDriven.EffectivePressureRatio, ExposureDriven.EffectiveDisturbanceRatio));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAEM4HysteresisDebounceTest, "AdaptiveEnv.M4.State.SharedHysteresisAndDebounce", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verifies threshold noise is stable and one shared debounce controls transitions. */
bool FAEM4HysteresisDebounceTest::RunTest(const FString& Parameters)
{
	const FAEM4ParameterSet Values = AdaptiveEnvM4Tests::MakeParameters();
	FAEM4StateMemory State;
	FAEM3CellSnapshot M3;
	M3.CurrentExposure = 0.4f;
	FAEM4ParameterService::EvaluateDecision(10.0, 0.5, M3, 1.0, 0.25, Values, State);
	TestEqual(TEXT("State remains Unused before debounce"), State.CurrentState, EAERegionState::Unused);
	FAEM4ParameterService::EvaluateDecision(10.0, 0.5, M3, 1.0, 0.25, Values, State);
	TestEqual(TEXT("State commits Active at debounce"), State.CurrentState, EAERegionState::Active);
	M3.CurrentExposure = 0.26f;
	FAEM4ParameterService::EvaluateDecision(10.0, 0.5, M3, 1.0, 1.0, Values, State);
	TestEqual(TEXT("Pressure inside hysteresis band remains Active"), State.CurrentState, EAERegionState::Active);
	return true;
}

#endif
