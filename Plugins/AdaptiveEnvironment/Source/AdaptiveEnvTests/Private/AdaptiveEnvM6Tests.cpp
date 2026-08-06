#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AEPathHeatmapGrid.h"
#include "AEPathHeatmapRendererComponent.h"
#include "AdaptiveEnvSettings.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

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
		Input.SourceBehaviourRevision = ResponseRevision;
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

/* Verifies unsorted inputs emit stable row-major commands with bounded RGBA8 values. */
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
		TestEqual(TEXT("Zero Flow X uses the neutral code"), Commands[0].EncodedValue.R, static_cast<uint8>(128));
		TestEqual(TEXT("Zero Flow Y uses the neutral code"), Commands[0].EncodedValue.G, static_cast<uint8>(128));
		TestEqual(TEXT("Reserved channel remains zero"), Commands[0].EncodedValue.B, static_cast<uint8>(0));
		TestEqual(TEXT("Full path intensity quantizes to alpha 255"), Commands[0].EncodedValue.A, static_cast<uint8>(255));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAEM6FlowEncodingTest,
	"AdaptiveEnv.M6.Command.FlowEncoding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verifies M1 direction and consistency produce the documented signed RG encoding. */
bool FAEM6FlowEncodingTest::RunTest(const FString& Parameters)
{
	FAEPathHeatmapGrid Grid;
	TestTrue(TEXT("M6 Grid initializes"), AdaptiveEnvM6Tests::InitializeSingleCellGrid(Grid));
	FAEM6InputSnapshot Input = AdaptiveEnvM6Tests::MakeInput(0.6, 1, 1);
	Input.FlowDirection = FVector2D(1.0, 0.0);
	Input.FlowMagnitude = 0.5;
	TestTrue(TEXT("Flow update succeeds"), Grid.Update(
		{Input},
		4.0,
		AdaptiveEnvM6Tests::MakeParameters()));

	const TArray<FAEPathHeatmapVisualCommand>& Commands = Grid.GetVisualCommands();
	TestEqual(TEXT("One Flow command is emitted"), Commands.Num(), 1);
	if (Commands.Num() == 1)
	{
		TestEqual(TEXT("Positive half-strength X encodes to 191"), Commands[0].EncodedValue.R, static_cast<uint8>(191));
		TestEqual(TEXT("Zero Y retains the neutral code"), Commands[0].EncodedValue.G, static_cast<uint8>(128));
		TestEqual(TEXT("Path intensity remains in alpha"), Commands[0].EncodedValue.A, static_cast<uint8>(255));
	}

	FAEPathHeatmapSnapshot Snapshot;
	TestTrue(TEXT("Flow Cell is queryable"), Grid.GetCellSnapshot(FIntPoint::ZeroValue, Snapshot));
	TestTrue(TEXT("Snapshot retains weighted Flow"), Snapshot.FlowVector.Equals(FVector2D(0.5, 0.0), 1.0e-6));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAEM6MeshMaterialBindingTest,
	"AdaptiveEnv.M6.Renderer.MeshMaterialBinding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verifies one Mesh slot receives the shared M6 texture contract through an owned MID. */
bool FAEM6MeshMaterialBindingTest::RunTest(const FString& Parameters)
{
	const FName WorldName = MakeUniqueObjectName(
		GetTransientPackage(),
		UWorld::StaticClass(),
		TEXT("AE_M6_MeshMaterialWorld"));
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, WorldName, GetTransientPackage(), true);
	TestNotNull(TEXT("Temporary World"), World);
	if (World == nullptr)
	{
		return false;
	}

	UStaticMesh* PlaneMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	UMaterialInterface* PathMaterial = LoadObject<UMaterialInterface>(
		nullptr,
		TEXT("/Game/TopDown/Materials/M_Landscape.M_Landscape"));
	TestNotNull(TEXT("Engine plane Mesh"), PlaneMesh);
	TestNotNull(TEXT("Product M6 material"), PathMaterial);
	if (PlaneMesh == nullptr || PathMaterial == nullptr)
	{
		World->DestroyWorld(false);
		return false;
	}

	AActor* Owner = World->SpawnActor<AActor>();
	UStaticMeshComponent* MeshComponent = NewObject<UStaticMeshComponent>(Owner);
	Owner->AddInstanceComponent(MeshComponent);
	Owner->SetRootComponent(MeshComponent);
	MeshComponent->SetStaticMesh(PlaneMesh);
	MeshComponent->SetMaterial(0, PathMaterial);
	MeshComponent->RegisterComponent();

	UAEPathHeatmapRendererComponent* Renderer = NewObject<UAEPathHeatmapRendererComponent>(Owner);
	Owner->AddInstanceComponent(Renderer);
	Renderer->RegisterComponent();
	FAEPathMeshMaterialBinding& Binding = Renderer->TargetMeshMaterials.AddDefaulted_GetRef();
	Binding.MeshComponent = MeshComponent;
	Binding.MaterialSlotIndex = 0;
	World->UpdateWorldComponents(false, false);

	const FBox2D Bounds(FVector2D::ZeroVector, FVector2D(200.0, 400.0));
	TestTrue(TEXT("M6 Render Target initializes for a Mesh output"), Renderer->InitializeVisualOutput(FIntPoint(2, 4), Bounds));
	TestEqual(TEXT("One Mesh material output is bound"), Renderer->GetBoundMaterialOutputCount(), 1);
	const UAdaptiveEnvSettings* Settings = GetDefault<UAdaptiveEnvSettings>();
	UTextureRenderTarget2D* StateTarget = Renderer->GetPathStateRenderTarget();
	UTextureRenderTarget2D* VisualTarget = Renderer->GetPathHeatmapRenderTarget();
	TestNotNull(TEXT("M6 Cell-state Render Target"), StateTarget);
	TestNotNull(TEXT("M6 supersampled visual Render Target"), VisualTarget);
	if (StateTarget != nullptr && VisualTarget != nullptr)
	{
		const int32 PixelsPerCell = FMath::Clamp(Settings->M6VisualPixelsPerCell, 1, 8);
		TestNotEqual(TEXT("State and visual outputs use separate resources"), StateTarget, VisualTarget);
		TestEqual(TEXT("State width remains aligned to Grid Cells"), StateTarget->SizeX, 2);
		TestEqual(TEXT("State height remains aligned to Grid Cells"), StateTarget->SizeY, 4);
		TestEqual(TEXT("Visual width is supersampled"), VisualTarget->SizeX, 2 * PixelsPerCell);
		TestEqual(TEXT("Visual height is supersampled"), VisualTarget->SizeY, 4 * PixelsPerCell);
	}
	UMaterialInstanceDynamic* DynamicMaterial = Cast<UMaterialInstanceDynamic>(MeshComponent->GetMaterial(0));
	TestNotNull(TEXT("Mesh slot receives an MID"), DynamicMaterial);
	if (DynamicMaterial != nullptr)
	{
		TestTrue(
			TEXT("MID receives the shared M6 Render Target"),
			DynamicMaterial->K2_GetTextureParameterValue(Settings->M6PathTextureParameterName)
				== Renderer->GetPathHeatmapRenderTarget());
		const FLinearColor Transform = DynamicMaterial->K2_GetVectorParameterValue(
			Settings->M6GridTransformParameterName);
		TestTrue(TEXT("MID receives WorldMin and inverse WorldSize"), Transform.Equals(
			FLinearColor(0.0f, 0.0f, 1.0f / 200.0f, 1.0f / 400.0f),
			1.0e-6f));
		TestTrue(
			TEXT("MID sampling is enabled"),
			FMath::IsNearlyEqual(
				DynamicMaterial->K2_GetScalarParameterValue(Settings->M6EnabledParameterName),
				1.0f));

		Renderer->ResetVisualOutput();
		TestTrue(
			TEXT("Reset disables Mesh material sampling"),
			FMath::IsNearlyZero(
				DynamicMaterial->K2_GetScalarParameterValue(Settings->M6EnabledParameterName)));

		TestTrue(TEXT("Explicit refresh rebuilds configured outputs"), Renderer->RefreshMaterialBindings());
		UMaterialInstanceDynamic* RefreshedMaterial = Cast<UMaterialInstanceDynamic>(MeshComponent->GetMaterial(0));
		TestNotNull(TEXT("Refresh leaves one owned MID on the Mesh slot"), RefreshedMaterial);
		if (RefreshedMaterial != nullptr)
		{
			TestNotEqual(TEXT("Refresh does not stack on the previous MID"), RefreshedMaterial, DynamicMaterial);
			TestEqual(TEXT("Refreshed MID retains the original product material parent"), RefreshedMaterial->Parent.Get(), PathMaterial);
			TestTrue(
				TEXT("Refresh re-enables Mesh material sampling"),
				FMath::IsNearlyEqual(
					RefreshedMaterial->K2_GetScalarParameterValue(Settings->M6EnabledParameterName),
					1.0f));
		}
	}

	World->DestroyWorld(false);
	return true;
}

#endif
