#include "AEPlantSpeciesProfile.h"
#include "AEPlantBiomeMapAsset.h"

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
	OutError.Reset();
	return true;
}
