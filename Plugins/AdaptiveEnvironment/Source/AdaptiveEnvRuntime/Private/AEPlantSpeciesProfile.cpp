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
	if (BiomeMap != nullptr && !BiomeMap->IsValidMap(OutError))
	{
		return false;
	}
	OutError.Reset();
	return true;
}
