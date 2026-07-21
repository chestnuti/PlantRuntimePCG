#pragma once

#include "CoreMinimal.h"
#include "AEParameterBundleTypes.h"
#include "AEM3Types.h"
#include "AEM5Types.h"
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

/* Atomically binds one bundle identity to its M3, M4, and M5 typed values. */
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
	/* Stores the validated M5 block identity. */
	FGuid M5BlockId;
	/* Stores the validated M5 block semantic version. */
	FString M5BlockVersion;
	/* Stores the validated M5 block canonical hash. */
	FString M5BlockHash;
	/* Stores grouped M3 runtime values. */
	FAEM3ParameterSet M3;
	/* Stores grouped M4 runtime values. */
	FAEM4ParameterSet M4;
	/* Stores grouped M5 runtime values. */
	FAEM5ParameterSet M5;
};

/* Stores mutable temporal state for one M4 region or Cell. */
struct ADAPTIVEENVRUNTIME_API FAEM4StateMemory
{
	/* Stores the currently committed environment state. */
	EAERegionState CurrentState = EAERegionState::Unused;
	/* Stores the candidate state while debounce accumulates. */
	EAERegionState CandidateState = EAERegionState::Unused;
	/* Stores continuous candidate duration in simulated hours. */
	double CandidateDurationSimulationHours = 0.0;
};

/* Exposes one immutable M4 environment-constraint result. */
USTRUCT(BlueprintType)
struct ADAPTIVEENVRUNTIME_API FAEEnvironmentConstraintSnapshot
{
	GENERATED_BODY()

	/* Stores normalized combined slope and moisture pressure from zero to one. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M4")
	float ConstraintPressureRatio = 0.0f;
	/* Stores combined habitat suitability from zero to one. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M4")
	float HabitatSuitabilityRatio = 1.0f;
	/* Stores the environment state committed after hysteresis and debounce. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M4")
	EAERegionState State = EAERegionState::Unused;
};
