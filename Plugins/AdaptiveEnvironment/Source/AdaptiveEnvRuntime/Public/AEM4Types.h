#pragma once

#include "CoreMinimal.h"
#include "AEParameterBundleTypes.h"
#include "AEM3Types.h"
#include "AdaptiveEnvTypes.h"
#include "AEM4Types.generated.h"

/* Stores terrain and moisture constraint-response parameters. */
struct ADAPTIVEENVRUNTIME_API FAEConstraintResponseParameters
{
	/* Defines the slope angle below which suitability remains one. */
	double SlopeFullySuitableDegrees = 0.0;
	/* Defines the slope angle at which suitability reaches zero. */
	double SlopeUnsuitableDegrees = 90.0;
	/* Defines the inclusive lower moisture optimum ratio. */
	double MoistureOptimalMinimumRatio = 0.0;
	/* Defines the inclusive upper moisture optimum ratio. */
	double MoistureOptimalMaximumRatio = 1.0;
	/* Defines the positive moisture falloff width outside the optimum interval. */
	double MoistureToleranceWidthRatio = 0.1;
	/* Scales constraint stress when deriving effective pressure. */
	double ConstraintStressSensitivity = 0.0;
};

/* Stores shared region-state thresholds, hysteresis, and debounce. */
struct ADAPTIVEENVRUNTIME_API FAERegionStateParameters
{
	/* Defines the centre threshold between Unused and Active. */
	double ActiveThreshold = 0.25;
	/* Defines the centre threshold between Active and Overused. */
	double OverusedThreshold = 0.75;
	/* Defines the shared total hysteresis width around both thresholds. */
	double HysteresisWidth = 0.1;
	/* Defines continuous candidate duration required before transition in simulated hours. */
	double TransitionDebounceSimulationHours = 0.0;
};

/* Stores the validated effective M4 model snapshot. */
struct ADAPTIVEENVRUNTIME_API FAEM4ParameterSet
{
	/* Stores constraint suitability and fragility response values. */
	FAEConstraintResponseParameters ConstraintResponse;
	/* Stores state threshold and temporal stability values. */
	FAERegionStateParameters RegionState;
};

/* Atomically binds one bundle identity to its M3 and M4 typed values. */
struct ADAPTIVEENVRUNTIME_API FAEActiveParameterSnapshot
{
	/* Stores immutable bundle identity for manifests and switch checks. */
	FAEParameterBundleIdentity BundleIdentity;
	/* Stores the validated M3 block identity. */
	FGuid M3BlockId;
	/* Stores the validated M3 block semantic version. */
	FString M3BlockVersion;
	/* Stores the validated M3 block canonical hash. */
	FString M3BlockHash;
	/* Stores the validated M4 block identity. */
	FGuid M4BlockId;
	/* Stores the validated M4 block semantic version. */
	FString M4BlockVersion;
	/* Stores the validated M4 block canonical hash. */
	FString M4BlockHash;
	/* Stores grouped M3 runtime values. */
	FAEM3ParameterSet M3;
	/* Stores grouped M4 runtime values. */
	FAEM4ParameterSet M4;
};

/* Stores mutable temporal state for one M4 region or Cell. */
struct ADAPTIVEENVRUNTIME_API FAEM4StateMemory
{
	/* Stores the currently committed ecological state. */
	EAERegionState CurrentState = EAERegionState::Unused;
	/* Stores the candidate state while debounce accumulates. */
	EAERegionState CandidateState = EAERegionState::Unused;
	/* Stores continuous candidate duration in simulated hours. */
	double CandidateDurationSimulationHours = 0.0;
};

/* Exposes one immutable M4 constraint and decision result. */
USTRUCT(BlueprintType)
struct ADAPTIVEENVRUNTIME_API FAEM4DecisionSnapshot
{
	GENERATED_BODY()

	/* Stores normalized combined slope and moisture stress from zero to one. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M4")
	float ConstraintStressRatio = 0.0f;
	/* Stores constraint-adjusted Exposure pressure from zero to one. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M4")
	float EffectiveDisturbanceRatio = 0.0f;
	/* Stores constraint-adjusted ecological Damage pressure from zero to one. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M4")
	float EffectiveDamageRatio = 0.0f;
	/* Stores the maximum of disturbance and Damage pressure from zero to one. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M4")
	float EffectivePressureRatio = 0.0f;
	/* Stores the state committed after hysteresis and debounce. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M4")
	EAERegionState State = EAERegionState::Unused;
};
