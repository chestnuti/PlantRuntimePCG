#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AEExposureGrid.h"
#include "AEHeatmapGrid.h"
#include "AEParameterBundleService.h"
#include "AEM3ParameterService.h"
#include "AdaptiveEnvGameplayTags.h"

namespace AdaptiveEnvM3Tests
{
	/* Creates one valid first-version parameter set with equal six-channel weights. */
	FAEM3ParameterSet MakeValidParameters()
	{
		FAEM3ParameterSet Parameters;
		for (int32 Index = 0; Index < static_cast<int32>(EAEExposureChannel::Count); ++Index)
		{
			Parameters.Channels[Index].ReferenceValue = 1.0;
			Parameters.Channels[Index].Weight = 1.0 / 6.0;
		}
		Parameters.ExposureDynamics.Maximum = 1.0;
		Parameters.ExposureDynamics.HalfLifeSimulationHours = 1.0;
		return Parameters;
	}

	/* Creates one canonical transient block containing the exact 14-parameter M3 contract. */
	FAEParameterBlock MakeValidBlock()
	{
		FAEParameterBlock Block;
		Block.BlockId = FGuid(0xAE000003, 0, 0, 100);
		Block.ModelContract = FAEParameterBundleService::M3ModelContract();
		Block.BlockVersion = TEXT("1.0.0");
		auto Add = [&Block](const TCHAR* Name, const TCHAR* Unit, const double Value, const uint32 Id)
		{
			FAEPublishedParameter& Parameter = Block.Parameters.AddDefaulted_GetRef();
			Parameter.ParameterId = FGuid(0xAE000003, 0, 0, Id);
			Parameter.Name = FName(Name);
			Parameter.Unit = Unit;
			Parameter.ParameterVersion = TEXT("1.0.0");
			Parameter.EvidenceBasedValue = Value;
			Parameter.EffectiveValue = Value;
			Parameter.PlausibleMinimum = FMath::Min(Value, 0.0);
			Parameter.PlausibleMaximum = FMath::Max(Value, 1.0);
		};
		uint32 Id = 1;
		Add(TEXT("ExposureCollectEventReferenceCount"), TEXT("count"), 1.0, Id++);
		Add(TEXT("ExposureCollectEventWeight"), TEXT("ratio"), 1.0 / 6.0, Id++);
		Add(TEXT("ExposureCombatEventReferenceCount"), TEXT("count"), 1.0, Id++);
		Add(TEXT("ExposureCombatEventWeight"), TEXT("ratio"), 1.0 / 6.0, Id++);
		Add(TEXT("ExposureDwellReferenceSeconds"), TEXT("s"), 1.0, Id++);
		Add(TEXT("ExposureDwellWeight"), TEXT("ratio"), 1.0 / 6.0, Id++);
		Add(TEXT("ExposureHalfLifeSimulationHours"), TEXT("h"), 1.0, Id++);
		Add(TEXT("ExposureMaximum"), TEXT("ratio"), 1.0, Id++);
		Add(TEXT("ExposurePassReferenceCount"), TEXT("count"), 1.0, Id++);
		Add(TEXT("ExposurePassWeight"), TEXT("ratio"), 1.0 / 6.0, Id++);
		Add(TEXT("ExposureSprintDistanceReferenceMeters"), TEXT("m"), 1.0, Id++);
		Add(TEXT("ExposureSprintWeight"), TEXT("ratio"), 1.0 / 6.0, Id++);
		Add(TEXT("ExposureTravelDistanceReferenceMeters"), TEXT("m"), 1.0, Id++);
		Add(TEXT("ExposureTravelDistanceWeight"), TEXT("ratio"), 1.0 / 6.0, Id++);
		return Block;
	}

	/* Initializes aligned one-Cell raw and M3 Grids for deterministic model tests. */
	bool InitializeGrids(FAEHeatmapGrid& OutRawGrid, FAEExposureGrid& OutExposureGrid)
	{
		FAEHeatmapGridConfig Config;
		Config.Dimensions = FIntPoint(1, 1);
		Config.WorldCenter = FVector2D::ZeroVector;
		Config.CellSizeCm = 100.0f;
		Config.KernelRadiusCells = 0;
		Config.KernelSigma = 1.0f;
		return OutRawGrid.Initialize(Config) && OutExposureGrid.Initialize(Config);
	}

