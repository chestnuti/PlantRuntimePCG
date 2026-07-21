#pragma once

#include "CoreMinimal.h"
#include "AEM5Types.generated.h"

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
};
