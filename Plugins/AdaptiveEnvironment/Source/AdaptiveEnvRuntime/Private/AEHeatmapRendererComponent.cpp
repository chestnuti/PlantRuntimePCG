#include "AEHeatmapRendererComponent.h"

#include "AdaptiveEnvSettings.h"
#include "AdaptiveEnvWorldSubsystem.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"

namespace AEHeatmapRendererPrivate
{
	constexpr int32 SafeDebugTextLabelCap = 96;

	/* Stores one renderer-independent Cell value ready for geometry and label drawing. */
	struct FCellDrawData
	{
		FIntPoint Coordinate = FIntPoint::ZeroValue;
		FVector WorldCenter = FVector::ZeroVector;
		FVector2D FlowDirection = FVector2D::ZeroVector;
		float Value = 0.0f;
		FColor Color = FColor::White;
	};
}

// Disable component Tick because the World Subsystem controls debug refreshes.
UAEHeatmapRendererComponent::UAEHeatmapRendererComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

// Register this renderer with the current World Subsystem.
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

// Unregister this renderer before component teardown.
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

// Draw the selected metric for nearby non-empty behaviour cells.
void UAEHeatmapRendererComponent::RenderDebug(const UAEAdaptiveEnvWorldSubsystem& Subsystem, const float DurationSeconds) const
{
#if ENABLE_DRAW_DEBUG
	// Skip drawing when visualization is disabled or the World is unavailable.
	if (Mode == EAEHeatmapDebugMode::None || GetWorld() == nullptr)
	{
		return;
	}

	// Derive shared debug geometry and parameter-aware colour normalization.
	const UAdaptiveEnvSettings* Settings = GetDefault<UAdaptiveEnvSettings>();
	const FVector Origin = GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
	const FVector2D GridSize = Subsystem.GetGridWorldBounds().GetSize();
	const FIntPoint Dimensions = Subsystem.GetGridDimensions();
	const float CellSize = Dimensions.X > 0 ? static_cast<float>(GridSize.X / Dimensions.X) : 100.0f;
	const FVector Extent(CellSize * 0.48f, CellSize * 0.48f, 3.0f);
	const float ModeMaximum = bUseModeDefaultRange
		? (Mode == EAEHeatmapDebugMode::EnvironmentState ? 3.0f
			: (IsM3Mode() ? Subsystem.GetM3DebugMaximumValue(Mode)
				: ((IsM4Mode() || IsM5Mode()) ? 1.0f : 0.0f)))
		: 0.0f;
	const float DisplayMaximum = FMath::Max(
		ModeMaximum > 0.0f ? ModeMaximum : FixedMaximumValue,
		UE_SMALL_NUMBER);
	TArray<AEHeatmapRendererPrivate::FCellDrawData> DrawCells;

	// Query the selected data layer and convert immutable snapshots into common draw data.
	if (IsM5Mode())
	{
		TArray<FAEEcologicalResponseSnapshot> Cells;
		Subsystem.GetM5DebugCells(Origin, Settings->DebugDrawRadiusCm, Settings->MaxDebugCells, Cells);
		DrawCells.Reserve(Cells.Num());
		for (const FAEEcologicalResponseSnapshot& Cell : Cells)
		{
			const float Value = GetM5DisplayValue(Cell);
			if (!FMath::IsFinite(Value) || Value < MinimumValue) continue;
			const float Alpha = FMath::Clamp(Value / DisplayMaximum, 0.0f, 1.0f);
			DrawCells.Add({Cell.Coordinate, Cell.WorldCenter, FVector2D::ZeroVector, Value, FLinearColor::LerpUsingHSV(FLinearColor::Blue, FLinearColor::Red, Alpha).ToFColor(true)});
		}
	}
	else if (IsM4Mode())
	{
		TArray<FAEEnvironmentConstraintSnapshot> Cells;
		Subsystem.GetM4DebugCells(Origin, Settings->DebugDrawRadiusCm, Settings->MaxDebugCells, Cells);
		DrawCells.Reserve(Cells.Num());
		for (const FAEEnvironmentConstraintSnapshot& Cell : Cells)
		{
			const float Value = GetM4DisplayValue(Cell);
			if (!FMath::IsFinite(Value) || Value < MinimumValue) continue;
			const float Alpha = FMath::Clamp(Value / DisplayMaximum, 0.0f, 1.0f);
			DrawCells.Add({Cell.Coordinate, Cell.WorldCenter, FVector2D::ZeroVector, Value, FLinearColor::LerpUsingHSV(FLinearColor::Blue, FLinearColor::Red, Alpha).ToFColor(true)});
		}
	}
	else if (IsM3Mode())
	{
		TArray<FAEM3CellSnapshot> Cells;
		Subsystem.GetM3DebugCells(Origin, Settings->DebugDrawRadiusCm, Settings->MaxDebugCells, Cells);
		DrawCells.Reserve(Cells.Num());
		for (const FAEM3CellSnapshot& Cell : Cells)
		{
			const float Value = GetM3DisplayValue(Cell);
			if (!FMath::IsFinite(Value) || Value < MinimumValue)
			{
				continue;
			}
			const float Alpha = FMath::Clamp(Value / DisplayMaximum, 0.0f, 1.0f);
			DrawCells.Add({Cell.Coordinate, Cell.WorldCenter, FVector2D::ZeroVector, Value,
				FLinearColor::LerpUsingHSV(FLinearColor::Blue, FLinearColor::Red, Alpha).ToFColor(true)});
		}
	}
	else
	{
		TArray<FAEBehaviourCellSnapshot> Cells;
		Subsystem.GetDebugCells(Origin, Settings->DebugDrawRadiusCm, Settings->MaxDebugCells, Cells);
		DrawCells.Reserve(Cells.Num());
		for (const FAEBehaviourCellSnapshot& Cell : Cells)
		{
			const float Value = GetBehaviourDisplayValue(Cell);
			if (!FMath::IsFinite(Value) || Value < MinimumValue)
			{
				continue;
			}
			const float Alpha = FMath::Clamp(Value / DisplayMaximum, 0.0f, 1.0f);
			DrawCells.Add({Cell.Coordinate, Cell.WorldCenter, Cell.FlowDirection, Value,
				FLinearColor::LerpUsingHSV(FLinearColor::Blue, FLinearColor::Red, Alpha).ToFColor(true)});
		}
	}

	// Draw geometry for every selected Cell independently from the smaller text budget.
	for (const AEHeatmapRendererPrivate::FCellDrawData& Cell : DrawCells)
	{
		const FVector Center = Cell.WorldCenter + FVector(0.0, 0.0, DrawHeightCm);
		DrawDebugBox(GetWorld(), Center, Extent, Cell.Color, false, DurationSeconds * 1.1f, 0, 1.0f);
		if (Mode == EAEHeatmapDebugMode::Flow && !Cell.FlowDirection.IsNearlyZero())
		{
			const FVector Direction(Cell.FlowDirection.X, Cell.FlowDirection.Y, 0.0);
			DrawDebugDirectionalArrow(GetWorld(), Center, Center + Direction * CellSize * 0.4f, 20.0f, Cell.Color, false, DurationSeconds * 1.1f, 0, 2.0f);
		}
	}

	// Keep labels below Unreal's shared 128-string WorldSettings cap and prefer nearby camera values.
	if (bDrawValues && Settings->MaxDebugTextLabels > 0)
	{
		FVector CameraLocation = Origin;
		FRotator CameraRotation = FRotator::ZeroRotator;
		if (APlayerController* PlayerController = GetWorld()->GetFirstPlayerController())
		{
			PlayerController->GetPlayerViewPoint(CameraLocation, CameraRotation);
		}
		DrawCells.Sort([CameraLocation](const AEHeatmapRendererPrivate::FCellDrawData& A, const AEHeatmapRendererPrivate::FCellDrawData& B)
		{
			const double DistanceA = FVector::DistSquared(CameraLocation, A.WorldCenter);
			const double DistanceB = FVector::DistSquared(CameraLocation, B.WorldCenter);
			return DistanceA < DistanceB
				|| (FMath::IsNearlyEqual(DistanceA, DistanceB)
					&& (A.Coordinate.Y < B.Coordinate.Y
						|| (A.Coordinate.Y == B.Coordinate.Y && A.Coordinate.X < B.Coordinate.X)));
		});
		const int32 LabelBudget = FMath::Clamp(Settings->MaxDebugTextLabels, 0, AEHeatmapRendererPrivate::SafeDebugTextLabelCap);
		const int32 LabelCount = FMath::Min(DrawCells.Num(), LabelBudget);
		const float TextDurationSeconds = FMath::Max(DurationSeconds * 0.9f, 0.01f);
		AActor* TextBaseActor = GetOwner();
		for (int32 LabelIndex = 0; LabelIndex < LabelCount; ++LabelIndex)
		{
			const AEHeatmapRendererPrivate::FCellDrawData& Cell = DrawCells[LabelIndex];
			const FVector TextLocation = Cell.WorldCenter + FVector(0.0, 0.0, DrawHeightCm + 10.0f);
			const FVector TextOffset = TextBaseActor != nullptr ? TextLocation - TextBaseActor->GetActorLocation() : TextLocation;
			DrawDebugString(GetWorld(), TextOffset, FString::Printf(TEXT("%.2f"), Cell.Value), TextBaseActor, Cell.Color, TextDurationSeconds, false);
		}
	}
#else
	// Suppress unused parameter warnings in builds without debug drawing.
	(void)Subsystem;
	(void)DurationSeconds;
#endif
}

