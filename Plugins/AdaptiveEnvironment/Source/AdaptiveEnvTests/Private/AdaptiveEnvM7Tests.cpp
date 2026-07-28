#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AEPlantBiomeMapAsset.h"
#include "AEPlantDistributionService.h"
#include "AEPlantSpeciesProfile.h"
#include "AEM7Types.h"

namespace AdaptiveEnvM7Tests
{
	/* Creates one deterministic test distribution contract. */
	FAEPlantDistributionConfig MakeConfig(const int32 Seed = 1337)
	{
		FAEPlantDistributionConfig Config;
		Config.WorldBounds = FBox2D(FVector2D(-500.0, -500.0), FVector2D(500.0, 500.0));
		Config.GridDimensions = FIntPoint(10, 10);
		Config.SpeciesId = TEXT("TestGrass");
		Config.Seed = Seed;
		Config.MinimumSpacingCm = 75.0f;
		Config.MaximumInstancesPerSquareMeter = 0.5f;
		Config.AttemptsPerExpectedPoint = 30;
		Config.MaximumCandidateCount = 1000;
		return Config;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAEM7DeterministicCandidatePoolTest,
	"AdaptiveEnv.M7.Distribution.DeterministicStablePool",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verifies identical structural inputs reproduce positions and identities. */
bool FAEM7DeterministicCandidatePoolTest::RunTest(const FString& Parameters)
{
	TArray<FAEM7CandidatePoint> First;
	TArray<FAEM7CandidatePoint> Second;
	FString Error;
	TestTrue(TEXT("First candidate pool builds"), FAEPlantDistributionService::GenerateStableCandidatePool(
		AdaptiveEnvM7Tests::MakeConfig(), First, Error));
	TestTrue(TEXT("Second candidate pool builds"), FAEPlantDistributionService::GenerateStableCandidatePool(
		AdaptiveEnvM7Tests::MakeConfig(), Second, Error));
	TestEqual(TEXT("Candidate counts match"), First.Num(), Second.Num());
	for (int32 Index = 0; Index < First.Num() && Index < Second.Num(); ++Index)
	{
		TestEqual(TEXT("Stable ids match"), First[Index].StablePointId, Second[Index].StablePointId);
		TestTrue(TEXT("Locations match"), First[Index].Location.Equals(Second[Index].Location, 0.0f));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAEM7PoissonHaloTest,
	"AdaptiveEnv.M7.Distribution.GlobalHaloSpacing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verifies global exclusion also prevents cross-Cell boundary seams. */
bool FAEM7PoissonHaloTest::RunTest(const FString& Parameters)
{
	const FAEPlantDistributionConfig Config = AdaptiveEnvM7Tests::MakeConfig();
	TArray<FAEM7CandidatePoint> Points;
	FString Error;
	TestTrue(TEXT("Candidate pool builds"), FAEPlantDistributionService::GenerateStableCandidatePool(Config, Points, Error));
	for (int32 A = 0; A < Points.Num(); ++A)
	{
		for (int32 B = A + 1; B < Points.Num(); ++B)
		{
			TestTrue(
				TEXT("Global spacing also covers adjacent-Cell halo"),
				FVector::DistSquared2D(Points[A].Location, Points[B].Location)
					>= FMath::Square(static_cast<double>(Config.MinimumSpacingCm)));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAEM7SeedAndStableSelectionTest,
	"AdaptiveEnv.M7.Distribution.SeedAndSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verifies seed differentiation and normalized stable selection keys. */
bool FAEM7SeedAndStableSelectionTest::RunTest(const FString& Parameters)
{
	TArray<FAEM7CandidatePoint> First;
	TArray<FAEM7CandidatePoint> OtherSeed;
	FString Error;
	FAEPlantDistributionService::GenerateStableCandidatePool(AdaptiveEnvM7Tests::MakeConfig(), First, Error);
	FAEPlantDistributionService::GenerateStableCandidatePool(AdaptiveEnvM7Tests::MakeConfig(2026), OtherSeed, Error);
	TestTrue(TEXT("Different seed changes the pool"), First.Num() > 0 && OtherSeed.Num() > 0
		&& First[0].StablePointId != OtherSeed[0].StablePointId);
	for (const FAEM7CandidatePoint& Point : First)
	{
		TestTrue(TEXT("Selection key remains normalized"), Point.SelectionKey >= 0.0f && Point.SelectionKey < 1.0f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAEM7BiomeMapSamplingTest,
	"AdaptiveEnv.M7.BiomeMap.BilinearSampling",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verifies species biome weights use bilinear world-space sampling. */
bool FAEM7BiomeMapSamplingTest::RunTest(const FString& Parameters)
{
	UAEPlantBiomeMapAsset* Map = NewObject<UAEPlantBiomeMapAsset>();
	Map->Dimensions = FIntPoint(2, 2);
	Map->WorldMin = FVector2D::ZeroVector;
	Map->WorldMax = FVector2D(100.0, 100.0);
	Map->Weights = {0.0f, 1.0f, 1.0f, 0.0f};
	TestTrue(TEXT("Centre bilinear sample is one half"), FMath::IsNearlyEqual(
		Map->SampleWeight(FVector(50.0, 50.0, 0.0)), 0.5f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAEM7FirstEvaluationHealthInputTest,
	"AdaptiveEnv.M7.Lifecycle.FirstEvaluationInitializesTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verifies first effective input, including intact baseline, initializes without a recovery ramp. */
bool FAEM7FirstEvaluationHealthInputTest::RunTest(const FString& Parameters)
{
	const float BaselineHealth = FAEM7LifecycleModel::ResolveHealthRatio(
		0.0f,
		1.0f,
		false,
		0.01,
		0.5f,
		0.25f);
	TestTrue(TEXT("Unsampled intact baseline starts healthy"), FMath::IsNearlyEqual(BaselineHealth, 1.0f));

	const float InitializedHealth = FAEM7LifecycleModel::ResolveHealthRatio(
		0.0f,
		0.72f,
		false,
		0.01,
		0.5f,
		0.25f);
	TestTrue(TEXT("First valid input adopts target health"), FMath::IsNearlyEqual(InitializedHealth, 0.72f));
	TestEqual(
		TEXT("First non-terminal target starts stable"),
		FAEM7LifecycleModel::ResolveInitialState(InitializedHealth, 0.01f),
		EAEPlantLifecycleState::Stable);
	TestEqual(
		TEXT("First terminal target starts dead"),
		FAEM7LifecycleModel::ResolveInitialState(0.01f, 0.01f),
		EAEPlantLifecycleState::Dead);

	const float RecoveredHealth = FAEM7LifecycleModel::ResolveHealthRatio(
		0.2f,
		0.8f,
		true,
		1.0,
		0.5f,
		0.25f);
	TestTrue(TEXT("Later input uses bounded recovery rate"), FMath::IsNearlyEqual(RecoveredHealth, 0.45f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAEM7SpeciesCollisionDefaultsTest,
	"AdaptiveEnv.M7.Profile.CollisionDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verifies new species default to non-blocking, non-overlapping, navigation-neutral HISM behavior. */
bool FAEM7SpeciesCollisionDefaultsTest::RunTest(const FString& Parameters)
{
	const UAEPlantSpeciesProfile* Profile = NewObject<UAEPlantSpeciesProfile>();
	TestEqual(TEXT("Collision defaults to disabled"), Profile->CollisionEnabled.GetValue(), ECollisionEnabled::NoCollision);
	TestEqual(TEXT("Collision profile defaults to NoCollision"), Profile->CollisionProfileName, FName(TEXT("NoCollision")));
	TestFalse(TEXT("Overlap events default off"), Profile->bGenerateOverlapEvents);
	TestFalse(TEXT("Navigation effect defaults off"), Profile->bCanEverAffectNavigation);
	TestEqual(TEXT("New profile semantic version"), Profile->SemanticVersion, FString(TEXT("1.1.0")));
	return true;
}

#endif
