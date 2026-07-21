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
		Parameters.RegionState.ActiveThreshold = 0.25;
		Parameters.RegionState.OverusedThreshold = 0.75;
		Parameters.RegionState.HysteresisWidth = 0.1;
		Parameters.RegionState.TransitionDebounceSimulationHours = 0.5;
		return Parameters;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAEM4ParameterContractTest, "AdaptiveEnv.M4.Parameters.ConstraintAndState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verifies valid independent environment parameters and hysteresis relationships. */
bool FAEM4ParameterContractTest::RunTest(const FString& Parameters)
{
	FAEM4ParameterSet Values = AdaptiveEnvM4Tests::MakeParameters();
	TestTrue(TEXT("Valid M4 parameters pass"), FAEM4ParameterService::ValidateParameterSet(Values).IsValid());
	Values.RegionState.HysteresisWidth = 0.6;
	TestFalse(TEXT("Overlapping hysteresis fails"), FAEM4ParameterService::ValidateParameterSet(Values).IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAEM4IndependentPressureTest, "AdaptiveEnv.M4.State.IndependentEnvironmentPressure", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verifies slope and moisture alone determine M4 pressure and suitability. */
bool FAEM4IndependentPressureTest::RunTest(const FString& Parameters)
{
	const FAEM4ParameterSet Values = AdaptiveEnvM4Tests::MakeParameters();
	FAEM4StateMemory State;
	const FAEEnvironmentConstraintSnapshot Suitable = FAEM4ParameterService::EvaluateEnvironment(10.0, 0.5, 0.5, Values, State);
	const FAEEnvironmentConstraintSnapshot Severe = FAEM4ParameterService::EvaluateEnvironment(45.0, 0.0, 0.5, Values, State);
	TestTrue(TEXT("Suitable habitat has no pressure"), FMath::IsNearlyZero(Suitable.ConstraintPressureRatio));
	TestTrue(TEXT("Suitable habitat has full suitability"), FMath::IsNearlyEqual(Suitable.HabitatSuitabilityRatio, 1.0f));
	TestTrue(TEXT("Severe habitat has full pressure"), FMath::IsNearlyEqual(Severe.ConstraintPressureRatio, 1.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAEM4HysteresisDebounceTest, "AdaptiveEnv.M4.State.SharedHysteresisAndDebounce", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verifies one shared debounce controls environment-state transitions. */
bool FAEM4HysteresisDebounceTest::RunTest(const FString& Parameters)
{
	const FAEM4ParameterSet Values = AdaptiveEnvM4Tests::MakeParameters();
	FAEM4StateMemory State;
	FAEM4ParameterService::EvaluateEnvironment(30.0, 0.5, 0.25, Values, State);
	TestEqual(TEXT("State remains Unused before debounce"), State.CurrentState, EAERegionState::Unused);
	FAEM4ParameterService::EvaluateEnvironment(30.0, 0.5, 0.25, Values, State);
	TestEqual(TEXT("State commits after debounce"), State.CurrentState, EAERegionState::Active);
	return true;
}

#endif