/* Return whether the selected layer is derived from M3 state. */
bool UAEHeatmapRendererComponent::IsM3Mode() const
{
	return Mode >= EAEHeatmapDebugMode::PassExposure && Mode <= EAEHeatmapDebugMode::CurrentExposure;
}

/* Return whether the selected layer is derived from M4 state. */
bool UAEHeatmapRendererComponent::IsM4Mode() const
{
	return Mode >= EAEHeatmapDebugMode::ConstraintPressure && Mode <= EAEHeatmapDebugMode::EnvironmentState;
}

/* Return whether the selected layer is derived from M5 state. */
bool UAEHeatmapRendererComponent::IsM5Mode() const
{
	return Mode >= EAEHeatmapDebugMode::EffectiveImpact;
}

/* Map the current M1 debug mode to one raw behavior metric. */
float UAEHeatmapRendererComponent::GetBehaviourDisplayValue(const FAEBehaviourCellSnapshot& Snapshot) const
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

/* Map the current M3 debug mode to one Exposure or ecological response metric. */
float UAEHeatmapRendererComponent::GetM3DisplayValue(const FAEM3CellSnapshot& Snapshot) const
{
	switch (Mode)
	{
	case EAEHeatmapDebugMode::PassExposure:
		return Snapshot.PassExposure;
	case EAEHeatmapDebugMode::TravelExposure:
		return Snapshot.TravelExposure;
	case EAEHeatmapDebugMode::DwellExposure:
		return Snapshot.DwellExposure;
	case EAEHeatmapDebugMode::SprintExposure:
		return Snapshot.SprintExposure;
	case EAEHeatmapDebugMode::CollectExposure:
		return Snapshot.CollectExposure;
	case EAEHeatmapDebugMode::CombatExposure:
		return Snapshot.CombatExposure;
	case EAEHeatmapDebugMode::CurrentExposure:
		return Snapshot.CurrentExposure;
	default:
		return 0.0f;
	}
}

