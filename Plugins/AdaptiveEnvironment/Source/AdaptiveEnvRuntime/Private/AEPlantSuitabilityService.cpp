#include "AEPlantSuitabilityService.h"

#include "AEPlantSpeciesProfile.h"
#include "AEPlantSuitabilityLUTAsset.h"

namespace AEPlantSuitabilityServicePrivate
{
	float EvaluateSlope(
		const float SlopeDegrees,
		const FAEPlantManualSuitabilityParameters& Parameters)
	{
		const float Slope = FMath::Clamp(SlopeDegrees, 0.0f, 90.0f);
		if (Slope <= Parameters.SlopeFullySuitableDegrees) return 1.0f;
		if (Slope >= Parameters.SlopeUnsuitableDegrees) return 0.0f;
		return 1.0f - (Slope - Parameters.SlopeFullySuitableDegrees)
			/ (Parameters.SlopeUnsuitableDegrees - Parameters.SlopeFullySuitableDegrees);
	}

	float EvaluateMoisture(
		const float MoistureRatio,
		const FAEPlantManualSuitabilityParameters& Parameters)
	{
		const float Moisture = FMath::Clamp(MoistureRatio, 0.0f, 1.0f);
		if (Moisture >= Parameters.MoistureOptimalMinimumRatio
			&& Moisture <= Parameters.MoistureOptimalMaximumRatio)
		{
			return 1.0f;
		}
		const float Distance = Moisture < Parameters.MoistureOptimalMinimumRatio
			? Parameters.MoistureOptimalMinimumRatio - Moisture
			: Moisture - Parameters.MoistureOptimalMaximumRatio;
		return FMath::Clamp(1.0f - Distance / Parameters.MoistureToleranceWidthRatio, 0.0f, 1.0f);
	}
}

/* Prefer a valid authored response surface and fall back only when it is absent. */
bool FAEPlantSuitabilityService::EvaluateEnvironmentResponse(
	const UAEPlantSpeciesProfile& Profile,
	const float SlopeDegrees,
	const float MoistureRatio,
	float& OutEnvironmentResponseRatio)
{
	if (!FMath::IsFinite(SlopeDegrees) || !FMath::IsFinite(MoistureRatio))
	{
		return false;
	}
	if (Profile.SuitabilityLUT != nullptr)
	{
		return Profile.SuitabilityLUT->SampleSuitability(
			SlopeDegrees,
			MoistureRatio,
			OutEnvironmentResponseRatio);
	}
	OutEnvironmentResponseRatio = EvaluateManualResponse(
		Profile.ManualSuitability,
		SlopeDegrees,
		MoistureRatio);
	return true;
}

/* Multiply independent manual slope and moisture suitability responses. */
float FAEPlantSuitabilityService::EvaluateManualResponse(
	const FAEPlantManualSuitabilityParameters& Parameters,
	const float SlopeDegrees,
	const float MoistureRatio)
{
	if (!FMath::IsFinite(SlopeDegrees) || !FMath::IsFinite(MoistureRatio))
	{
		return 0.0f;
	}
	if (!FMath::IsFinite(Parameters.SlopeFullySuitableDegrees)
		|| !FMath::IsFinite(Parameters.SlopeUnsuitableDegrees)
		|| !FMath::IsFinite(Parameters.MoistureOptimalMinimumRatio)
		|| !FMath::IsFinite(Parameters.MoistureOptimalMaximumRatio)
		|| !FMath::IsFinite(Parameters.MoistureToleranceWidthRatio)
		|| Parameters.SlopeFullySuitableDegrees < 0.0f
		|| Parameters.SlopeFullySuitableDegrees >= Parameters.SlopeUnsuitableDegrees
		|| Parameters.SlopeUnsuitableDegrees > 90.0f
		|| Parameters.MoistureOptimalMinimumRatio < 0.0f
		|| Parameters.MoistureOptimalMinimumRatio > Parameters.MoistureOptimalMaximumRatio
		|| Parameters.MoistureOptimalMaximumRatio > 1.0f
		|| Parameters.MoistureToleranceWidthRatio <= 0.0f)
	{
		return 0.0f;
	}
	return FMath::Clamp(
		AEPlantSuitabilityServicePrivate::EvaluateSlope(SlopeDegrees, Parameters)
		* AEPlantSuitabilityServicePrivate::EvaluateMoisture(MoistureRatio, Parameters),
		0.0f,
		1.0f);
}

float FAEPlantSuitabilityService::ResolveSpeciesSuitability(
	const float EnvironmentResponseRatio,
	const float SuitabilityScale,
	const float BiomeWeight)
{
	if (!FMath::IsFinite(EnvironmentResponseRatio) || !FMath::IsFinite(SuitabilityScale)
		|| !FMath::IsFinite(BiomeWeight))
	{
		return 0.0f;
	}
	return FMath::Clamp(FMath::Clamp(EnvironmentResponseRatio, 0.0f, 1.0f)
		* FMath::Clamp(SuitabilityScale, 0.0f, 2.0f)
		* FMath::Clamp(BiomeWeight, 0.0f, 1.0f), 0.0f, 1.0f);
}

float FAEPlantSuitabilityService::ResolveEffectiveRecoveryRate(
	const float BaseRecoveryRatePerSimulationHour,
	const float SpeciesSuitabilityRatio)
{
	if (!FMath::IsFinite(BaseRecoveryRatePerSimulationHour)
		|| !FMath::IsFinite(SpeciesSuitabilityRatio))
	{
		return 0.0f;
	}
	return FMath::Max(BaseRecoveryRatePerSimulationHour, 0.0f)
		* FMath::Clamp(SpeciesSuitabilityRatio, 0.0f, 1.0f);
}

float FAEPlantSuitabilityService::ResolveTargetHealth(
	const float CellDamageRatio,
	const float SpeciesDamageSensitivity,
	const float HealthVariation)
{
	if (!FMath::IsFinite(CellDamageRatio) || !FMath::IsFinite(SpeciesDamageSensitivity)
		|| !FMath::IsFinite(HealthVariation))
	{
		return 0.0f;
	}
	return FMath::Clamp(
		1.0f - FMath::Clamp(CellDamageRatio, 0.0f, 1.0f)
		* FMath::Clamp(SpeciesDamageSensitivity, 0.0f, 1.0f)
		+ HealthVariation,
		0.0f,
		1.0f);
}
