#include "AEPlantSpeciesProfile.h"
#include "AEPlantBiomeMapAsset.h"
#include "AEPlantSuitabilityLUTAsset.h"

#if WITH_EDITOR
#include "UObject/UnrealType.h"
#endif

/* Validates the structural and ecological species contract. */
bool UAEPlantSpeciesProfile::IsValidProfile(FString& OutError) const
{
	if (SpeciesId.IsNone() || StaticMesh.IsNull() || MinimumSpacingCm <= 0.0f || MaximumInstancesPerSquareMeter <= 0.0f)
	{
		OutError = TEXT("Species id, static mesh, spacing, and maximum density are required.");
		return false;
	}
	if (!FMath::IsFinite(GroundOffsetCm))
	{
		OutError = TEXT("Ground offset must be finite.");
		return false;
	}
	if (BiomeMap != nullptr && !BiomeMap->IsValidMap(OutError))
	{
		return false;
	}
	if (!FMath::IsFinite(DeathFadeDurationSeconds) || DeathFadeDurationSeconds <= 0.0f)
	{
		OutError = TEXT("Death fade duration must be finite and greater than zero seconds.");
		return false;
	}
	if (BiomeMap != nullptr && BiomeMap->FindBiome(BiomeId) == nullptr)
	{
		OutError = TEXT("BiomeId must identify one definition in BiomeMap.");
		return false;
	}
	if (SuitabilityLUT != nullptr && !SuitabilityLUT->IsValidLUT(OutError))
	{
		return false;
	}
	const float SuitabilityValues[] = {
		ManualSuitability.SlopeFullySuitableDegrees,
		ManualSuitability.SlopeUnsuitableDegrees,
		ManualSuitability.MoistureOptimalMinimumRatio,
		ManualSuitability.MoistureOptimalMaximumRatio,
		ManualSuitability.MoistureToleranceWidthRatio,
		SuitabilityScale,
		SpeciesDamageSensitivity,
		DeclineRatePerSimulationHour,
		RecoveryRatePerSimulationHour
	};
	for (const float Value : SuitabilityValues)
	{
		if (!FMath::IsFinite(Value))
		{
			OutError = TEXT("Suitability and lifecycle values must be finite.");
			return false;
		}
	}
	if (ManualSuitability.SlopeFullySuitableDegrees < 0.0f
		|| ManualSuitability.SlopeFullySuitableDegrees >= ManualSuitability.SlopeUnsuitableDegrees
		|| ManualSuitability.SlopeUnsuitableDegrees > 90.0f
		|| ManualSuitability.MoistureOptimalMinimumRatio < 0.0f
		|| ManualSuitability.MoistureOptimalMinimumRatio > ManualSuitability.MoistureOptimalMaximumRatio
		|| ManualSuitability.MoistureOptimalMaximumRatio > 1.0f
		|| ManualSuitability.MoistureToleranceWidthRatio <= 0.0f
		|| ManualSuitability.MoistureToleranceWidthRatio > 1.0f
		|| SuitabilityScale < 0.0f || SuitabilityScale > 2.0f
		|| SpeciesDamageSensitivity < 0.0f || SpeciesDamageSensitivity > 1.0f
		|| DeclineRatePerSimulationHour < 0.0f || RecoveryRatePerSimulationHour < 0.0f)
	{
		OutError = TEXT("Manual suitability ranges, scale, sensitivity, and lifecycle rates are invalid.");
		return false;
	}
	OutError.Reset();
	return true;
}

int32 UAEPlantSpeciesProfile::GetSuitabilityRuntimeRevision() const
{
	return HashCombine(
		GetTypeHash(ContentRevision),
		GetTypeHash(SuitabilityLUT != nullptr ? SuitabilityLUT->ContentRevision : 0));
}

#if WITH_EDITOR
void UAEPlantSpeciesProfile::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	ContentRevision = ContentRevision == MAX_int32 ? 1 : ContentRevision + 1;
}
#endif