/* Map the current M4 debug mode to one committed constraint metric. */
float UAEHeatmapRendererComponent::GetM4DisplayValue(const FAEEnvironmentConstraintSnapshot& Snapshot) const
{
	switch (Mode)
	{
	case EAEHeatmapDebugMode::ConstraintPressure: return Snapshot.ConstraintPressureRatio;
	case EAEHeatmapDebugMode::HabitatSuitability: return Snapshot.HabitatSuitabilityRatio;
	case EAEHeatmapDebugMode::EnvironmentState: return static_cast<float>(Snapshot.State);
	default: return 0.0f;
	}
}

/* Map the current M5 debug mode to one committed ecological response metric. */
float UAEHeatmapRendererComponent::GetM5DisplayValue(const FAEEcologicalResponseSnapshot& Snapshot) const
{
	switch (Mode)
	{
	case EAEHeatmapDebugMode::EffectiveImpact: return Snapshot.EffectiveImpactRatio;
	case EAEHeatmapDebugMode::Damage: return Snapshot.DamageRatio;
	case EAEHeatmapDebugMode::Recovery: return Snapshot.RecoveryRatio;
	case EAEHeatmapDebugMode::DamageRate: return Snapshot.DamageRatePerSimulationHour;
	case EAEHeatmapDebugMode::RecoveryRate: return Snapshot.RecoveryRatePerSimulationHour;
	default: return 0.0f;
	}
}
