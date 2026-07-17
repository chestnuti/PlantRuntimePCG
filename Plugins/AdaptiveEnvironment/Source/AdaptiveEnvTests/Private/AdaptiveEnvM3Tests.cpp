#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AEExposureGrid.h"
#include "AEHeatmapGrid.h"
#include "AEM3ParameterService.h"
#include "AEParameterPackageService.h"
#include "AdaptiveEnvDataAssets.h"
#include "AdaptiveEnvGameplayTags.h"

namespace AdaptiveEnvM3Tests
{
	/* Creates one valid first-version parameter set with equal six-channel weights. */
	FAEM3ParameterSet MakeValidParameters()
	{
		FAEM3ParameterSet Parameters;
		Parameters.PackageId = FGuid(0xAE000003, 0, 0, 1);
		Parameters.SemanticVersion = TEXT("1.0.0-test");
		Parameters.ContentHash = FString::ChrN(64, TEXT('a'));
		Parameters.ExposurePassReferenceCount = 1.0;
		Parameters.ExposureTravelDistanceReferenceMeters = 1.0;
		Parameters.ExposureDwellReferenceSeconds = 1.0;
		Parameters.ExposureSprintDistanceReferenceMeters = 1.0;
		Parameters.ExposureCollectEventReferenceCount = 1.0;
		Parameters.ExposureCombatEventReferenceCount = 1.0;
		Parameters.ExposurePassWeight = 1.0 / 6.0;
		Parameters.ExposureTravelDistanceWeight = 1.0 / 6.0;
		Parameters.ExposureDwellWeight = 1.0 / 6.0;
		Parameters.ExposureSprintWeight = 1.0 / 6.0;
		Parameters.ExposureCollectEventWeight = 1.0 / 6.0;
		Parameters.ExposureCombatEventWeight = 1.0 / 6.0;
		Parameters.ExposureMaximum = 1.0;
		Parameters.ExposureHalfLifeSimulationHours = 1.0;
		Parameters.DamageActivationExposure = 0.5;
		Parameters.DamageSaturationExposure = 0.8;
		Parameters.DamageMaximumRatePerSimulationHour = 0.2;
		Parameters.RecoveryActivationExposure = 0.25;
		Parameters.RecoveryDelaySimulationHours = 1.0;
		Parameters.RecoveryBaseRatePerSimulationHour = 0.1;
		return Parameters;
	}

