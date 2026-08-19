#pragma once

#include "CoreMinimal.h"

class UAEPlantSpeciesProfile;
struct FAEPlantManualSuitabilityParameters;

/* Evaluates one species' environment response and downstream normalized factors. */
struct ADAPTIVEENVRUNTIME_API FAEPlantSuitabilityService
{
	/* Uses the configured LUT, or manual parameters when no LUT is assigned. */
	static bool EvaluateEnvironmentResponse(
		const UAEPlantSpeciesProfile& Profile,
		float SlopeDegrees,
		float MoistureRatio,
		float& OutEnvironmentResponseRatio);

	/* Evaluates the manual plateau and linear-falloff response. */
	static float EvaluateManualResponse(
		const FAEPlantManualSuitabilityParameters& Parameters,
		float SlopeDegrees,
		float MoistureRatio);

	/* Combines environment response, authored scale, and biome prior exactly once. */
	static float ResolveSpeciesSuitability(
		float EnvironmentResponseRatio,
		float SuitabilityScale,
		float BiomeWeight);

	/* Scales species recovery by current final suitability. */
	static float ResolveEffectiveRecoveryRate(
		float BaseRecoveryRatePerSimulationHour,
		float SpeciesSuitabilityRatio);

	/* Maps shared Cell Damage through one species-specific sensitivity. */
	static float ResolveTargetHealth(
		float CellDamageRatio,
		float SpeciesDamageSensitivity,
		float HealthVariation);
};
