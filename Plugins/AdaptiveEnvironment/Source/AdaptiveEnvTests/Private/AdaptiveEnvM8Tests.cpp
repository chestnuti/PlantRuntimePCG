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
	FAEM8PersistentDeadWoodDeathFadeTest,
	"AdaptiveEnv.M8.Material.PersistentDeadWoodKeepsDeathFade",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verify M7 recovery cannot reduce the material retirement mask on persistent dead wood. */
bool FAEM8PersistentDeadWoodDeathFadeTest::RunTest(const FString& Parameters)
{
	bool bPersistentFadeLocked = false;
	TestEqual(
		TEXT("Dead wood preserves an incomplete source fade"),
		FAEM8MaterialPolicy::ResolveDeathFadeRatio(
			0.35f,
			EAEBranchStructuralState::DeadWood,
			bPersistentFadeLocked),
		0.35f);
	TestFalse(TEXT("Incomplete dead wood fade is not locked"), bPersistentFadeLocked);
	TestEqual(
		TEXT("Completed dead wood fade reaches full retirement"),
		FAEM8MaterialPolicy::ResolveDeathFadeRatio(
			1.0f,
			EAEBranchStructuralState::DeadWood,
			bPersistentFadeLocked),
		1.0f);
	TestTrue(TEXT("Completed dead wood fade becomes locked"), bPersistentFadeLocked);
	TestEqual(
		TEXT("Locked dead wood remains retired after source recovery"),
		FAEM8MaterialPolicy::ResolveDeathFadeRatio(
			0.0f,
			EAEBranchStructuralState::DeadWood,
			bPersistentFadeLocked),
		1.0f);
	bPersistentFadeLocked = true;
	TestEqual(
		TEXT("Intact modules retain reversible source fade"),
		FAEM8MaterialPolicy::ResolveDeathFadeRatio(
			0.35f,
			EAEBranchStructuralState::Intact,
			bPersistentFadeLocked),
		0.35f);
	TestFalse(TEXT("Non-dead-wood modules clear stale fade locks"), bPersistentFadeLocked);
	TestEqual(
		TEXT("Reversible source fade remains normalized"),
		FAEM8MaterialPolicy::ResolveDeathFadeRatio(
			2.0f,
			EAEBranchStructuralState::Stressed,
			bPersistentFadeLocked),
		1.0f);
	TestFalse(
		TEXT("Disabled cleanup preserves the owner"),
		FAEM8MaterialPolicy::ShouldDestroyOwnerAfterPersistentFade(false, true, true));
	TestFalse(
		TEXT("Incomplete persistent fade preserves the owner"),
		FAEM8MaterialPolicy::ShouldDestroyOwnerAfterPersistentFade(true, true, false));
	TestFalse(
		TEXT("No persistent dead wood preserves the owner"),
		FAEM8MaterialPolicy::ShouldDestroyOwnerAfterPersistentFade(true, false, true));
	TestTrue(
		TEXT("Configured completed persistent fade destroys the owner"),
		FAEM8MaterialPolicy::ShouldDestroyOwnerAfterPersistentFade(true, true, true));
	return true;
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
	FAEM8LeafInstanceDeterminismTest,
	"AdaptiveEnv.M8.Leaves.HISMInstanceDeterminism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verify equal leaf regions create stable bounded module-owned HISM transforms. */
bool FAEM8LeafInstanceDeterminismTest::RunTest(const FString& Parameters)
{
	FAELeafEmitterDescriptor Emitter;
	Emitter.OwnerBranchModuleId = 3;
	Emitter.LocalTransform = FTransform(FRotator(0.0f, 25.0f, 0.0f), FVector(10.0f, 20.0f, 100.0f));
	Emitter.EmitterLengthCm = 100.0f;
	Emitter.EmitterRadiusCm = 4.0f;
	Emitter.DensityPerMeter = 6.0f;
	Emitter.Seed = 47;

	TArray<FAEM8LeafInstanceDescriptor> First;
	TArray<FAEM8LeafInstanceDescriptor> Second;
	FAEM8LeafInstanceBuilder::Build({Emitter}, 1.0f, 1.0f, 0.2f, 4, First);
	FAEM8LeafInstanceBuilder::Build({Emitter}, 1.0f, 1.0f, 0.2f, 4, Second);
	TestEqual(TEXT("Concrete leaf count respects the hard cap"), First.Num(), 4);
	TestEqual(TEXT("Equal inputs repeat the concrete leaf count"), Second.Num(), First.Num());
	for (int32 Index = 0; Index < First.Num(); ++Index)
	{
		TestEqual(TEXT("Every leaf retains its owning branch module"), First[Index].OwnerBranchModuleId, int64(3));
		TestTrue(TEXT("Equal inputs repeat leaf transforms"), First[Index].PlantLocalTransform.Equals(
			Second[Index].PlantLocalTransform, 0.0f));
		TestEqual(TEXT("Equal inputs repeat visibility thresholds"),
			First[Index].VisibilityThreshold, Second[Index].VisibilityThreshold);
	}
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
	Snapshot.DeathFadeRatio = 0.35f;

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAEM8DetachedBranchExpiryBoundaryTest,
	"AdaptiveEnv.M8.Pooling.DetachedBranchExpiryBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verify debris remains live before its configured inclusive expiry boundary. */
bool FAEM8DetachedBranchExpiryBoundaryTest::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("Debris remains live before expiry"), FAEM8PoolPolicy::IsDetachedBranchExpired(14.999, 15.0));
	TestTrue(TEXT("Debris expires at its boundary"), FAEM8PoolPolicy::IsDetachedBranchExpired(15.0, 15.0));
	TestTrue(TEXT("Debris remains expired after its boundary"), FAEM8PoolPolicy::IsDetachedBranchExpired(16.0, 15.0));
	TestFalse(TEXT("Non-finite timestamps cannot expire debris"), FAEM8PoolPolicy::IsDetachedBranchExpired(NAN, 15.0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAEM8LiveDebrisBlocksPoolReturnTest,
	"AdaptiveEnv.M8.Pooling.LiveDebrisBlocksReturn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verify a pooled actor cannot become Available while any detached branch is alive. */
bool FAEM8LiveDebrisBlocksPoolReturnTest::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("One live branch blocks return"), FAEM8PoolPolicy::CanReturnToAvailable(1));
	TestFalse(TEXT("Multiple live branches block return"), FAEM8PoolPolicy::CanReturnToAvailable(8));
	TestTrue(TEXT("Zero live branches permits return"), FAEM8PoolPolicy::CanReturnToAvailable(0));
	TestFalse(TEXT("Invalid negative counts are rejected"), FAEM8PoolPolicy::CanReturnToAvailable(-1));
	return true;
}

#endif
