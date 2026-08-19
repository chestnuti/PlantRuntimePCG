#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AEWorldScalarFieldAsset.generated.h"

class UTexture2D;

UENUM(BlueprintType)
enum class EAETextureOutsidePolicy : uint8
{
	UseFallback,
	ClampToEdge,
	Zero
};

/* Stores one baked R-channel scalar field mapped onto a world-space XY rectangle. */
UCLASS(Abstract, BlueprintType)
class ADAPTIVEENVRUNTIME_API UAEWorldScalarFieldAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/* Supplies the authoring texture whose R channel is baked for runtime CPU sampling. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Source")
	TObjectPtr<UTexture2D> SourceTexture;

	/* Defines the inclusive lower world XY sample bound in centimetres. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mapping")
	FVector2D WorldMin = FVector2D(-5000.0, -5000.0);

	/* Defines the inclusive upper world XY sample bound in centimetres. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mapping")
	FVector2D WorldMax = FVector2D(5000.0, 5000.0);

	/* Flips texture V so top-left-authored images align with positive world Y. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mapping")
	bool bFlipVerticalAxis = true;

	/* Selects how samples outside WorldMin and WorldMax are resolved. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mapping")
	EAETextureOutsidePolicy OutsidePolicy = EAETextureOutsidePolicy::UseFallback;

	/* Stores the baked runtime raster width and height. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Runtime")
	FIntPoint BakedDimensions = FIntPoint::ZeroValue;

	/* Stores normalized R-channel samples as row-major unsigned 16-bit values. */
	UPROPERTY(VisibleAnywhere, Category = "Runtime")
	TArray<uint16> BakedSamples;

	/* Advances whenever the baked runtime data changes. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Runtime")
	int32 ContentRevision = 0;

	/* Rebuilds runtime samples from supported G8 or BGRA8 source data. */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Adaptive Environment|Scalar Field")
	bool RebuildFromSourceTexture();

	/* Samples one normalized value with bilinear filtering. */
	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|Scalar Field")
	bool SampleValue(const FVector& WorldLocation, float FallbackValue, float& OutValue) const;

	/* Validates bounds, dimensions, and baked storage. */
	bool IsValidField(FString& OutError) const;
};

/* Stores the dedicated M4 moisture texture field. */
UCLASS(BlueprintType, meta = (DisplayName = "AE Moisture Texture Field"))
class ADAPTIVEENVRUNTIME_API UAEMoistureTextureAsset final : public UAEWorldScalarFieldAsset
{
	GENERATED_BODY()
};

/* Stores the dedicated M7 biome classification texture field. */
UCLASS(BlueprintType, meta = (DisplayName = "AE Biome Texture Field"))
class ADAPTIVEENVRUNTIME_API UAEBiomeTextureAsset final : public UAEWorldScalarFieldAsset
{
	GENERATED_BODY()
};
