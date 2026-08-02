#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AELSystemGenerator.h"
#include "AELSystemPlantComponent.h"
#include "AELSystemRuleAsset.h"
#include "AEM8Types.h"

namespace AdaptiveEnvM8Tests
{
	/* Creates one compact deterministic rule package for M8 unit tests. */
	UAELSystemRuleAsset* MakeRuleAsset()
	{
		UAELSystemRuleAsset* Asset = NewObject<UAELSystemRuleAsset>();
		Asset->SpeciesId = TEXT("TestPlant");
		Asset->Axiom = TEXT("F[+FL][-FL]F");
		Asset->IterationCount = 0;
		Asset->SegmentLengthCm = 25.0f;
		Asset->RootRadiusCm = 2.0f;
		Asset->RadialSegments = 6;
		Asset->MaxBranchSegments = 32;
		Asset->MaxLeafEmitters = 8;
		Asset->MaxBreakableBranchModules = 4;
		return Asset;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAEM8FixedSeedDeterminismTest,
	"AdaptiveEnv.M8.Grammar.FixedSeedDeterminism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verify equal Rule Assets and seeds reproduce topology, module meshes, and content hash. */
bool FAEM8FixedSeedDeterminismTest::RunTest(const FString& Parameters)
{
	UAELSystemRuleAsset* Asset = AdaptiveEnvM8Tests::MakeRuleAsset();
	FAELSystemGeneratedPlant First;
	FAELSystemGeneratedPlant Second;
	FString Error;
	TestTrue(TEXT("First plant generates"), FAELSystemGenerator::Generate(*Asset, 1337, 99, First, Error));
	TestTrue(TEXT("Second plant generates"), FAELSystemGenerator::Generate(*Asset, 1337, 99, Second, Error));
	TestEqual(TEXT("Content hash repeats"), First.ContentHash, Second.ContentHash);
	TestEqual(TEXT("Segment count repeats"), First.BranchSegments.Num(), Second.BranchSegments.Num());
	TestEqual(TEXT("Emitter count repeats"), First.LeafEmitters.Num(), Second.LeafEmitters.Num());
	TestEqual(TEXT("Module count repeats"), First.ModuleMeshes.Num(), Second.ModuleMeshes.Num());
	for (int32 Index = 0; Index < First.BranchSegments.Num() && Index < Second.BranchSegments.Num(); ++Index)
	{
		TestEqual(TEXT("Branch identity repeats"), First.BranchSegments[Index].BranchId, Second.BranchSegments[Index].BranchId);
		TestTrue(TEXT("Branch endpoint repeats"), First.BranchSegments[Index].EndLocationCm.Equals(
			Second.BranchSegments[Index].EndLocationCm, 0.0f));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAEM8ManualGenerationHasNoUpstreamInputTest,
	"AdaptiveEnv.M8.Generation.ManualRequiresNoUpstream",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verify the pure generation contract requires no M7 snapshot, World, or component. */
bool FAEM8ManualGenerationHasNoUpstreamInputTest::RunTest(const FString& Parameters)
{
	UAELSystemRuleAsset* Asset = AdaptiveEnvM8Tests::MakeRuleAsset();
	FAELSystemGeneratedPlant Plant;
	FString Error;
	TestTrue(TEXT("Standalone generation succeeds"), FAELSystemGenerator::Generate(*Asset, 7, 1, Plant, Error));
	TestTrue(TEXT("Standalone generation creates branches"), Plant.BranchSegments.Num() > 0);
	TestTrue(TEXT("Standalone generation creates fixed mesh modules"), Plant.ModuleMeshes.Num() > 0);
	TestTrue(TEXT("Standalone generation creates triangles"), Plant.ModuleMeshes[0].Triangles.Num() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAEM8SnapshotBindingAvoidsManualIdentityTest,
	"AdaptiveEnv.M8.Integration.SnapshotBindingAvoidsManualIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verify Blueprint can bind a selected M7 snapshot without extracting its stable identity. */
bool FAEM8SnapshotBindingAvoidsManualIdentityTest::RunTest(const FString& Parameters)
{
	UAELSystemPlantComponent* Component = NewObject<UAELSystemPlantComponent>();
	FAEPlantInstanceSnapshot Snapshot;
	Snapshot.StablePointId = 42;
	Snapshot.HealthRatio = 0.45f;
	Snapshot.LifecycleProgressRatio = 0.30f;
	Snapshot.LifecycleState = EAEPlantLifecycleState::Declining;
	Snapshot.bVisible = true;

	FString Error;
	TestTrue(TEXT("Valid M7 snapshot binds"), Component->BindToM7PlantSnapshot(Snapshot, false, Error));
	TestTrue(TEXT("Successful binding clears error"), Error.IsEmpty());
	TestEqual(TEXT("Input mode becomes M7-driven"), Component->InputMode, EAELSystemInputMode::M7Driven);
	TestEqual(TEXT("Stable identity is copied internally"), Component->SourceStablePointId, int64(42));

	Snapshot.StablePointId = 0;
	TestFalse(TEXT("Snapshot without an internal identity is rejected"), Component->BindToM7PlantSnapshot(Snapshot, false, Error));
	TestTrue(TEXT("Rejected snapshot reports a diagnostic"), !Error.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAEM8RoundRobinCoverageTest,
	"AdaptiveEnv.M8.Scheduling.RoundRobinCoverage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verify a bounded M8 budget eventually advances every registered plant. */
bool FAEM8RoundRobinCoverageTest::RunTest(const FString& Parameters)
{
	int32 Cursor = 0;
	TSet<int32> VisitedIndices;
	TArray<int32> Window;
	for (int32 StepIndex = 0; StepIndex < 3; ++StepIndex)
	{
		FAEM8RoundRobinScheduler::BuildWindow(70, 32, Cursor, Window);
		TestEqual(TEXT("Every fixed step respects the M8 budget"), Window.Num(), 32);
		for (const int32 Index : Window)
		{
			VisitedIndices.Add(Index);
		}
	}

	TestEqual(TEXT("Three windows cover all registered plants"), VisitedIndices.Num(), 70);
	TestEqual(TEXT("Cursor continues after the wrapped third window"), Cursor, 26);

	FAEM8RoundRobinScheduler::BuildWindow(0, 32, Cursor, Window);
	TestTrue(TEXT("An empty registration set emits no work"), Window.IsEmpty());
	TestEqual(TEXT("An empty registration set resets the cursor"), Cursor, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAEM8NeighborhoodNearestVisibleSelectionTest,
	"AdaptiveEnv.M8.Activation.NearestVisibleSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verify neighborhood activation is visible-only, nearest-first, bounded, and duplicate-safe. */
bool FAEM8NeighborhoodNearestVisibleSelectionTest::RunTest(const FString& Parameters)
{
	TArray<FAEPlantInstanceSnapshot> Snapshots;
	auto AddSnapshot = [&Snapshots](const int64 StablePointId, const float X, const bool bVisible)
	{
		FAEPlantInstanceSnapshot& Snapshot = Snapshots.AddDefaulted_GetRef();
		Snapshot.StablePointId = StablePointId;
		Snapshot.WorldLocation = FVector(X, 0.0f, 0.0f);
		Snapshot.bVisible = bVisible;
	};
	AddSnapshot(30, 300.0f, true);
	AddSnapshot(20, 100.0f, true);
	AddSnapshot(10, 100.0f, true);
	AddSnapshot(40, 50.0f, false);
	AddSnapshot(50, 600.0f, true);

	TSet<int64> ExcludedIds = {20};
	TArray<FAEPlantInstanceSnapshot> Selected;
	FAEM8NeighborhoodSelector::SelectNearest(
		Snapshots,
		FVector::ZeroVector,
		400.0f,
		2,
		ExcludedIds,
		Selected);
	TestEqual(TEXT("Selection respects the activation budget"), Selected.Num(), 2);
	TestEqual(TEXT("Equal-distance candidates use stable identity order"), Selected[0].StablePointId, int64(10));
	TestEqual(TEXT("The next valid in-radius candidate follows"), Selected[1].StablePointId, int64(30));

	FAEM8NeighborhoodSelector::SelectNearest(
		Snapshots,
		FVector::ZeroVector,
		-1.0f,
		2,
		ExcludedIds,
		Selected);
	TestTrue(TEXT("Invalid negative radius emits no activation"), Selected.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAEM8PresegmentedBreakpointsTest,
	"AdaptiveEnv.M8.Structure.PresegmentedBreakpoints",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verify bracket roots create bounded stable module identities before runtime break events. */
bool FAEM8PresegmentedBreakpointsTest::RunTest(const FString& Parameters)
{
	UAELSystemRuleAsset* Asset = AdaptiveEnvM8Tests::MakeRuleAsset();
	FAELSystemGeneratedPlant Plant;
	FString Error;
	TestTrue(TEXT("Breakable plant generates"), FAELSystemGenerator::Generate(*Asset, 1337, 123, Plant, Error));
	TSet<int64> ModuleIds;
	for (const FAEBranchSegment& Segment : Plant.BranchSegments)
	{
		ModuleIds.Add(Segment.BranchModuleId);
	}
	TestTrue(TEXT("Trunk module exists"), ModuleIds.Contains(0));
	TestTrue(TEXT("At least one detachable branch module exists"), ModuleIds.Num() > 1);
	TestTrue(TEXT("Module cap is respected"), ModuleIds.Num() <= Asset->MaxBreakableBranchModules + 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAEM8BranchRotationsLeaveTrunkAxisTest,
	"AdaptiveEnv.M8.Grammar.BranchRotationsLeaveTrunkAxis",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verify yaw and pitch branch commands change heading instead of creating hidden trunk-overlap modules. */
bool FAEM8BranchRotationsLeaveTrunkAxisTest::RunTest(const FString& Parameters)
{
	UAELSystemRuleAsset* Asset = AdaptiveEnvM8Tests::MakeRuleAsset();
	Asset->Axiom = TEXT("F[+F][-F]F[&F][^F]F");
	Asset->BranchAngleDegrees = 25.0f;
	FAELSystemGeneratedPlant Plant;
	FString Error;
	TestTrue(TEXT("Four-direction branch plant generates"), FAELSystemGenerator::Generate(*Asset, 1337, 321, Plant, Error));

	TMap<int64, FVector> ModuleDirections;
	for (const FAEBranchSegment& Segment : Plant.BranchSegments)
	{
		if (Segment.BranchModuleId != 0 && !ModuleDirections.Contains(Segment.BranchModuleId))
		{
			ModuleDirections.Add(
				Segment.BranchModuleId,
				(Segment.EndLocationCm - Segment.StartLocationCm).GetSafeNormal());
		}
	}
	TestEqual(TEXT("All four authored branches receive modules"), ModuleDirections.Num(), 4);
	for (const TPair<int64, FVector>& Pair : ModuleDirections)
	{
		TestTrue(
			TEXT("Every branch direction leaves the vertical trunk axis"),
			FMath::Abs(FVector::DotProduct(Pair.Value, FVector::UpVector)) < 0.999f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAEM8UnbalancedStackRejectedTest,
	"AdaptiveEnv.M8.Grammar.UnbalancedStackRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verify malformed branch topology fails without publishing partial valid output. */
bool FAEM8UnbalancedStackRejectedTest::RunTest(const FString& Parameters)
{
	UAELSystemRuleAsset* Asset = AdaptiveEnvM8Tests::MakeRuleAsset();
	Asset->Axiom = TEXT("F[+F");
	FAELSystemGeneratedPlant Plant;
	FString Error;
	TestFalse(TEXT("Unbalanced grammar is rejected"), FAELSystemGenerator::Generate(*Asset, 1, 1, Plant, Error));
	TestEqual(
		TEXT("Failure reports invalid input"),
		Plant.TruncationReason,
		EAELSystemTruncationReason::InvalidInput);
	TestTrue(TEXT("Failure provides diagnostic text"), !Error.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAEM8ComplexityLimitTest,
	"AdaptiveEnv.M8.Grammar.SymbolLimit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verify exponential grammar expansion truncates at the configured symbol budget. */
bool FAEM8ComplexityLimitTest::RunTest(const FString& Parameters)
{
	UAELSystemRuleAsset* Asset = AdaptiveEnvM8Tests::MakeRuleAsset();
	Asset->Axiom = TEXT("F");
	Asset->ProductionRules = {{TEXT("F"), TEXT("FF"), 1.0f}};
	Asset->IterationCount = 8;
	Asset->MaxIterations = 8;
	Asset->MaxSymbolCount = 20;
	Asset->MaxBranchSegments = 64;
	FAELSystemGeneratedPlant Plant;
	FString Error;
	TestTrue(TEXT("Truncated plant remains usable"), FAELSystemGenerator::Generate(*Asset, 1, 1, Plant, Error));
	TestTrue(TEXT("Expanded symbol count stays bounded"), Plant.ExpandedSymbolCount <= Asset->MaxSymbolCount);
	TestEqual(
		TEXT("Truncation reason identifies symbol budget"),
		Plant.TruncationReason,
		EAELSystemTruncationReason::SymbolLimit);
	return true;
}

#endif