	/* Creates one transient M2 package containing the exact 20-parameter M3 contract. */
	UAEParameterSynthesisAsset* MakeValidPackage()
	{
		UAEParameterSynthesisAsset* Package = NewObject<UAEParameterSynthesisAsset>();
		Package->Metadata.PackageId = FGuid(0xAE000003, 0, 0, 100);
		Package->Metadata.SemanticVersion = TEXT("1.0.0-test");
		Package->Metadata.CreatedAt = FDateTime(2026, 7, 17, 0, 0, 0);
		Package->Metadata.CreatedBy = TEXT("AdaptiveEnvM3Tests");
		Package->Metadata.ReviewStatus = TEXT("TestOnly");

		// Add exact effective scalar values without claiming real literature evidence.
		auto Add = [Package](const TCHAR* Name, const TCHAR* Unit, const double Value, const uint32 Id)
		{
			FAEDerivedParameter& Parameter = Package->DerivedParameters.AddDefaulted_GetRef();
			Parameter.ParameterId = FGuid(0xAE000003, 0, 0, Id);
			Parameter.ParameterName = FName(Name);
			Parameter.Description = TEXT("Transient M3 automation-test parameter.");
			Parameter.Unit = Unit;
			Parameter.ParameterVersion = TEXT("1.0.0-test");
			Parameter.EvidenceBasedValue = Value;
			Parameter.RuntimeValue = Value;
			Parameter.PlausibleMinimum = FMath::Min(Value, 0.0);
			Parameter.PlausibleMaximum = FMath::Max(Value, 1.0);
		};
		uint32 Id = 1;
		Add(TEXT("ExposurePassReferenceCount"), TEXT("count"), 1.0, Id++);
		Add(TEXT("ExposureTravelDistanceReferenceMeters"), TEXT("m"), 1.0, Id++);
		Add(TEXT("ExposureDwellReferenceSeconds"), TEXT("s"), 1.0, Id++);
		Add(TEXT("ExposureSprintDistanceReferenceMeters"), TEXT("m"), 1.0, Id++);
		Add(TEXT("ExposureCollectEventReferenceCount"), TEXT("count"), 1.0, Id++);
		Add(TEXT("ExposureCombatEventReferenceCount"), TEXT("count"), 1.0, Id++);
		Add(TEXT("ExposurePassWeight"), TEXT("ratio"), 1.0 / 6.0, Id++);
		Add(TEXT("ExposureTravelDistanceWeight"), TEXT("ratio"), 1.0 / 6.0, Id++);
		Add(TEXT("ExposureDwellWeight"), TEXT("ratio"), 1.0 / 6.0, Id++);
		Add(TEXT("ExposureSprintWeight"), TEXT("ratio"), 1.0 / 6.0, Id++);
		Add(TEXT("ExposureCollectEventWeight"), TEXT("ratio"), 1.0 / 6.0, Id++);
		Add(TEXT("ExposureCombatEventWeight"), TEXT("ratio"), 1.0 / 6.0, Id++);
		Add(TEXT("ExposureMaximum"), TEXT("ratio"), 1.0, Id++);
		Add(TEXT("ExposureHalfLifeSimulationHours"), TEXT("h"), 1.0, Id++);
		Add(TEXT("DamageActivationExposure"), TEXT("ratio"), 0.5, Id++);
		Add(TEXT("DamageSaturationExposure"), TEXT("ratio"), 0.8, Id++);
		Add(TEXT("DamageMaximumRatePerSimulationHour"), TEXT("ratio/h"), 0.2, Id++);
		Add(TEXT("RecoveryActivationExposure"), TEXT("ratio"), 0.25, Id++);
		Add(TEXT("RecoveryDelaySimulationHours"), TEXT("h"), 1.0, Id++);
		Add(TEXT("RecoveryBaseRatePerSimulationHour"), TEXT("ratio/h"), 0.1, Id++);

		// Seal the canonical test package after all effective values are populated.
		FString HashError;
		Package->Metadata.ContentHash = FAEParameterPackageService::ComputeContentHash(*Package, HashError);
		return Package;
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

/* Verify the exact 20-parameter package loads and missing or mixed-unit values fail. */
bool FAEM3RequiredParametersTest::RunTest(const FString& Parameters)
{
	// Arrange one sealed transient package and build its immutable M3 snapshot.
	UAEParameterSynthesisAsset* Package = AdaptiveEnvM3Tests::MakeValidPackage();
	FAEM3ParameterSet ParameterSet;
	TestTrue(TEXT("Complete package validates"), FAEM3ParameterService::BuildParameterSet(*Package, ParameterSet).IsValid());
	TestEqual(TEXT("All parameter versions are captured"), ParameterSet.ParameterVersions.Num(), 20);

	// Assert missing and mixed-unit contracts are rejected after resealing each mutation.
	Package->DerivedParameters.Pop();
	FString HashError;
	Package->Metadata.ContentHash = FAEParameterPackageService::ComputeContentHash(*Package, HashError);
	TestFalse(TEXT("Missing parameter fails"), FAEM3ParameterService::BuildParameterSet(*Package, ParameterSet).IsValid());
	Package = AdaptiveEnvM3Tests::MakeValidPackage();
	Package->DerivedParameters[0].Unit = TEXT("m");
	Package->Metadata.ContentHash = FAEParameterPackageService::ComputeContentHash(*Package, HashError);
	TestFalse(TEXT("Mixed unit fails"), FAEM3ParameterService::BuildParameterSet(*Package, ParameterSet).IsValid());
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
	ParameterSet.ExposurePassWeight = -0.1;
	TestFalse(TEXT("Negative weight fails"), FAEM3ParameterService::ValidateParameterSet(ParameterSet).IsValid());
	ParameterSet = AdaptiveEnvM3Tests::MakeValidParameters();
	ParameterSet.ExposurePassWeight = 0.1;
	TestFalse(TEXT("Incorrect weight sum fails"), FAEM3ParameterService::ValidateParameterSet(ParameterSet).IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAEM3ThresholdContractTest,
	"AdaptiveEnv.M3.Parameters.ThresholdOrdering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verify Recovery, Damage, Saturation, and Exposure Maximum ordering is enforced. */
bool FAEM3ThresholdContractTest::RunTest(const FString& Parameters)
{
	// Arrange valid thresholds and verify the optional neutral band is accepted.
	FAEM3ParameterSet ParameterSet = AdaptiveEnvM3Tests::MakeValidParameters();
	TestTrue(TEXT("Ordered thresholds validate"), FAEM3ParameterService::ValidateParameterSet(ParameterSet).IsValid());

	// Assert reversed and degenerate threshold relationships are rejected.
	ParameterSet.RecoveryActivationExposure = 0.6;
	TestFalse(TEXT("Recovery above Damage activation fails"), FAEM3ParameterService::ValidateParameterSet(ParameterSet).IsValid());
	ParameterSet = AdaptiveEnvM3Tests::MakeValidParameters();
	ParameterSet.DamageSaturationExposure = ParameterSet.DamageActivationExposure;
	TestFalse(TEXT("Degenerate Damage ramp fails"), FAEM3ParameterService::ValidateParameterSet(ParameterSet).IsValid());
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
	ParameterSet.ExposurePassWeight = 1.0;
	ParameterSet.ExposureTravelDistanceWeight = 0.0;
	ParameterSet.ExposureDwellWeight = 0.0;
	ParameterSet.ExposureSprintWeight = 0.0;
	ParameterSet.ExposureCollectEventWeight = 0.0;
	ParameterSet.ExposureCombatEventWeight = 0.0;
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
	ParameterSet.ExposurePassWeight = 0.0;
	ParameterSet.ExposureTravelDistanceWeight = 0.5;
	ParameterSet.ExposureDwellWeight = 0.0;
	ParameterSet.ExposureSprintWeight = 0.5;
	ParameterSet.ExposureCollectEventWeight = 0.0;
	ParameterSet.ExposureCombatEventWeight = 0.0;
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
	ParameterSet.ExposurePassWeight = 1.0;
	ParameterSet.ExposureTravelDistanceWeight = 0.0;
	ParameterSet.ExposureDwellWeight = 0.0;
	ParameterSet.ExposureSprintWeight = 0.0;
	ParameterSet.ExposureCollectEventWeight = 0.0;
	ParameterSet.ExposureCombatEventWeight = 0.0;
	ParameterSet.DamageMaximumRatePerSimulationHour = 0.0;
	ParameterSet.RecoveryBaseRatePerSimulationHour = 0.0;
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
	FAEM3DamageResponseTest,
	"AdaptiveEnv.M3.Response.DamagePiecewiseBoundaries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verify the first-version Damage ramp interpolates and integrates in simulated hours. */
bool FAEM3DamageResponseTest::RunTest(const FString& Parameters)
{
	// Arrange Pass Exposure at the midpoint of a 0.25 to 0.75 Damage ramp.
	FAEHeatmapGrid RawGrid;
	FAEExposureGrid ExposureGrid;
	TestTrue(TEXT("Grids initialize"), AdaptiveEnvM3Tests::InitializeGrids(RawGrid, ExposureGrid));
	FAEM3ParameterSet ParameterSet = AdaptiveEnvM3Tests::MakeValidParameters();
	ParameterSet.ExposurePassReferenceCount = 2.0;
	ParameterSet.ExposurePassWeight = 1.0;
	ParameterSet.ExposureTravelDistanceWeight = 0.0;
	ParameterSet.ExposureDwellWeight = 0.0;
	ParameterSet.ExposureSprintWeight = 0.0;
	ParameterSet.ExposureCollectEventWeight = 0.0;
	ParameterSet.ExposureCombatEventWeight = 0.0;
	ParameterSet.ExposureHalfLifeSimulationHours = 1000000.0;
	ParameterSet.DamageActivationExposure = 0.25;
	ParameterSet.DamageSaturationExposure = 0.75;
	ParameterSet.DamageMaximumRatePerSimulationHour = 0.2;
	AdaptiveEnvM3Tests::AddPass(RawGrid);

	// Integrate one simulated hour and assert midpoint rate and bounded state.
	TestTrue(TEXT("Damage update succeeds"), ExposureGrid.Update(RawGrid, RawGrid.GetDirtyCellIndices(), 1.0, 1.0, RawGrid.GetBehaviourRevision(), ParameterSet));
	FAEM3CellSnapshot Snapshot;
	ExposureGrid.GetCellSnapshot(FIntPoint::ZeroValue, Snapshot);
	TestTrue(TEXT("Midpoint Damage rate is linear"), FMath::IsNearlyEqual(Snapshot.DamageRatePerSimulationHour, 0.1f, 1.0e-5f));
	TestTrue(TEXT("One-hour Damage integrates rate"), FMath::IsNearlyEqual(Snapshot.EcologicalDamageRatio, 0.1f, 1.0e-5f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAEM3RecoveryDelayTest,
	"AdaptiveEnv.M3.Response.RecoveryDelay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verify Recovery starts only after continuous low Exposure reaches the configured delay. */
bool FAEM3RecoveryDelayTest::RunTest(const FString& Parameters)
{
	// Arrange one damaged Cell whose Exposure decays rapidly below the Recovery threshold.
	FAEHeatmapGrid RawGrid;
	FAEExposureGrid ExposureGrid;
	TestTrue(TEXT("Grids initialize"), AdaptiveEnvM3Tests::InitializeGrids(RawGrid, ExposureGrid));
	FAEM3ParameterSet ParameterSet = AdaptiveEnvM3Tests::MakeValidParameters();
	ParameterSet.ExposurePassWeight = 1.0;
	ParameterSet.ExposureTravelDistanceWeight = 0.0;
	ParameterSet.ExposureDwellWeight = 0.0;
	ParameterSet.ExposureSprintWeight = 0.0;
	ParameterSet.ExposureCollectEventWeight = 0.0;
	ParameterSet.ExposureCombatEventWeight = 0.0;
	ParameterSet.ExposureHalfLifeSimulationHours = 0.01;
	ParameterSet.DamageActivationExposure = 0.2;
	ParameterSet.DamageSaturationExposure = 0.8;
	ParameterSet.DamageMaximumRatePerSimulationHour = 1.0;
	ParameterSet.RecoveryDelaySimulationHours = 1.0;
	ParameterSet.RecoveryBaseRatePerSimulationHour = 0.2;
	AdaptiveEnvM3Tests::AddPass(RawGrid);
	ExposureGrid.Update(RawGrid, RawGrid.GetDirtyCellIndices(), 0.5, 0.5, RawGrid.GetBehaviourRevision(), ParameterSet);
	FAEM3CellSnapshot Damaged;
	ExposureGrid.GetCellSnapshot(FIntPoint::ZeroValue, Damaged);

	// Advance two low-Exposure half-hour steps and assert Recovery begins on the second.
	const TArray<int32> NoDirty;
	ExposureGrid.Update(RawGrid, NoDirty, 1.0, 0.5, RawGrid.GetBehaviourRevision(), ParameterSet);
	FAEM3CellSnapshot BeforeDelay;
	ExposureGrid.GetCellSnapshot(FIntPoint::ZeroValue, BeforeDelay);
	ExposureGrid.Update(RawGrid, NoDirty, 1.5, 0.5, RawGrid.GetBehaviourRevision(), ParameterSet);
	FAEM3CellSnapshot AfterDelay;
	ExposureGrid.GetCellSnapshot(FIntPoint::ZeroValue, AfterDelay);
	TestTrue(TEXT("Initial Damage is positive"), Damaged.EcologicalDamageRatio > 0.0f);
	TestTrue(TEXT("Recovery is zero before delay"), FMath::IsNearlyZero(BeforeDelay.RecoveryRatePerSimulationHour, 1.0e-6f));
	TestTrue(TEXT("Recovery starts at delay"), FMath::IsNearlyEqual(AfterDelay.RecoveryRatePerSimulationHour, 0.2f, 1.0e-6f));
	TestTrue(TEXT("Recovery reduces Damage"), AfterDelay.EcologicalDamageRatio < BeforeDelay.EcologicalDamageRatio);
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
	TestTrue(TEXT("Damage resets"), FMath::IsNearlyZero(Snapshot.EcologicalDamageRatio, 1.0e-6f));
	TestEqual(TEXT("Exposure revision resets"), ExposureGrid.GetExposureRevision(), static_cast<uint64>(0));
	TestEqual(TEXT("Response revision resets"), ExposureGrid.GetResponseRevision(), static_cast<uint64>(0));
	TestEqual(TEXT("Active count resets"), ExposureGrid.GetActiveCellCount(), 0);
	return true;
}

#endif
