#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AEPlantBiomeMapAsset.generated.h"

class UAEBiomeTextureAsset;

/* Maps one inclusive scalar interval to a named biome with a soft outer edge. */
USTRUCT(BlueprintType)
struct ADAPTIVEENVRUNTIME_API FAEBiomeRangeDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biome")
	FName BiomeId = TEXT("Default");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biome", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LowerBoundRatio = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biome", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float UpperBoundRatio = 1.0f;

	/* Defines the smooth transition width outside both interval boundaries. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biome", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GradientWidthRatio = 0.05f;
};

/* Converts one dedicated biome texture field into named M7 density weights. */
UCLASS(BlueprintType)
class ADAPTIVEENVRUNTIME_API UAEPlantBiomeMapAsset final : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/* Supplies the dedicated baked R-channel biome texture used by every definition. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	TObjectPtr<UAEBiomeTextureAsset> BiomeTexture;

	/* Defines non-overlapping core intervals in authored order. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	TArray<FAEBiomeRangeDefinition> Biomes;

	/* Advances when an editor-authored biome definition changes. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Runtime")
	int32 ContentRevision = 1;

	/* Samples one named biome's normalized density weight. */
	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|M7")
	float SampleBiomeWeight(FName BiomeId, const FVector& WorldLocation) const;

	/* Evaluates the plateau and smooth outer falloff for one scalar value. */
	static float EvaluateBiomeWeight(float ScalarValue, const FAEBiomeRangeDefinition& Definition);

	/* Finds one authored biome definition by stable identity. */
	const FAEBiomeRangeDefinition* FindBiome(FName BiomeId) const;

	/* Combines map and scalar-field revisions for cache invalidation. */
	int32 GetRuntimeRevision() const;

	/* Validates the scalar field, identities, ranges, and core overlap contract. */
	bool IsValidMap(FString& OutError) const;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
