#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AEBehaviourTrackerComponent.h"
#include "AEHeatmapGrid.h"
#include "AdaptiveEnvGameplayTags.h"
#include "AdaptiveEnvWorldSubsystem.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace AdaptiveEnvM1Tests
{
	// Build a compact deterministic grid for unit tests.
	FAEHeatmapGridConfig MakeGridConfig()
	{
		FAEHeatmapGridConfig Config;
		Config.WorldCenter = FVector2D::ZeroVector;
		Config.Dimensions = FIntPoint(4, 4);
		Config.CellSizeCm = 100.0f;
		Config.KernelRadiusCells = 1;
		Config.KernelSigma = 0.75f;
		return Config;
	}

	// Build a minimal ordered behaviour sample for test scenarios.
	FAEBehaviourSample MakeSample(
		const FGuid& AgentId,
		const int64 Sequence,
		const FVector& Location,
		const FGameplayTag& Tag)
	{
		FAEBehaviourSample Sample;
		Sample.AgentId = AgentId;
		Sample.WorldLocation = Location;
		Sample.Timestamp = static_cast<double>(Sequence) * 0.1;
		Sample.SequenceNumber = Sequence;
		Sample.BehaviourTag = Tag;
		return Sample;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAEGridMappingTest,
	"AdaptiveEnv.M1.GridMapping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Verify half-open world bounds and row-major coordinate mapping.
bool FAEGridMappingTest::RunTest(const FString& Parameters)
{
	// Initialize a four-by-four grid centred at the origin.
	FAEHeatmapGrid Grid;
	TestTrue(TEXT("Grid initializes"), Grid.Initialize(AdaptiveEnvM1Tests::MakeGridConfig()));

	// Check included lower bounds, excluded upper bounds, and outside points.
	FIntPoint Coordinate;
	TestTrue(TEXT("Lower boundary is included"), Grid.WorldToCell(FVector(-200.0, -200.0, 0.0), Coordinate));
	TestEqual(TEXT("Lower boundary coordinate"), Coordinate, FIntPoint(0, 0));
	TestTrue(TEXT("Upper interior is included"), Grid.WorldToCell(FVector(199.9, 199.9, 0.0), Coordinate));
	TestEqual(TEXT("Upper interior coordinate"), Coordinate, FIntPoint(3, 3));
	TestFalse(TEXT("Upper X boundary is excluded"), Grid.WorldToCell(FVector(200.0, 0.0, 0.0), Coordinate));
	TestFalse(TEXT("Upper Y boundary is excluded"), Grid.WorldToCell(FVector(0.0, 200.0, 0.0), Coordinate));
	TestFalse(TEXT("Outside point is rejected"), Grid.WorldToCell(FVector(-201.0, 0.0, 0.0), Coordinate));

	// Verify flat indexing and world-space cell centres.
	int32 Index = INDEX_NONE;
	TestTrue(TEXT("Coordinate maps to index"), Grid.CellToIndex(FIntPoint(2, 1), Index));
	TestEqual(TEXT("Flattened index"), Index, 6);
	TestEqual(TEXT("Cell centre"), Grid.GetCellWorldCenter(FIntPoint(0, 0)), FVector(-150.0, -150.0, 0.0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAEBehaviourAggregationTest,
	"AdaptiveEnv.M1.BehaviourAggregation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Verify movement, pass, dwell, flow, and ordering aggregation.
bool FAEBehaviourAggregationTest::RunTest(const FString& Parameters)
{
	// Initialize a deterministic grid and agent identity.
	FAEHeatmapGrid Grid;
	Grid.Initialize(AdaptiveEnvM1Tests::MakeGridConfig());
	const FGuid AgentId = FGuid::NewGuid();

	// Seed initial occupancy without adding movement distance.
	FAEBehaviourSample First = AdaptiveEnvM1Tests::MakeSample(
		AgentId,
		0,
		FVector(-150.0, -50.0, 0.0),
		AdaptiveEnvGameplayTags::Behaviour_Move.GetTag());
	TestEqual(TEXT("First sample accepted"), Grid.AccumulateSample(First), EAEBehaviourSubmitResult::Accepted);

	// Move across four cells and provide the conserved distance total.
	FAEBehaviourSample Move = AdaptiveEnvM1Tests::MakeSample(
		AgentId,
		1,
		FVector(150.0, -50.0, 0.0),
		AdaptiveEnvGameplayTags::Behaviour_Move.GetTag());
	Move.PreviousWorldLocation = First.WorldLocation;
	Move.bHasPreviousLocation = true;
	Move.DeltaSeconds = 0.1f;
	Move.TravelDistanceMeters = 3.0f;
	TestEqual(TEXT("Movement sample accepted"), Grid.AccumulateSample(Move), EAEBehaviourSubmitResult::Accepted);

	// Sum every cell to verify distance, pass, flow, and dirty invariants.
	float TotalDistance = 0.0f;
	float TotalPasses = 0.0f;
	int32 FlowCellCount = 0;
	for (int32 Y = 0; Y < 4; ++Y)
	{
		for (int32 X = 0; X < 4; ++X)
		{
			FAEBehaviourCellSnapshot Cell;
			Grid.GetCellSnapshot(FIntPoint(X, Y), Cell);
			TotalDistance += Cell.TravelDistanceMeters;
			TotalPasses += Cell.PassCount;
			if (Cell.FlowMagnitude > 0.0f)
			{
				++FlowCellCount;
				TestTrue(TEXT("Flow points right"), Cell.FlowDirection.X > 0.99);
			}
		}
	}
	TestTrue(TEXT("Distance is conserved"), FMath::IsNearlyEqual(TotalDistance, 3.0f, 0.001f));
	TestEqual(TEXT("Each entered cell counts once"), TotalPasses, 4.0f);
	TestTrue(TEXT("Movement creates flow"), FlowCellCount > 0);
	TestTrue(TEXT("Dirty cells are unique"), Grid.GetDirtyCellIndices().Num() <= 16);

	// Add dwell time at the final occupied position.
	FAEBehaviourSample Dwell = AdaptiveEnvM1Tests::MakeSample(
		AgentId,
		2,
		Move.WorldLocation,
		AdaptiveEnvGameplayTags::Behaviour_Dwell.GetTag());
	Dwell.PreviousWorldLocation = Move.WorldLocation;
	Dwell.bHasPreviousLocation = true;
	Dwell.DeltaSeconds = 0.1f;
	TestEqual(TEXT("Dwell sample accepted"), Grid.AccumulateSample(Dwell), EAEBehaviourSubmitResult::Accepted);
	FAEBehaviourCellSnapshot DwellCell;
	Grid.GetCellSnapshotAtWorldLocation(Dwell.WorldLocation, DwellCell);
	TestTrue(TEXT("Dwell time accumulates"), FMath::IsNearlyEqual(DwellCell.DwellSeconds, 0.1f));

	// Verify rejected samples cannot mutate the grid revision.
	const uint64 RevisionBeforeDuplicate = Grid.GetBehaviourRevision();
	TestEqual(TEXT("Duplicate is rejected"), Grid.AccumulateSample(Dwell), EAEBehaviourSubmitResult::DuplicateSequence);
	TestEqual(TEXT("Duplicate does not change revision"), Grid.GetBehaviourRevision(), RevisionBeforeDuplicate);

	FAEBehaviourSample OldSample = First;
	OldSample.SequenceNumber = 1;
	TestEqual(TEXT("Out-of-order sample is rejected"), Grid.AccumulateSample(OldSample), EAEBehaviourSubmitResult::OutOfOrder);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAETrackerSampleTest,
	"AdaptiveEnv.M1.TrackerSample",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Verify tracker identity, first-sample semantics, and three-dimensional distance.
bool FAETrackerSampleTest::RunTest(const FString& Parameters)
{
	// Create an isolated World, Actor, root component, and tracker.
	const FName WorldName = MakeUniqueObjectName(GetTransientPackage(), UWorld::StaticClass(), TEXT("AE_M1_TrackerWorld"));
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, WorldName, GetTransientPackage(), true);
	TestNotNull(TEXT("Temporary world"), World);
	if (World == nullptr)
	{
		return false;
	}

	AActor* Actor = World->SpawnActor<AActor>();
	USceneComponent* Root = NewObject<USceneComponent>(Actor);
	Actor->SetRootComponent(Root);
	Root->RegisterComponent();
	UAEBehaviourTrackerComponent* Tracker = NewObject<UAEBehaviourTrackerComponent>(Actor);
	Actor->AddInstanceComponent(Tracker);
	Tracker->RegisterComponent();
	// Inject a deterministic agent identity before sampling.
	const FGuid AgentId = FGuid::NewGuid();
	Tracker->SetAgentIdForTesting(AgentId);

	// Verify that the first sample has no synthetic previous distance.
	Actor->SetActorLocation(FVector::ZeroVector);
	FAEBehaviourSample First;
	TestTrue(TEXT("First sample captured"), Tracker->CaptureBehaviourSample(0.1, 0.1f, First));
	TestFalse(TEXT("First sample has no previous point"), First.bHasPreviousLocation);
	TestEqual(TEXT("First distance is zero"), First.TravelDistanceMeters, 0.0f);
	TestEqual(TEXT("First sequence"), First.SequenceNumber, static_cast<int64>(0));

	// Move one metre and verify distance, sequence, and identity continuity.
	Actor->SetActorLocation(FVector(100.0, 0.0, 0.0));
	FAEBehaviourSample Second;
	TestTrue(TEXT("Second sample captured"), Tracker->CaptureBehaviourSample(0.2, 0.1f, Second));
	TestTrue(TEXT("Second sample has previous point"), Second.bHasPreviousLocation);
	TestTrue(TEXT("Tracker distance is correct"), FMath::IsNearlyEqual(Second.TravelDistanceMeters, 1.0f));
	TestEqual(TEXT("Second sequence"), Second.SequenceNumber, static_cast<int64>(1));
	TestEqual(TEXT("Agent ID is stable"), Second.AgentId, AgentId);

	// Destroy the transient World after tracker assertions complete.
	World->DestroyWorld(false);
	World->RemoveFromRoot();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAEFixedStepSchedulerTest,
	"AdaptiveEnv.M1.FixedStepScheduler",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Verify fixed-step counts remain independent from render frame rate.
bool FAEFixedStepSchedulerTest::RunTest(const FString& Parameters)
{
	// Create an isolated World and resolve its adaptive subsystem.
	const FName WorldName = MakeUniqueObjectName(GetTransientPackage(), UWorld::StaticClass(), TEXT("AE_M1_SchedulerWorld"));
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, WorldName, GetTransientPackage(), true);
	TestNotNull(TEXT("Temporary world"), World);
	if (World == nullptr)
	{
		return false;
	}

	UAEAdaptiveEnvWorldSubsystem* Subsystem = World->GetSubsystem<UAEAdaptiveEnvWorldSubsystem>();
	TestNotNull(TEXT("Subsystem"), Subsystem);
	// Verify queue acceptance, duplicate rejection, and rejection statistics.
	FAEBehaviourSample QueuedSample = AdaptiveEnvM1Tests::MakeSample(
		FGuid::NewGuid(),
		0,
		FVector::ZeroVector,
		AdaptiveEnvGameplayTags::Behaviour_Move.GetTag());
	TestEqual(TEXT("Queue accepts valid sample"), Subsystem->SubmitBehaviourSample(QueuedSample), EAEBehaviourSubmitResult::Accepted);
	TestEqual(TEXT("Queue rejects duplicate sample"), Subsystem->SubmitBehaviourSample(QueuedSample), EAEBehaviourSubmitResult::DuplicateSequence);
	TestEqual(TEXT("Queue records duplicate"), Subsystem->GetBehaviourGridStats().DuplicateSampleCount, static_cast<int64>(1));
	Subsystem->ResetBehaviourGrid();

	// Simulate one real second at 60 FPS.
	for (int32 Frame = 0; Frame < 60; ++Frame)
	{
		Subsystem->Tick(1.0f / 60.0f);
	}
	TestEqual(TEXT("Ten fixed steps at 60 FPS"), Subsystem->GetProcessedBehaviourStepCount(), static_cast<int64>(10));

	// Simulate one real second at 120 FPS.
	Subsystem->ResetBehaviourGrid();
	for (int32 Frame = 0; Frame < 120; ++Frame)
	{
		Subsystem->Tick(1.0f / 120.0f);
	}
	TestEqual(TEXT("Ten fixed steps at 120 FPS"), Subsystem->GetProcessedBehaviourStepCount(), static_cast<int64>(10));

	// Simulate one real second at 30 FPS.
	Subsystem->ResetBehaviourGrid();
	for (int32 Frame = 0; Frame < 30; ++Frame)
	{
		Subsystem->Tick(1.0f / 30.0f);
	}
	TestEqual(TEXT("Ten fixed steps at 30 FPS"), Subsystem->GetProcessedBehaviourStepCount(), static_cast<int64>(10));

	// Destroy the transient World after scheduler assertions complete.
	World->DestroyWorld(false);
	World->RemoveFromRoot();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAEDebugActiveWindowTest,
	"AdaptiveEnv.M1.DebugRenderer.ActiveWindow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verify debug queries retain recent activity until refresh and discard historical Cells afterward. */
bool FAEDebugActiveWindowTest::RunTest(const FString& Parameters)
{
	// Create an isolated World and submit one first observation at the Grid centre.
	const FName WorldName = MakeUniqueObjectName(GetTransientPackage(), UWorld::StaticClass(), TEXT("AE_M1_DebugWindowWorld"));
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, WorldName, GetTransientPackage(), true);
	TestNotNull(TEXT("Temporary world"), World);
	if (World == nullptr)
	{
		return false;
	}
	UAEAdaptiveEnvWorldSubsystem* Subsystem = World->GetSubsystem<UAEAdaptiveEnvWorldSubsystem>();
	TestNotNull(TEXT("Subsystem"), Subsystem);
	const FGuid FirstAgent = FGuid::NewGuid();
	FAEBehaviourSample First = AdaptiveEnvM1Tests::MakeSample(
		FirstAgent,
		0,
		FVector::ZeroVector,
		AdaptiveEnvGameplayTags::Behaviour_Move.GetTag());
	TestEqual(TEXT("First activity queues"), Subsystem->SubmitBehaviourSample(First), EAEBehaviourSubmitResult::Accepted);
	Subsystem->Tick(0.1f);

	// Assert the pending window contains the active Cell and bounded neighbouring snapshots.
	FAEBehaviourCellSnapshot FirstCell;
	TestTrue(TEXT("First Cell remains queryable"), Subsystem->GetBehaviourCellAtWorldLocation(FVector::ZeroVector, FirstCell));
	TArray<FAEBehaviourCellSnapshot> Cells;
	Subsystem->GetDebugCells(FVector::ZeroVector, 5000.0f, 2048, Cells);
	TestTrue(TEXT("Recent activity produces a local window"), !Cells.IsEmpty() && Cells.Num() <= 25);
	TestTrue(TEXT("Active Cell is included"), Cells.ContainsByPredicate([&FirstCell](const FAEBehaviourCellSnapshot& Cell)
	{
		return Cell.Coordinate == FirstCell.Coordinate;
	}));

	// Complete the independent debug interval, then add unrelated activity with a different Agent.
	Subsystem->Tick(0.1f);
	Subsystem->GetDebugCells(FVector::ZeroVector, 5000.0f, 2048, Cells);
	TestTrue(TEXT("Completed refresh clears the old window"), Cells.IsEmpty());
	const FVector SecondLocation(3000.0f, 0.0f, 0.0f);
	FAEBehaviourSample Second = AdaptiveEnvM1Tests::MakeSample(
		FGuid::NewGuid(),
		0,
		SecondLocation,
		AdaptiveEnvGameplayTags::Behaviour_Move.GetTag());
	TestEqual(TEXT("Second activity queues"), Subsystem->SubmitBehaviourSample(Second), EAEBehaviourSubmitResult::Accepted);
	Subsystem->Tick(0.1f);
	Subsystem->GetDebugCells(SecondLocation, 5000.0f, 2048, Cells);
	TestFalse(TEXT("Historical Cell is excluded from the new window"), Cells.ContainsByPredicate([&FirstCell](const FAEBehaviourCellSnapshot& Cell)
	{
		return Cell.Coordinate == FirstCell.Coordinate;
	}));

	// Destroy the transient World after all activity-window assertions complete.
	World->DestroyWorld(false);
	World->RemoveFromRoot();
	return true;
}

#endif
