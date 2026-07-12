#include "AEHeatmapRendererComponent.h"

#include "AdaptiveEnvSettings.h"
#include "AdaptiveEnvWorldSubsystem.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Actor.h"

UAEHeatmapRendererComponent::UAEHeatmapRendererComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UAEHeatmapRendererComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UWorld* World = GetWorld())
	{
		if (UAEAdaptiveEnvWorldSubsystem* Subsystem = World->GetSubsystem<UAEAdaptiveEnvWorldSubsystem>())
		{
			Subsystem->RegisterHeatmapRenderer(this);
		}
	}
}

void UAEHeatmapRendererComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UAEAdaptiveEnvWorldSubsystem* Subsystem = World->GetSubsystem<UAEAdaptiveEnvWorldSubsystem>())
		{
			Subsystem->UnregisterHeatmapRenderer(this);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void UAEHeatmapRendererComponent::RenderDebug(const UAEAdaptiveEnvWorldSubsystem& Subsystem, const float DurationSeconds) const
{
#if ENABLE_DRAW_DEBUG
	if (Mode == EAEHeatmapDebugMode::None || GetWorld() == nullptr)
	{
		return;
	}

	const UAdaptiveEnvSettings* Settings = GetDefault<UAdaptiveEnvSettings>();
	const FVector Origin = GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
	TArray<FAEBehaviourCellSnapshot> Cells;
	Subsystem.GetDebugCells(Origin, Settings->DebugDrawRadiusCm, Settings->MaxDebugCells, Cells);
	const FVector2D GridSize = Subsystem.GetGridWorldBounds().GetSize();
	const FIntPoint Dimensions = Subsystem.GetGridDimensions();
	const float CellSize = Dimensions.X > 0 ? static_cast<float>(GridSize.X / Dimensions.X) : 100.0f;
	const FVector Extent(CellSize * 0.48f, CellSize * 0.48f, 3.0f);

	for (const FAEBehaviourCellSnapshot& Cell : Cells)
	{
		const float Value = GetDisplayValue(Cell);
		if (Value < MinimumValue)
		{
			continue;
		}

		const float Alpha = FMath::Clamp(Value / FMath::Max(FixedMaximumValue, UE_SMALL_NUMBER), 0.0f, 1.0f);
		const FColor Color = FLinearColor::LerpUsingHSV(FLinearColor::Blue, FLinearColor::Red, Alpha).ToFColor(true);
		const FVector Center = Cell.WorldCenter + FVector(0.0, 0.0, DrawHeightCm);
		DrawDebugBox(GetWorld(), Center, Extent, Color, false, DurationSeconds * 1.1f, 0, 1.0f);

		if (Mode == EAEHeatmapDebugMode::Flow && !Cell.FlowDirection.IsNearlyZero())
		{
			const FVector Direction(Cell.FlowDirection.X, Cell.FlowDirection.Y, 0.0);
			DrawDebugDirectionalArrow(GetWorld(), Center, Center + Direction * CellSize * 0.4f, 20.0f, Color, false, DurationSeconds * 1.1f, 0, 2.0f);
		}
		if (bDrawValues)
		{
			DrawDebugString(GetWorld(), Center + FVector(0.0, 0.0, 10.0f), FString::Printf(TEXT("%.2f"), Value), nullptr, Color, DurationSeconds * 1.1f, false);
		}
	}
#else
	(void)Subsystem;
	(void)DurationSeconds;
#endif
}

float UAEHeatmapRendererComponent::GetDisplayValue(const FAEBehaviourCellSnapshot& Snapshot) const
{
	switch (Mode)
	{
	case EAEHeatmapDebugMode::PassCount:
		return Snapshot.PassCount;
	case EAEHeatmapDebugMode::TravelDistance:
		return Snapshot.TravelDistanceMeters;
	case EAEHeatmapDebugMode::DwellTime:
		return Snapshot.DwellSeconds;
	case EAEHeatmapDebugMode::SmoothedActivity:
		return Snapshot.SmoothedActivity;
	case EAEHeatmapDebugMode::Flow:
		return Snapshot.FlowMagnitude;
	default:
		return 0.0f;
	}
}
