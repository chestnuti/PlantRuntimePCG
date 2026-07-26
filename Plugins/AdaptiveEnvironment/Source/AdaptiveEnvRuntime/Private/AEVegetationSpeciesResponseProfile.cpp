#include "AEVegetationSpeciesResponseProfile.h"

#include "Algo/AllOf.h"

/* Validate this complete research-labelled profile before runtime registration. */
bool UAEVegetationSpeciesResponseProfile::BuildValidatedParameters(
	FAEM7SpeciesParameters& OutParameters,
	FString& OutError) const
{
	OutError.Reset();
	// Validate stable research identity before accepting effective values.
	if (!SpeciesId.IsValid() || SemanticVersion.IsEmpty()
		|| ContentHash.Len() != 64
		|| !Algo::AllOf(ContentHash, [](const TCHAR Character)
		{
			// Require canonical lowercase hexadecimal characters.
			return FChar::IsHexDigit(Character) && !FChar::IsUpper(Character);
		}))
	{
		OutError = TEXT("SpeciesId, SemanticVersion, or lowercase SHA-256 ContentHash is invalid.");
		return false;
	}

	// Validate every finite numerical field and cross-field range.
	if (!FMath::IsFinite(DamageToleranceRatio)
		|| !FMath::IsFinite(DamageResponseExponent)
		|| !FMath::IsFinite(MinimumDensityRatio)
		|| !FMath::IsFinite(DeclineRatePerSimulationHour)
		|| !FMath::IsFinite(RecoveryRatePerSimulationHour)
		|| !FMath::IsFinite(HealthDirtyEpsilon)
		|| DamageToleranceRatio < 0.0f || DamageToleranceRatio >= 1.0f
		|| DamageResponseExponent <= 0.0f
		|| MinimumDensityRatio < 0.0f || MinimumDensityRatio > 1.0f
		|| DeclineRatePerSimulationHour < 0.0f
		|| RecoveryRatePerSimulationHour < 0.0f
		|| HealthDirtyEpsilon <= 0.0f || HealthDirtyEpsilon > 1.0f)
	{
		OutError = TEXT("Species response values are non-finite or outside the documented ranges.");
		return false;
	}

	// Freeze editor values into the plain-data simulation contract.
	OutParameters.DamageToleranceRatio = DamageToleranceRatio;
	OutParameters.DamageResponseExponent = DamageResponseExponent;
	OutParameters.MinimumDensityRatio = MinimumDensityRatio;
	OutParameters.DeclineRatePerSimulationHour = DeclineRatePerSimulationHour;
	OutParameters.RecoveryRatePerSimulationHour = RecoveryRatePerSimulationHour;
	OutParameters.HealthDirtyEpsilon = HealthDirtyEpsilon;
	return true;
}
