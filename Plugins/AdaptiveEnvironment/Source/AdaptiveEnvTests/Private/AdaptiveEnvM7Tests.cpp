#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AEVegetationPatchComponent.h"
#include "AEVegetationPatchStateService.h"

namespace AdaptiveEnvM7Tests
{
	/* Creates one valid deterministic species parameter set. */
	FAEM7SpeciesParameters MakeParameters()
	{
		FAEM7SpeciesParameters Parameters;
		Parameters.DamageToleranceRatio = 0.2;
		Parameters.DamageResponseExponent = 1.0;
		Parameters.MinimumDensityRatio = 0.1;
		Parameters.DeclineRatePerSimulationHour = 0.25;
		Parameters.RecoveryRatePerSimulationHour = 0.1;
		Parameters.HealthDirtyEpsilon = 1.0e-6;
		return Parameters;
	}

	/* Creates one registered two-Cell Patch owned by a transient non-ticking component. */
	bool RegisterTwoCellPatch(
		FAEVegetationPatchStateService& Service,
		UAEVegetationPatchComponent*& OutComponent,
		FGuid& OutPatchId)
	{
		OutComponent = NewObject<UAEVegetationPatchComponent>();
		OutPatchId = FGuid(0xAE000007, 0, 0, 1);
		FAEVegetationPatchRegistration Registration;
		Registration.PatchId = OutPatchId;
		Registration.SpeciesId = FGuid(0xAE000007, 0, 0, 2);
		Registration.RegistrationGeneration = 1;
		Registration.WeightedCells = {{0, 0.25}, {1, 0.75}};
		Registration.Parameters = MakeParameters();
		Registration.Component = OutComponent;
		return Service.RegisterPatch(Registration);
	}

	/* Creates one minimal committed M5 consumer view. */
	FAEM5ConsumerCellView MakeView(
		const int32 CellIndex,
		const double Damage,
		const uint64 Revision,
		const uint64 Step)
	{
		FAEM5ConsumerCellView View;
		View.CellIndex = CellIndex;
		View.DamageRatio = Damage;
		View.ResponseRevision = Revision;
		View.ResponseSimulationStep = Step;
		return View;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAEM7AggregationAndDeclineTest,
	"AdaptiveEnv.M7.Response.AggregationAndDecline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verifies weighted M5 Damage produces bounded target and current health. */
bool FAEM7AggregationAndDeclineTest::RunTest(const FString& Parameters)
{
	FAEVegetationPatchStateService Service;
	TestTrue(TEXT("M7 service initializes"), Service.Initialize(2));
	UAEVegetationPatchComponent* Component = nullptr;
	FGuid PatchId;
	TestTrue(
		TEXT("Two-Cell Patch registers"),
		AdaptiveEnvM7Tests::RegisterTwoCellPatch(Service, Component, PatchId));
	const TArray<FAEM5ConsumerCellView> Workset = {
		AdaptiveEnvM7Tests::MakeView(0, 0.2, 1, 1),
		AdaptiveEnvM7Tests::MakeView(1, 1.0, 1, 1)};
	TestTrue(TEXT("M7 update succeeds"), Service.Update(Workset, {0, 1}, 1.0, 1));
	FAEVegetationPatchSnapshot Snapshot;
	TestTrue(TEXT("Patch snapshot is available"), Service.GetPatchSnapshot(PatchId, Snapshot));
	TestTrue(TEXT("Weighted Damage lowers target health"), Snapshot.TargetHealthRatio < 0.5f);
	TestTrue(TEXT("Decline rate limits current health"), FMath::IsNearlyEqual(Snapshot.HealthRatio, 0.75f, 1.0e-6f));
	TestTrue(TEXT("Density is derived and bounded"), Snapshot.DensityRatio > 0.1f && Snapshot.DensityRatio < 1.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAEM7ActiveRecoveryTest,
	"AdaptiveEnv.M7.Response.ActiveTransitionRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verifies health continues toward a cached target without another M5 dirty Cell. */
bool FAEM7ActiveRecoveryTest::RunTest(const FString& Parameters)
{
	FAEVegetationPatchStateService Service;
	TestTrue(TEXT("M7 service initializes"), Service.Initialize(2));
	UAEVegetationPatchComponent* Component = nullptr;
	FGuid PatchId;
	TestTrue(TEXT("Patch registers"), AdaptiveEnvM7Tests::RegisterTwoCellPatch(Service, Component, PatchId));
	TArray<FAEM5ConsumerCellView> Workset = {
		AdaptiveEnvM7Tests::MakeView(0, 1.0, 1, 1),
		AdaptiveEnvM7Tests::MakeView(1, 1.0, 1, 1)};
	Service.Update(Workset, {0, 1}, 2.0, 1);
	Workset[0] = AdaptiveEnvM7Tests::MakeView(0, 0.0, 2, 2);
	Workset[1] = AdaptiveEnvM7Tests::MakeView(1, 0.0, 2, 2);
	Service.Update(Workset, {0, 1}, 1.0, 2);
	FAEVegetationPatchSnapshot Before;
	Service.GetPatchSnapshot(PatchId, Before);

	TArray<int32> Required;
	Service.BuildRequiredCellIndices({}, Required);
	TestEqual(TEXT("Active Patch retains both Cell dependencies"), Required.Num(), 2);
	Service.Update(Workset, {}, 1.0, 3);
	FAEVegetationPatchSnapshot After;
	Service.GetPatchSnapshot(PatchId, After);
	TestTrue(TEXT("Recovery continues without M5 dirty input"), After.HealthRatio > Before.HealthRatio);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAEM7RevisionAndValidationTest,
	"AdaptiveEnv.M7.Contract.RevisionAndValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verifies stale M5 revisions and invalid duplicate Cell weights fail closed. */
bool FAEM7RevisionAndValidationTest::RunTest(const FString& Parameters)
{
	FAEVegetationPatchStateService Service;
	TestTrue(TEXT("M7 service initializes"), Service.Initialize(2));
	UAEVegetationPatchComponent* Component = nullptr;
	FGuid PatchId;
	TestTrue(TEXT("Patch registers"), AdaptiveEnvM7Tests::RegisterTwoCellPatch(Service, Component, PatchId));
	TArray<FAEM5ConsumerCellView> Workset = {
		AdaptiveEnvM7Tests::MakeView(0, 0.8, 2, 1),
		AdaptiveEnvM7Tests::MakeView(1, 0.8, 2, 1)};
	Service.Update(Workset, {0, 1}, 1.0, 1);
	const uint64 RevisionBefore = Service.GetPatchStateRevision();
	Workset[0].ResponseRevision = 1;
	Workset[1].ResponseRevision = 1;
	Service.Update(Workset, {0, 1}, 1.0, 2);
	TestEqual(TEXT("Stale input does not advance M7 revision"), Service.GetPatchStateRevision(), RevisionBefore);

	FAEVegetationPatchRegistration Invalid;
	Invalid.PatchId = FGuid(0xAE000007, 0, 0, 3);
	Invalid.SpeciesId = FGuid(0xAE000007, 0, 0, 4);
	Invalid.RegistrationGeneration = 1;
	Invalid.WeightedCells = {{0, 0.5}, {0, 0.5}};
	Invalid.Parameters = AdaptiveEnvM7Tests::MakeParameters();
	Invalid.Component = Component;
	TestFalse(TEXT("Duplicate Cell weights are rejected"), Service.RegisterPatch(Invalid));
	return true;
}

#endif
