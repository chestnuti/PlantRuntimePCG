#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AEPathHeatmapGrid.h"

namespace AdaptiveEnvM6Tests
{
	/* Creates one valid deterministic M6 parameter set. */
	FAEM6ParameterSet MakeParameters()
	{
		FAEM6ParameterSet Parameters;
		Parameters.VisibleDamageThresholdRatio = 0.2;
		Parameters.FullPathDamageThresholdRatio = 0.6;
		Parameters.FormationRatePerSimulationHour = 0.25;
		Parameters.FadeRatePerSimulationHour = 0.1;
		Parameters.DirtyIntensityEpsilon = 1.0e-6;
		return Parameters;
	}

	/* Creates one single-Cell M6 Grid. */
	bool InitializeSingleCellGrid(FAEPathHeatmapGrid& Grid)
	{
		FAEHeatmapGridConfig Config;
		Config.Dimensions = FIntPoint(1, 1);
		Config.CellSizeCm = 100.0f;
		return Grid.Initialize(Config);
	}

	/* Creates one valid input for the shared origin Cell. */
	FAEM6InputSnapshot MakeInput(
		const double DamageRatio,
		const uint64 ResponseRevision,
		const uint64 CurrentStep)
	{
		FAEM6InputSnapshot Input;
		Input.Coordinate = FIntPoint::ZeroValue;
		Input.DamageRatio = DamageRatio;
		Input.SourceResponseRevision = ResponseRevision;
		Input.SourceResponseSimulationStep = ResponseRevision;
		Input.CurrentSimulationStep = CurrentStep;
		return Input;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAEM6MappingAndFormationTest,
	"AdaptiveEnv.M6.State.MappingAndFormation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verifies threshold mapping and fixed-rate path formation. */
bool FAEM6MappingAndFormationTest::RunTest(const FString& Parameters)
{
	FAEPathHeatmapGrid Grid;
	TestTrue(TEXT("M6 Grid initializes"), AdaptiveEnvM6Tests::InitializeSingleCellGrid(Grid));
	const FAEM6ParameterSet Values = AdaptiveEnvM6Tests::MakeParameters();
	TestTrue(TEXT("M6 formation update succeeds"), Grid.Update(
		{AdaptiveEnvM6Tests::MakeInput(0.6, 1, 1)},
		1.0,
		Values));
	FAEPathHeatmapSnapshot Snapshot;
	TestTrue(TEXT("M6 Cell is queryable"), Grid.GetCellSnapshot(FIntPoint::ZeroValue, Snapshot));
	TestTrue(TEXT("Full threshold maps to one"), FMath::IsNearlyEqual(Snapshot.TargetPathIntensity, 1.0f));
	TestTrue(TEXT("Formation rate advances once"), FMath::IsNearlyEqual(Snapshot.PathIntensity, 0.25f));
	TestEqual(TEXT("First visual revision is one"), Grid.GetPathVisualRevision(), static_cast<uint64>(1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAEM6AsymmetricFadeTest,
	"AdaptiveEnv.M6.State.AsymmetricFade",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verifies M6 continues fading under the same committed M5 response revision. */
bool FAEM6AsymmetricFadeTest::RunTest(const FString& Parameters)
{
	FAEPathHeatmapGrid Grid;
	TestTrue(TEXT("M6 Grid initializes"), AdaptiveEnvM6Tests::InitializeSingleCellGrid(Grid));
	const FAEM6ParameterSet Values = AdaptiveEnvM6Tests::MakeParameters();
	Grid.Update({AdaptiveEnvM6Tests::MakeInput(0.6, 1, 1)}, 4.0, Values);
	Grid.Update({AdaptiveEnvM6Tests::MakeInput(0.0, 2, 2)}, 1.0, Values);
	FAEPathHeatmapSnapshot FirstFade;
	Grid.GetCellSnapshot(FIntPoint::ZeroValue, FirstFade);
	Grid.Update({AdaptiveEnvM6Tests::MakeInput(0.0, 2, 3)}, 1.0, Values);
	FAEPathHeatmapSnapshot SecondFade;
	Grid.GetCellSnapshot(FIntPoint::ZeroValue, SecondFade);
	TestTrue(TEXT("Fade uses configured slower rate"), FMath::IsNearlyEqual(FirstFade.PathIntensity, 0.9f));
	TestTrue(TEXT("Same M5 revision may continue M6 transition"), FMath::IsNearlyEqual(SecondFade.PathIntensity, 0.8f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAEM6RevisionGateTest,
	"AdaptiveEnv.M6.Grid.RevisionGate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verifies stale M5 revisions and repeated M6 steps cannot mutate committed state. */
bool FAEM6RevisionGateTest::RunTest(const FString& Parameters)
{
	FAEPathHeatmapGrid Grid;
	TestTrue(TEXT("M6 Grid initializes"), AdaptiveEnvM6Tests::InitializeSingleCellGrid(Grid));
	const FAEM6ParameterSet Values = AdaptiveEnvM6Tests::MakeParameters();
	Grid.Update({AdaptiveEnvM6Tests::MakeInput(0.6, 2, 2)}, 1.0, Values);
	const uint64 RevisionBefore = Grid.GetPathVisualRevision();
	Grid.Update({AdaptiveEnvM6Tests::MakeInput(0.0, 1, 3)}, 1.0, Values);
	Grid.Update({AdaptiveEnvM6Tests::MakeInput(0.0, 2, 2)}, 1.0, Values);
	TestEqual(TEXT("Two stale inputs are rejected"), Grid.GetRejectedInputCount(), static_cast<uint64>(2));
	TestEqual(TEXT("Rejected inputs do not advance revision"), Grid.GetPathVisualRevision(), RevisionBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAEM6OrderingAndQuantizationTest,
	"AdaptiveEnv.M6.Command.OrderingAndQuantization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verifies unsorted inputs emit stable row-major commands with bounded R8 values. */
bool FAEM6OrderingAndQuantizationTest::RunTest(const FString& Parameters)
{
	FAEHeatmapGridConfig Config;
	Config.Dimensions = FIntPoint(2, 2);
	Config.CellSizeCm = 100.0f;
	FAEPathHeatmapGrid Grid;
	TestTrue(TEXT("M6 Grid initializes"), Grid.Initialize(Config));
	FAEM6InputSnapshot Last = AdaptiveEnvM6Tests::MakeInput(0.6, 1, 1);
	Last.Coordinate = FIntPoint(1, 1);
	FAEM6InputSnapshot First = AdaptiveEnvM6Tests::MakeInput(0.6, 1, 1);
	First.Coordinate = FIntPoint(0, 0);
	Grid.Update({Last, First}, 4.0, AdaptiveEnvM6Tests::MakeParameters());
	const TArray<FAEPathHeatmapVisualCommand>& Commands = Grid.GetVisualCommands();
	TestEqual(TEXT("Two observable commands are emitted"), Commands.Num(), 2);
	if (Commands.Num() == 2)
	{
		TestEqual(TEXT("Commands are row-major"), Commands[0].CellIndex, 0);
		TestEqual(TEXT("Second command retains final index"), Commands[1].CellIndex, 3);
		TestEqual(TEXT("Full intensity quantizes to 255"), Commands[0].EncodedIntensity, static_cast<uint8>(255));
	}
	return true;
}

#endif
