#pragma once

#include "CoreMinimal.h"
#include "AEM6Types.generated.h"

namespace AdaptiveEnvM6
{
	/* Identifies supersampled Flow-capsule RG and Path Intensity A encoding. */
	inline constexpr uint32 PathHeatmapTextureEncodingVersion = 3;
}

/* Stores validated deterministic M6 path-visual parameters. */
struct ADAPTIVEENVRUNTIME_API FAEM6ParameterSet
{
	/* Starts visible path formation above this Damage ratio. */
	double VisibleDamageThresholdRatio = 0.15;
	/* Reaches the full path target at this Damage ratio. */
	double FullPathDamageThresholdRatio = 0.65;
	/* Advances path formation per simulation hour. */
	double FormationRatePerSimulationHour = 0.20;
	/* Advances path fading per simulation hour. */
	double FadeRatePerSimulationHour = 0.08;
	/* Suppresses state revisions below this intensity difference. */
	double DirtyIntensityEpsilon = 1.0 / 255.0;
};

/* Freezes committed M1 and M5 inputs for an M6 fixed-step update. */
struct ADAPTIVEENVRUNTIME_API FAEM6InputSnapshot
{
	/* Stores the shared integer XY Cell coordinate. */
	FIntPoint Coordinate = FIntPoint::ZeroValue;
	/* Stores the normalized M1 horizontal flow direction. */
	FVector2D FlowDirection = FVector2D::ZeroVector;
	/* Stores M1 directional consistency in the zero-to-one range. */
	double FlowMagnitude = 0.0;
	/* Identifies the latest M1 behaviour revision visible to this snapshot. */
	uint64 SourceBehaviourRevision = 0;
	/* Stores the committed M5 ecological Damage state. */
	double DamageRatio = 0.0;
	/* Identifies the consumed M5 response revision. */
	uint64 SourceResponseRevision = 0;
	/* Identifies the M5 step that last changed the ecological response. */
	uint64 SourceResponseSimulationStep = 0;
	/* Identifies the current World fixed step advancing M6. */
	uint64 CurrentSimulationStep = 0;
};

/* Stores deterministic M6 visual state for one shared Grid Cell. */
struct ADAPTIVEENVRUNTIME_API FAEPathHeatmapCellState
{
	/* Stores the M1-derived horizontal flow vector with directional consistency. */
	FVector2D FlowVector = FVector2D::ZeroVector;
	/* Stores the currently displayed path intensity in the zero-to-one range. */
	double PathIntensity = 0.0;
	/* Stores the M5-derived target path intensity in the zero-to-one range. */
	double TargetPathIntensity = 0.0;
	/* Identifies the latest M5 response revision consumed by this Cell. */
	uint64 SourceResponseRevision = 0;
	/* Identifies the latest M1 behaviour revision consumed by this Cell. */
	uint64 SourceBehaviourRevision = 0;
	/* Identifies the latest committed M6 visual-state revision. */
	uint64 PathVisualRevision = 0;
	/* Identifies the latest fixed M6 simulation step applied to this Cell. */
	uint64 SimulationStep = 0;
};

/* Exposes one immutable M6 path-visual snapshot. */
USTRUCT(BlueprintType)
struct ADAPTIVEENVRUNTIME_API FAEPathHeatmapSnapshot
{
	GENERATED_BODY()

	/* Stores the shared integer XY Cell coordinate. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M6")
	FIntPoint Coordinate = FIntPoint::ZeroValue;
	/* Stores the Cell centre in world centimetres. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M6")
	FVector WorldCenter = FVector::ZeroVector;
	/* Reports the current visual path intensity. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M6")
	float PathIntensity = 0.0f;
	/* Reports the M1-derived horizontal flow vector in the minus-one-to-one range. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M6")
	FVector2D FlowVector = FVector2D::ZeroVector;
	/* Reports the current M5-derived target intensity. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M6")
	float TargetPathIntensity = 0.0f;
	/* Identifies the consumed M5 response revision. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M6")
	int64 SourceResponseRevision = 0;
	/* Identifies the consumed M1 behaviour revision. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M6")
	int64 SourceBehaviourRevision = 0;
	/* Identifies the committed M6 visual revision. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M6")
	int64 PathVisualRevision = 0;
	/* Identifies the latest M6 fixed simulation step. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M6")
	int64 SimulationStep = 0;
};

/* Carries one immutable M6 Cell update to the visual application layer. */
struct ADAPTIVEENVRUNTIME_API FAEPathHeatmapVisualCommand
{
	/* Stores the row-major destination Cell index. */
	int32 CellIndex = INDEX_NONE;
	/* Stores the destination texture coordinate aligned with the shared Grid XY axes. */
	FIntPoint Coordinate = FIntPoint::ZeroValue;
	/* Stores encoded Flow XY in RG, reserved zero in B, and Path Intensity in A. */
	FColor EncodedValue = FColor(128, 128, 0, 0);
	/* Identifies the M6 revision represented by this command. */
	uint64 PathVisualRevision = 0;
};
