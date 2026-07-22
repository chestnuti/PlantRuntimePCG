#pragma once

#include "CoreMinimal.h"
#include "AEParameterBundleTypes.h"
#include "AEM5Types.generated.h"

/* Freezes one version-compatible M3/M4 input pair for a single M5 Cell step. */
struct ADAPTIVEENVRUNTIME_API FAEM5InputSnapshot
{
	/* Stores the row-major Cell coordinate shared by all input layers. */
	FIntPoint Coordinate = FIntPoint::ZeroValue;
	/* Stores accumulated M3 Exposure before normalization. */
	double Exposure = 0.0;
	/* Stores the positive M3 Exposure normalization maximum. */
	double ExposureMaximum = 1.0;
	/* Stores committed M4 environmental pressure in the zero-to-one range. */
	double ConstraintPressureRatio = 0.0;
	/* Stores committed M4 habitat suitability in the zero-to-one range. */
	double HabitatSuitabilityRatio = 1.0;
	/* Identifies the consumed M3 Cell revision. */
	uint64 ExposureRevision = 0;
	/* Identifies the consumed M4 Cell revision. */
	uint64 ConstraintRevision = 0;
	/* Identifies the shared fixed simulation step. */
	uint64 SimulationStep = 0;
	/* Identifies the atomic parameter bundle used for the calculation. */
	FAEParameterBundleIdentity BundleIdentity;
};

/* Stores the M5 impact-fusion parameters. */
struct ADAPTIVEENVRUNTIME_API FAEImpactFusionParameters
{
	/* Scales environment pressure before it amplifies normalized Exposure. */
	double ConstraintSensitivity = 0.0;
};

/* Stores the piecewise-linear Damage response. */
struct ADAPTIVEENVRUNTIME_API FAEDamageResponseParameters
{
	/* Starts Damage accumulation once effective impact reaches this ratio. */
	double ActivationImpact = 0.5;
	/* Reaches the configured maximum Damage rate at this impact ratio. */
	double SaturationImpact = 1.0;
	/* Caps Damage accumulation per simulation hour. */
	double MaximumRatePerSimulationHour = 0.0;
};

/* Stores delayed Recovery parameters. */
struct ADAPTIVEENVRUNTIME_API FAERecoveryResponseParameters
{
	/* Allows Recovery only while normalized Exposure stays below this ratio. */
	double ActivationExposure = 0.25;
	/* Requires continuous low Exposure for this many simulation hours. */
	double DelaySimulationHours = 0.0;
	/* Defines the Recovery rate before habitat-suitability scaling. */
	double BaseRatePerSimulationHour = 0.0;
};

/* Stores validated M5 values. */
struct ADAPTIVEENVRUNTIME_API FAEM5ParameterSet
{
	/* Controls how M4 pressure changes M3 Exposure impact. */
	FAEImpactFusionParameters Fusion;
	/* Controls Damage activation, saturation, and integration rate. */
	FAEDamageResponseParameters Damage;
	/* Controls low-Exposure Recovery activation and rate. */
	FAERecoveryResponseParameters Recovery;
};

/* Stores mutable response state for one Cell. */
struct ADAPTIVEENVRUNTIME_API FAEM5StateMemory
{
	/* Stores the bounded ecological Damage state in the zero-to-one range. */
	double DamageRatio = 0.0;
	/* Accumulates uninterrupted time below the Recovery Exposure threshold. */
	double LowExposureDurationSimulationHours = 0.0;
};

/* Exposes one immutable M5 ecological response. */
USTRUCT(BlueprintType)
struct ADAPTIVEENVRUNTIME_API FAEEcologicalResponseSnapshot
{
	GENERATED_BODY()
	/* Stores the integer XY Cell coordinate. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M5") FIntPoint Coordinate = FIntPoint::ZeroValue;
	/* Stores the Cell centre in world centimetres. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M5") FVector WorldCenter = FVector::ZeroVector;
	/* Reports normalized Exposure after M4 environment-pressure amplification. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M5") float EffectiveImpactRatio = 0.0f;
	/* Reports the integrated ecological Damage state. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M5") float DamageRatio = 0.0f;
	/* Reports the intact ecological proportion derived from Damage. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M5") float RecoveryRatio = 0.0f;
	/* Reports the Damage increment rate selected for the current step. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M5") float DamageRatePerSimulationHour = 0.0f;
	/* Reports the Recovery decrement rate selected for the current step. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M5") float RecoveryRatePerSimulationHour = 0.0f;
	/* Identifies the latest committed M5 Grid change affecting this Cell. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M5") int64 ResponseRevision = 0;
	/* Identifies the consumed M3 Exposure revision. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M5") int64 SourceExposureRevision = 0;
	/* Identifies the consumed M4 constraint revision. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M5") int64 SourceConstraintRevision = 0;
	/* Identifies the fixed simulation step that produced this snapshot. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M5") int64 SimulationStep = 0;
};
