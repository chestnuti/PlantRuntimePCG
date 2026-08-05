#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AEPlantBiomeMapAsset.h"
#include "AEPlantDistributionService.h"
#include "AEPlantSpeciesProfile.h"
#include "AEWorldScalarFieldAsset.h"
#include "AEWorldConstraintProvider.h"
#include "AEM7Types.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

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
	"AdaptiveEnv.M7.BiomeMap.RangeAndGradient",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verifies named biome ranges preserve a plateau and smooth both outer edges. */
bool FAEM7BiomeMapSamplingTest::RunTest(const FString& Parameters)
{
	UAEBiomeTextureAsset* Field = NewObject<UAEBiomeTextureAsset>();
	Field->BakedDimensions = FIntPoint(2, 1);
	Field->WorldMin = FVector2D::ZeroVector;
	Field->WorldMax = FVector2D(100.0, 100.0);
	Field->bFlipVerticalAxis = false;
	Field->BakedSamples = {0, 65535};
	UAEPlantBiomeMapAsset* Map = NewObject<UAEPlantBiomeMapAsset>();
	Map->BiomeTexture = Field;
	FAEBiomeRangeDefinition& Biome = Map->Biomes.AddDefaulted_GetRef();
	Biome.BiomeId = TEXT("Forest");
	Biome.LowerBoundRatio = 0.4f;
	Biome.UpperBoundRatio = 0.6f;
	Biome.GradientWidthRatio = 0.2f;
	FString Error;
	TestTrue(TEXT("Biome map validates"), Map->IsValidMap(Error));
	TestTrue(TEXT("Core interval has full density"), FMath::IsNearlyEqual(
		Map->SampleBiomeWeight(TEXT("Forest"), FVector(50.0, 50.0, 0.0)), 1.0f, 1.0e-4f));
	TestTrue(TEXT("Lower gradient is smooth"), FMath::IsNearlyEqual(
		UAEPlantBiomeMapAsset::EvaluateBiomeWeight(0.3f, Biome), 0.5f, 1.0e-4f));
	Biome.GradientWidthRatio = 0.0f;
	TestTrue(TEXT("Zero gradient uses a hard boundary"), FMath::IsNearlyZero(
		UAEPlantBiomeMapAsset::EvaluateBiomeWeight(0.3f, Biome)));
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
	FAEM7OccupiedCellBaselineTest,
	"AdaptiveEnv.M7.Baseline.OccupiedCellsOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verifies M7 baseline requests include every occupied Cell once without expanding to the full Grid. */
bool FAEM7OccupiedCellBaselineTest::RunTest(const FString& Parameters)
{
	// Arrange duplicate candidate coverage and one invalid coordinate.
	TArray<FAEM7CandidatePoint> Candidates;
	Candidates.AddDefaulted(4);
	Candidates[0].CellCoordinate = FIntPoint(2, 1);
	Candidates[1].CellCoordinate = FIntPoint(0, 0);
	Candidates[2].CellCoordinate = FIntPoint(2, 1);
	Candidates[3].CellCoordinate = FIntPoint(-1, 0);

	// Collect stable row-major indices for a four-by-three Grid.
	TArray<int32> OccupiedCellIndices;
	FAEPlantDistributionService::CollectOccupiedCellIndices(
		Candidates,
		FIntPoint(4, 3),
		OccupiedCellIndices);

	// Assert unique ascending coverage and exclusion of unused or invalid Cells.
	TestEqual(TEXT("Only two valid occupied Cells are requested"), OccupiedCellIndices.Num(), 2);
	if (OccupiedCellIndices.Num() == 2)
	{
		TestEqual(TEXT("First occupied Cell is row-major zero"), OccupiedCellIndices[0], 0);
		TestEqual(TEXT("Second occupied Cell is row-major six"), OccupiedCellIndices[1], 6);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAEM7VisibilityHysteresisTest,
	"AdaptiveEnv.M7.Lifecycle.VisibilityHysteresis",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verifies structural eligibility and existing health thresholds produce stable hide/reappear behavior. */
bool FAEM7VisibilityHysteresisTest::RunTest(const FString& Parameters)
{
	// Structural rejection always wins regardless of health.
	TestFalse(
		TEXT("Structurally ineligible candidate stays hidden"),
		FAEM7LifecycleModel::ResolveVisibility(false, true, true, 1.0f, 0.1f, 0.1f));

	// A visible plant remains until health reaches the removal threshold.
	TestTrue(
		TEXT("Visible candidate remains during decline above removal health"),
		FAEM7LifecycleModel::ResolveVisibility(true, true, true, 0.11f, 0.1f, 0.1f));
	TestFalse(
		TEXT("Visible candidate hides at removal health"),
		FAEM7LifecycleModel::ResolveVisibility(true, true, true, 0.1f, 0.1f, 0.1f));

	// A hidden plant stays hidden inside the dead band and returns at the upper threshold.
	TestFalse(
		TEXT("Hidden candidate remains hidden inside hysteresis band"),
		FAEM7LifecycleModel::ResolveVisibility(true, false, true, 0.19f, 0.1f, 0.1f));
	TestTrue(
		TEXT("Hidden candidate reappears at recovery threshold"),
		FAEM7LifecycleModel::ResolveVisibility(true, false, true, 0.2f, 0.1f, 0.1f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAEM7GroundProjectionTraceTest,
	"AdaptiveEnv.M7.GroundProjection.ApprovedSurface",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verifies M7 ground projection skips unauthorized blockers and resolves approved surface height. */
bool FAEM7GroundProjectionTraceTest::RunTest(const FString& Parameters)
{
	// Create an isolated physics World with one unauthorized blocker above tagged ground.
	const FName WorldName = MakeUniqueObjectName(
		GetTransientPackage(),
		UWorld::StaticClass(),
		TEXT("AE_M7_GroundProjectionWorld"));
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, WorldName, GetTransientPackage(), true);
	TestNotNull(TEXT("Temporary World"), World);
	if (World == nullptr)
	{
		return false;
	}

	AActor* GroundActor = World->SpawnActor<AActor>();
	UBoxComponent* Ground = NewObject<UBoxComponent>(GroundActor);
	GroundActor->SetRootComponent(Ground);
	Ground->InitBoxExtent(FVector(200.0, 200.0, 50.0));
	Ground->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Ground->SetCollisionObjectType(ECC_WorldStatic);
	Ground->SetCollisionResponseToAllChannels(ECR_Block);
	Ground->ComponentTags.Add(FAEWorldConstraintProvider::EnvironmentGroundTag);
	Ground->RegisterComponent();
	GroundActor->SetActorLocation(FVector(0.0, 0.0, 200.0));

	AActor* BlockerActor = World->SpawnActor<AActor>();
	UBoxComponent* Blocker = NewObject<UBoxComponent>(BlockerActor);
	BlockerActor->SetRootComponent(Blocker);
	Blocker->InitBoxExtent(FVector(100.0, 100.0, 50.0));
	Blocker->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Blocker->SetCollisionObjectType(ECC_WorldDynamic);
	Blocker->SetCollisionResponseToAllChannels(ECR_Block);
	Blocker->RegisterComponent();
	BlockerActor->SetActorLocation(FVector(0.0, 0.0, 400.0));
	World->UpdateWorldComponents(false, false);

	// Resolve the tagged lower surface after the untagged upper blocker is rejected.
	FAEGroundSurfaceSample Surface;
	TestTrue(
		TEXT("Approved ground is found below an unauthorized blocker"),
		FAEWorldConstraintProvider::TraceGroundSurface(
			*World,
			FVector::ZeroVector,
			1000.0f,
			nullptr,
			Surface));
	TestTrue(TEXT("Projected height matches the tagged box top"), FMath::IsNearlyEqual(Surface.WorldLocation.Z, 250.0, 0.1));
	TestTrue(TEXT("Projected normal is world up"), Surface.WorldNormal.Equals(FVector::UpVector, 1.0e-6f));

	// Ignore the only approved ground owner and verify no fallback plane is fabricated.
	FAEGroundSurfaceSample IgnoredSurface;
	TestFalse(
		TEXT("Ignored approved owner leaves no valid ground"),
		FAEWorldConstraintProvider::TraceGroundSurface(
			*World,
			FVector::ZeroVector,
			1000.0f,
			GroundActor,
			IgnoredSurface));

	// Destroy the transient World after collision assertions complete.
	World->DestroyWorld(false);
	World->RemoveFromRoot();
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
	TestTrue(TEXT("Ground offset defaults to zero"), FMath::IsNearlyZero(Profile->GroundOffsetCm));
	TestFalse(TEXT("Ground normal alignment defaults off"), Profile->bAlignToGroundNormal);
	TestTrue(TEXT("Dead health defaults to ten percent"), FMath::IsNearlyEqual(Profile->DeadHealthThreshold, 0.1f));
	TestTrue(TEXT("Visibility hysteresis defaults to ten percent"), FMath::IsNearlyEqual(Profile->StateEpsilon, 0.1f));
	TestEqual(TEXT("New profile semantic version"), Profile->SemanticVersion, FString(TEXT("1.3.0")));
	return true;
}

#endif
