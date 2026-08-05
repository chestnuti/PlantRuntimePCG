#include "AEPlantBiomeMapAsset.h"

#include "AEWorldScalarFieldAsset.h"

#if WITH_EDITOR
#include "UObject/UnrealType.h"
#endif

/* Sample the dedicated biome texture and evaluate one named interval. */
float UAEPlantBiomeMapAsset::SampleBiomeWeight(const FName BiomeId, const FVector& WorldLocation) const
{
	const FAEBiomeRangeDefinition* Definition = FindBiome(BiomeId);
	float ScalarValue = 0.0f;
	if (Definition == nullptr || BiomeTexture == nullptr
		|| !BiomeTexture->SampleValue(WorldLocation, 0.0f, ScalarValue))
	{
		return 0.0f;
	}
	return EvaluateBiomeWeight(ScalarValue, *Definition);
}

/* Keep full density inside the core interval and smooth both outer edges. */
float UAEPlantBiomeMapAsset::EvaluateBiomeWeight(const float ScalarValue, const FAEBiomeRangeDefinition& Definition)
{
	if (!FMath::IsFinite(ScalarValue) || Definition.LowerBoundRatio > Definition.UpperBoundRatio)
	{
		return 0.0f;
	}
	const float Value = FMath::Clamp(ScalarValue, 0.0f, 1.0f);
	const float Width = FMath::Max(Definition.GradientWidthRatio, 0.0f);
	if (Width <= UE_KINDA_SMALL_NUMBER)
	{
		return Value >= Definition.LowerBoundRatio && Value <= Definition.UpperBoundRatio ? 1.0f : 0.0f;
	}
	const float LowerWeight = FMath::SmoothStep(Definition.LowerBoundRatio - Width, Definition.LowerBoundRatio, Value);
	const float UpperWeight = 1.0f - FMath::SmoothStep(Definition.UpperBoundRatio, Definition.UpperBoundRatio + Width, Value);
	return FMath::Clamp(LowerWeight * UpperWeight, 0.0f, 1.0f);
}

const FAEBiomeRangeDefinition* UAEPlantBiomeMapAsset::FindBiome(const FName BiomeId) const
{
	return Biomes.FindByPredicate([BiomeId](const FAEBiomeRangeDefinition& Definition)
	{
		return Definition.BiomeId == BiomeId;
	});
}

int32 UAEPlantBiomeMapAsset::GetRuntimeRevision() const
{
	return HashCombine(GetTypeHash(ContentRevision), GetTypeHash(BiomeTexture != nullptr ? BiomeTexture->ContentRevision : 0));
}

/* Reject invalid scalar storage, duplicate identities, and overlapping core intervals. */
bool UAEPlantBiomeMapAsset::IsValidMap(FString& OutError) const
{
	if (BiomeTexture == nullptr || !BiomeTexture->IsValidField(OutError))
	{
		if (OutError.IsEmpty()) OutError = TEXT("A dedicated biome texture field is required.");
		return false;
	}
	if (Biomes.IsEmpty())
	{
		OutError = TEXT("At least one biome definition is required.");
		return false;
	}
	TSet<FName> Identities;
	for (const FAEBiomeRangeDefinition& Definition : Biomes)
	{
		if (Definition.BiomeId.IsNone() || Identities.Contains(Definition.BiomeId)
			|| !FMath::IsFinite(Definition.LowerBoundRatio) || !FMath::IsFinite(Definition.UpperBoundRatio)
			|| !FMath::IsFinite(Definition.GradientWidthRatio)
			|| Definition.LowerBoundRatio < 0.0f || Definition.UpperBoundRatio > 1.0f
			|| Definition.LowerBoundRatio > Definition.UpperBoundRatio
			|| Definition.GradientWidthRatio < 0.0f || Definition.GradientWidthRatio > 1.0f)
		{
			OutError = TEXT("Biome identities, bounds, and GradientWidthRatio must be unique, finite, and normalized.");
			return false;
		}
		Identities.Add(Definition.BiomeId);
	}
	for (int32 A = 0; A < Biomes.Num(); ++A)
	{
		for (int32 B = A + 1; B < Biomes.Num(); ++B)
		{
			const bool bCoreOverlap = Biomes[A].LowerBoundRatio < Biomes[B].UpperBoundRatio
				&& Biomes[B].LowerBoundRatio < Biomes[A].UpperBoundRatio;
			if (bCoreOverlap)
			{
				OutError = TEXT("Biome core intervals must not overlap; gradient bands may overlap.");
				return false;
			}
		}
	}
	OutError.Reset();
	return true;
}

#if WITH_EDITOR
void UAEPlantBiomeMapAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	ContentRevision = ContentRevision == MAX_int32 ? 1 : ContentRevision + 1;
}
#endif
