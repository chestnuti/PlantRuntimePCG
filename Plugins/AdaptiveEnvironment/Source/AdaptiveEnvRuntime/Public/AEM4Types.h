#pragma once

#include "CoreMinimal.h"
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

	/* Stores the integer XY Cell coordinate. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M4")
	FIntPoint Coordinate = FIntPoint::ZeroValue;
	/* Stores the Cell centre in world centimetres at the sampled ground height. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M4")
	FVector WorldCenter = FVector::ZeroVector;
	/* Stores the sampled ground slope in degrees. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M4")
	float SlopeDegrees = 0.0f;
	/* Stores the selected moisture-source value in the zero-to-one range. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M4")
	float MoistureRatio = 0.5f;

	/* Stores normalized combined slope and moisture pressure from zero to one. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M4")
	float ConstraintPressureRatio = 0.0f;
	/* Stores combined habitat suitability from zero to one. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M4")
	float HabitatSuitabilityRatio = 1.0f;
	/* Stores the environment state committed after hysteresis and debounce. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M4")
	EAERegionState State = EAERegionState::Unused;
	/* Identifies the latest committed M4 Grid change affecting this Cell. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M4")
	int64 ConstraintRevision = 0;
	/* Identifies the fixed simulation step that produced this snapshot. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M4")
	int64 SimulationStep = 0;
};
