#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AEPlantBiomeMapAsset.generated.h"

UCLASS(BlueprintType)
class ADAPTIVEENVRUNTIME_API UAEPlantBiomeMapAsset final : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/* Stores the immutable research asset identity. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity") FGuid MapId;
	/* Stores the semantic asset version. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity") FString SemanticVersion = TEXT("1.0.0");
	/* Defines raster width and height. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map") FIntPoint Dimensions = FIntPoint(1, 1);
	/* Defines the inclusive lower world XY sample bound. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map") FVector2D WorldMin = FVector2D(-5000.0, -5000.0);
	/* Defines the inclusive upper world XY sample bound. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map") FVector2D WorldMax = FVector2D(5000.0, 5000.0);
	/* Stores normalized row-major species weights. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map") TArray<float> Weights = {1.0f};

	/* Samples one normalized weight at a world position. */
	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|M7")
	float SampleWeight(const FVector& WorldLocation) const;
	/* Validates the complete biome-map storage contract. */
	bool IsValidMap(FString& OutError) const;
};