	/* Adds one first observation that contributes a Pass to the only Cell. */
	void AddPass(FAEHeatmapGrid& Grid, const int64 Sequence = 0)
	{
		FAEBehaviourSample Sample;
		Sample.AgentId = FGuid(0xAE000003, 0, 1, 1);
		Sample.WorldLocation = FVector::ZeroVector;
		Sample.Timestamp = static_cast<double>(Sequence);
		Sample.DeltaSeconds = 0.1f;
		Sample.SequenceNumber = Sequence;
		Sample.BehaviourTag = AdaptiveEnvGameplayTags::Behaviour_Move.GetTag();
		Grid.AccumulateSample(Sample);
	}

	/* Adds one sprint segment with both total Travel and Sprint distance in the only Cell. */
	void AddSprint(FAEHeatmapGrid& Grid, const float DistanceMeters, const int64 Sequence = 1)
	{
		FAEBehaviourSample Sample;
		Sample.AgentId = FGuid(0xAE000003, 0, 2, 1);
		Sample.PreviousWorldLocation = FVector(-10.0, 0.0, 0.0);
		Sample.WorldLocation = FVector(10.0, 0.0, 0.0);
		Sample.Timestamp = static_cast<double>(Sequence);
		Sample.DeltaSeconds = 0.1f;
		Sample.TravelDistanceMeters = DistanceMeters;
		Sample.SequenceNumber = Sequence;
		Sample.bHasPreviousLocation = true;
		Sample.BehaviourTag = AdaptiveEnvGameplayTags::Behaviour_Sprint.GetTag();
		Grid.AccumulateSample(Sample);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAEM3RequiredParametersTest,
	"AdaptiveEnv.M3.Parameters.RequiredAndUnits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verify the exact 14-parameter block maps into the grouped M3 snapshot. */
bool FAEM3RequiredParametersTest::RunTest(const FString& Parameters)
{
	// Arrange one canonical transient block and valid parent bundle identity.
	FAEParameterBlock Block = AdaptiveEnvM3Tests::MakeValidBlock();
	FAEParameterBlockView View{ &Block };
	FAEParameterBundleIdentity Identity{ FGuid(0xAE000003, 0, 0, 200), TEXT("1.0.0"), FString::ChrN(64, TEXT('a')) };
	FAEM3ParameterSet ParameterSet;
	TestTrue(TEXT("Complete block validates"), FAEM3ParameterService::BuildParameterSet(View, Identity, ParameterSet).IsValid());
	TestTrue(TEXT("Pass maps to fixed channel"), FMath::IsNearlyEqual(ParameterSet.Channel(EAEExposureChannel::Pass).ReferenceValue, 1.0));
	Block.Parameters.Pop();
	TestFalse(TEXT("Missing parameter fails"), FAEM3ParameterService::BuildParameterSet(View, Identity, ParameterSet).IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAEM3WeightContractTest,
	"AdaptiveEnv.M3.Parameters.WeightContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verify M3 rejects negative weights and weight totals that differ from one. */
bool FAEM3WeightContractTest::RunTest(const FString& Parameters)
{
	// Arrange one valid direct parameter snapshot.
	FAEM3ParameterSet ParameterSet = AdaptiveEnvM3Tests::MakeValidParameters();
	TestTrue(TEXT("Equal weights validate"), FAEM3ParameterService::ValidateParameterSet(ParameterSet).IsValid());

	// Assert negative and unnormalized weights produce blocking findings.
	ParameterSet.Channel(EAEExposureChannel::Pass).Weight = -0.1;
	TestFalse(TEXT("Negative weight fails"), FAEM3ParameterService::ValidateParameterSet(ParameterSet).IsValid());
	ParameterSet = AdaptiveEnvM3Tests::MakeValidParameters();
	ParameterSet.Channel(EAEExposureChannel::Pass).Weight = 0.1;
	TestFalse(TEXT("Incorrect weight sum fails"), FAEM3ParameterService::ValidateParameterSet(ParameterSet).IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAEM3DynamicsContractTest,
	"AdaptiveEnv.M3.Parameters.Dynamics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verify Exposure maximum and half-life remain positive. */
bool FAEM3DynamicsContractTest::RunTest(const FString& Parameters)
{
	// Arrange valid dynamics and verify both positive values are accepted.
	FAEM3ParameterSet ParameterSet = AdaptiveEnvM3Tests::MakeValidParameters();
	TestTrue(TEXT("Positive dynamics validate"), FAEM3ParameterService::ValidateParameterSet(ParameterSet).IsValid());

	// Assert non-positive dynamics are rejected.
	ParameterSet.ExposureDynamics.Maximum = 0.0;
	TestFalse(TEXT("Zero maximum fails"), FAEM3ParameterService::ValidateParameterSet(ParameterSet).IsValid());
	ParameterSet = AdaptiveEnvM3Tests::MakeValidParameters();
	ParameterSet.ExposureDynamics.HalfLifeSimulationHours = 0.0;
	TestFalse(TEXT("Zero half-life fails"), FAEM3ParameterService::ValidateParameterSet(ParameterSet).IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAEM3CumulativeDeltaTest,
	"AdaptiveEnv.M3.Exposure.CumulativeDeltaConsumedOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verify one cumulative M1 contribution is consumed once even when the Cell remains dirty. */
bool FAEM3CumulativeDeltaTest::RunTest(const FString& Parameters)
{
	// Arrange aligned Grids, a Pass-only model, and one raw Pass.
	FAEHeatmapGrid RawGrid;
	FAEExposureGrid ExposureGrid;
	TestTrue(TEXT("Grids initialize"), AdaptiveEnvM3Tests::InitializeGrids(RawGrid, ExposureGrid));
	FAEM3ParameterSet ParameterSet = AdaptiveEnvM3Tests::MakeValidParameters();
	ParameterSet.Channel(EAEExposureChannel::Pass).Weight = 1.0;
	ParameterSet.Channel(EAEExposureChannel::Travel).Weight = 0.0;
	ParameterSet.Channel(EAEExposureChannel::Dwell).Weight = 0.0;
	ParameterSet.Channel(EAEExposureChannel::Sprint).Weight = 0.0;
	ParameterSet.Channel(EAEExposureChannel::Collect).Weight = 0.0;
	ParameterSet.Channel(EAEExposureChannel::Combat).Weight = 0.0;
	AdaptiveEnvM3Tests::AddPass(RawGrid);
	const TArray<int32> Dirty = RawGrid.GetDirtyCellIndices();

	// Consume twice with zero elapsed time and assert the cumulative value is not added twice.
	TestTrue(TEXT("First update succeeds"), ExposureGrid.Update(RawGrid, Dirty, 0.0, 0.0, RawGrid.GetBehaviourRevision(), ParameterSet));
	FAEM3CellSnapshot First;
	ExposureGrid.GetCellSnapshot(FIntPoint::ZeroValue, First);
	TestTrue(TEXT("Second update succeeds"), ExposureGrid.Update(RawGrid, Dirty, 0.0, 0.0, RawGrid.GetBehaviourRevision(), ParameterSet));
	FAEM3CellSnapshot Second;
	ExposureGrid.GetCellSnapshot(FIntPoint::ZeroValue, Second);
	TestTrue(TEXT("Exposure is added once"), FMath::IsNearlyEqual(First.CurrentExposure, 1.0f, 1.0e-6f));
	TestTrue(TEXT("Repeated cumulative snapshot is idempotent"), FMath::IsNearlyEqual(Second.CurrentExposure, First.CurrentExposure, 1.0e-6f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAEM3SprintTravelTest,
	"AdaptiveEnv.M3.Exposure.SprintTravelNoDoubleCount",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verify Sprint distance is removed from ordinary Travel before normalization. */
bool FAEM3SprintTravelTest::RunTest(const FString& Parameters)
{
	// Arrange one pure Sprint segment with only Travel and Sprint weights enabled.
	FAEHeatmapGrid RawGrid;
	FAEExposureGrid ExposureGrid;
	TestTrue(TEXT("Grids initialize"), AdaptiveEnvM3Tests::InitializeGrids(RawGrid, ExposureGrid));
	FAEM3ParameterSet ParameterSet = AdaptiveEnvM3Tests::MakeValidParameters();
	ParameterSet.Channel(EAEExposureChannel::Pass).Weight = 0.0;
	ParameterSet.Channel(EAEExposureChannel::Travel).Weight = 0.5;
	ParameterSet.Channel(EAEExposureChannel::Dwell).Weight = 0.0;
	ParameterSet.Channel(EAEExposureChannel::Sprint).Weight = 0.5;
	ParameterSet.Channel(EAEExposureChannel::Collect).Weight = 0.0;
	ParameterSet.Channel(EAEExposureChannel::Combat).Weight = 0.0;
	AdaptiveEnvM3Tests::AddSprint(RawGrid, 1.0f);

	// Assert Sprint contributes while non-sprint Travel remains zero.
	TestTrue(TEXT("Update succeeds"), ExposureGrid.Update(RawGrid, RawGrid.GetDirtyCellIndices(), 0.0, 0.0, RawGrid.GetBehaviourRevision(), ParameterSet));
	FAEM3CellSnapshot Snapshot;
	ExposureGrid.GetCellSnapshot(FIntPoint::ZeroValue, Snapshot);
	TestTrue(TEXT("Ordinary Travel excludes Sprint"), FMath::IsNearlyZero(Snapshot.TravelExposure, 1.0e-6f));
	TestTrue(TEXT("Sprint remains normalized"), FMath::IsNearlyEqual(Snapshot.SprintExposure, 1.0f, 1.0e-6f));
	TestTrue(TEXT("No double count in total Exposure"), FMath::IsNearlyEqual(Snapshot.CurrentExposure, 0.5f, 1.0e-6f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAEM3HalfLifeTest,
	"AdaptiveEnv.M3.Exposure.HalfLifeSimulationTime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verify one simulated half-life halves Exposure independently of render time. */
bool FAEM3HalfLifeTest::RunTest(const FString& Parameters)
{
	// Arrange a saturated Pass Exposure and disable ecological rates.
	FAEHeatmapGrid RawGrid;
	FAEExposureGrid ExposureGrid;
	TestTrue(TEXT("Grids initialize"), AdaptiveEnvM3Tests::InitializeGrids(RawGrid, ExposureGrid));
	FAEM3ParameterSet ParameterSet = AdaptiveEnvM3Tests::MakeValidParameters();
	ParameterSet.Channel(EAEExposureChannel::Pass).Weight = 1.0;
	ParameterSet.Channel(EAEExposureChannel::Travel).Weight = 0.0;
	ParameterSet.Channel(EAEExposureChannel::Dwell).Weight = 0.0;
	ParameterSet.Channel(EAEExposureChannel::Sprint).Weight = 0.0;
	ParameterSet.Channel(EAEExposureChannel::Collect).Weight = 0.0;
	ParameterSet.Channel(EAEExposureChannel::Combat).Weight = 0.0;
	AdaptiveEnvM3Tests::AddPass(RawGrid);
	ExposureGrid.Update(RawGrid, RawGrid.GetDirtyCellIndices(), 0.0, 0.0, RawGrid.GetBehaviourRevision(), ParameterSet);

	// Advance exactly one simulated hour with no new raw contribution.
	const TArray<int32> NoDirty;
	TestTrue(TEXT("Decay update succeeds"), ExposureGrid.Update(RawGrid, NoDirty, 1.0, 1.0, RawGrid.GetBehaviourRevision(), ParameterSet));
	FAEM3CellSnapshot Snapshot;
	ExposureGrid.GetCellSnapshot(FIntPoint::ZeroValue, Snapshot);
	TestTrue(TEXT("One half-life halves Exposure"), FMath::IsNearlyEqual(Snapshot.CurrentExposure, 0.5f, 1.0e-6f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAEM3SixComponentsTest,
	"AdaptiveEnv.M3.Exposure.SixComponents",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verify Pass, ordinary Travel, Dwell, Sprint, Collect, and Combat remain separately observable. */
bool FAEM3SixComponentsTest::RunTest(const FString& Parameters)
{
	// Arrange six behavior channels in one Cell with unit references and equal weights.
	FAEHeatmapGrid RawGrid;
	FAEExposureGrid ExposureGrid;
	TestTrue(TEXT("Grids initialize"), AdaptiveEnvM3Tests::InitializeGrids(RawGrid, ExposureGrid));
	const FAEM3ParameterSet ParameterSet = AdaptiveEnvM3Tests::MakeValidParameters();
	AdaptiveEnvM3Tests::AddPass(RawGrid, 0);

	FAEBehaviourSample Move;
	Move.AgentId = FGuid(0xAE000003, 0, 3, 1);
	Move.PreviousWorldLocation = FVector(-10.0, 0.0, 0.0);
	Move.WorldLocation = FVector(10.0, 0.0, 0.0);
	Move.Timestamp = 1.0;
	Move.DeltaSeconds = 0.1f;
	Move.TravelDistanceMeters = 1.0f;
	Move.SequenceNumber = 0;
	Move.bHasPreviousLocation = true;
	Move.BehaviourTag = AdaptiveEnvGameplayTags::Behaviour_Move.GetTag();
	RawGrid.AccumulateSample(Move);

	FAEBehaviourSample Dwell;
	Dwell.AgentId = FGuid(0xAE000003, 0, 4, 1);
	Dwell.WorldLocation = FVector::ZeroVector;
	Dwell.Timestamp = 1.0;
	Dwell.DeltaSeconds = 1.0f;
	Dwell.SequenceNumber = 0;
	Dwell.BehaviourTag = AdaptiveEnvGameplayTags::Behaviour_Dwell.GetTag();
	RawGrid.AccumulateSample(Dwell);
	AdaptiveEnvM3Tests::AddSprint(RawGrid, 1.0f, 1);

	FAEBehaviourSample Event;
	Event.AgentId = FGuid(0xAE000003, 0, 5, 1);
	Event.WorldLocation = FVector::ZeroVector;
	Event.Timestamp = 1.0;
	Event.EventIntensity = 1.0f;
	Event.SequenceNumber = 0;
	Event.BehaviourTag = AdaptiveEnvGameplayTags::Behaviour_Collect.GetTag();
	RawGrid.AccumulateSample(Event);
	Event.AgentId = FGuid(0xAE000003, 0, 6, 1);
	Event.BehaviourTag = AdaptiveEnvGameplayTags::Behaviour_Combat.GetTag();
	RawGrid.AccumulateSample(Event);

	// Consume once and assert every normalized channel reaches one without semantic merging.
	TestTrue(TEXT("Update succeeds"), ExposureGrid.Update(RawGrid, RawGrid.GetDirtyCellIndices(), 0.0, 0.0, RawGrid.GetBehaviourRevision(), ParameterSet));
	FAEM3CellSnapshot Snapshot;
	ExposureGrid.GetCellSnapshot(FIntPoint::ZeroValue, Snapshot);
	TestTrue(TEXT("Pass component is one"), FMath::IsNearlyEqual(Snapshot.PassExposure, 1.0f, 1.0e-6f));
	TestTrue(TEXT("Travel component is one"), FMath::IsNearlyEqual(Snapshot.TravelExposure, 1.0f, 1.0e-6f));
	TestTrue(TEXT("Dwell component is one"), FMath::IsNearlyEqual(Snapshot.DwellExposure, 1.0f, 1.0e-6f));
	TestTrue(TEXT("Sprint component is one"), FMath::IsNearlyEqual(Snapshot.SprintExposure, 1.0f, 1.0e-6f));
	TestTrue(TEXT("Collect component is one"), FMath::IsNearlyEqual(Snapshot.CollectExposure, 1.0f, 1.0e-6f));
	TestTrue(TEXT("Combat component is one"), FMath::IsNearlyEqual(Snapshot.CombatExposure, 1.0f, 1.0e-6f));
	TestTrue(TEXT("Equal weights sum to one Exposure"), FMath::IsNearlyEqual(Snapshot.CurrentExposure, 1.0f, 1.0e-6f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAEM3ResetTest,
	"AdaptiveEnv.M3.Integration.ResetAndRevision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verify Reset clears Cell state, active tracking, and global revisions idempotently. */
bool FAEM3ResetTest::RunTest(const FString& Parameters)
{
	// Arrange one changed M3 Cell.
	FAEHeatmapGrid RawGrid;
	FAEExposureGrid ExposureGrid;
	TestTrue(TEXT("Grids initialize"), AdaptiveEnvM3Tests::InitializeGrids(RawGrid, ExposureGrid));
	FAEM3ParameterSet ParameterSet = AdaptiveEnvM3Tests::MakeValidParameters();
	AdaptiveEnvM3Tests::AddPass(RawGrid);
	ExposureGrid.Update(RawGrid, RawGrid.GetDirtyCellIndices(), 1.0, 1.0, RawGrid.GetBehaviourRevision(), ParameterSet);
	TestTrue(TEXT("Exposure revision advances"), ExposureGrid.GetExposureRevision() > 0);

	// Reset twice and assert all observable M3 state returns to baseline.
	ExposureGrid.Reset();
	ExposureGrid.Reset();
	FAEM3CellSnapshot Snapshot;
	TestTrue(TEXT("Cell remains queryable"), ExposureGrid.GetCellSnapshot(FIntPoint::ZeroValue, Snapshot));
	TestTrue(TEXT("Exposure resets"), FMath::IsNearlyZero(Snapshot.CurrentExposure, 1.0e-6f));
	TestEqual(TEXT("Exposure revision resets"), ExposureGrid.GetExposureRevision(), static_cast<uint64>(0));
	TestEqual(TEXT("Active count resets"), ExposureGrid.GetActiveCellCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAEM3DebugRangeQueryTest,
	"AdaptiveEnv.M3.DebugRenderer.RangeQuery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verify active M3 debug queries enforce radius, budget, nearest order, and stable ties. */
bool FAEM3DebugRangeQueryTest::RunTest(const FString& Parameters)
{
	// Arrange aligned three-by-three Grids with one active Pass in every Cell.
	FAEHeatmapGridConfig Config;
	Config.Dimensions = FIntPoint(3, 3);
	Config.WorldCenter = FVector2D::ZeroVector;
	Config.CellSizeCm = 100.0f;
	Config.KernelRadiusCells = 0;
	Config.KernelSigma = 1.0f;
	FAEHeatmapGrid RawGrid;
	FAEExposureGrid ExposureGrid;
	TestTrue(TEXT("Raw Grid initializes"), RawGrid.Initialize(Config));
	TestTrue(TEXT("Exposure Grid initializes"), ExposureGrid.Initialize(Config));
	int32 AgentIndex = 1;
	for (int32 Y = 0; Y < Config.Dimensions.Y; ++Y)
	{
		for (int32 X = 0; X < Config.Dimensions.X; ++X)
		{
			FAEBehaviourSample Sample;
			Sample.AgentId = FGuid(0xAE000003, 0, 20, AgentIndex++);
			Sample.WorldLocation = FVector(-100.0f + X * 100.0f, -100.0f + Y * 100.0f, 0.0f);
			Sample.Timestamp = 0.0;
			Sample.DeltaSeconds = 0.1f;
			Sample.SequenceNumber = 0;
			Sample.BehaviourTag = AdaptiveEnvGameplayTags::Behaviour_Move.GetTag();
			TestEqual(TEXT("Cell Pass is accepted"), RawGrid.AccumulateSample(Sample), EAEBehaviourSubmitResult::Accepted);
		}
	}
	FAEM3ParameterSet ParameterSet = AdaptiveEnvM3Tests::MakeValidParameters();
	ParameterSet.Channel(EAEExposureChannel::Pass).Weight = 1.0;
	ParameterSet.Channel(EAEExposureChannel::Travel).Weight = 0.0;
	ParameterSet.Channel(EAEExposureChannel::Dwell).Weight = 0.0;
	ParameterSet.Channel(EAEExposureChannel::Sprint).Weight = 0.0;
	ParameterSet.Channel(EAEExposureChannel::Collect).Weight = 0.0;
	ParameterSet.Channel(EAEExposureChannel::Combat).Weight = 0.0;
	TestTrue(TEXT("M3 update succeeds"), ExposureGrid.Update(
		RawGrid, RawGrid.GetDirtyCellIndices(), 0.0, 0.0, RawGrid.GetBehaviourRevision(), ParameterSet));

	// Assert a small radius isolates one Cell and a budgeted query prefers nearest stable indices.
	TArray<FAEM3CellSnapshot> Cells;
	ExposureGrid.GetActiveCellsInRadius(FVector(-100.0f, -100.0f, 0.0f), 10.0f, 9, Cells);
	TestEqual(TEXT("Small radius returns one Cell"), Cells.Num(), 1);
	if (Cells.Num() == 1)
	{
		TestEqual(TEXT("Small radius returns queried Cell"), Cells[0].Coordinate, FIntPoint(0, 0));
	}
	ExposureGrid.GetActiveCellsInRadius(FVector(-100.0f, -100.0f, 0.0f), 1000.0f, 2, Cells);
	TestEqual(TEXT("Debug budget is enforced"), Cells.Num(), 2);
	if (Cells.Num() == 2)
	{
		TestEqual(TEXT("Nearest Cell is first"), Cells[0].Coordinate, FIntPoint(0, 0));
		TestEqual(TEXT("Equal-distance tie uses stable row-major index"), Cells[1].Coordinate, FIntPoint(1, 0));
	}
	ExposureGrid.GetActiveCellsInRadius(FVector::ZeroVector, 1000.0f, 0, Cells);
	TestTrue(TEXT("Zero budget clears output"), Cells.IsEmpty());
	return true;
}

#endif
