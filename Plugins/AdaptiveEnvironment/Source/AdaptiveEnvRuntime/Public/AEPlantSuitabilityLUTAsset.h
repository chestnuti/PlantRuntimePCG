#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AEPlantSuitabilityLUTAsset.generated.h"

class UTexture2D;

/* Stores one species slope-by-moisture response surface for deterministic CPU sampling. */
UCLASS(BlueprintType, meta = (DisplayName = "AE Plant Suitability LUT"))
class ADAPTIVEENVRUNTIME_API UAEPlantSuitabilityLUTAsset final : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/* Supplies the linear G8 or BGRA8 texture baked into runtime samples. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Source")
	TObjectPtr<UTexture2D> SourceTexture;

	/* Defines the inclusive moisture ratio represented by the first LUT column. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Axes", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MoistureMinimumRatio = 0.0f;

	/* Defines the inclusive moisture ratio represented by the last LUT column. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Axes", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MoistureMaximumRatio = 1.0f;

	/* Defines the inclusive slope represented by the first LUT row. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Axes", meta = (ClampMin = "0.0", ClampMax = "90.0", Units = "deg"))
	float SlopeMinimumDegrees = 0.0f;

	/* Defines the inclusive slope represented by the last LUT row. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Axes", meta = (ClampMin = "0.0", ClampMax = "90.0", Units = "deg"))
	float SlopeMaximumDegrees = 90.0f;

	/* Stores the baked moisture-column by slope-row dimensions. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Runtime")
	FIntPoint BakedDimensions = FIntPoint::ZeroValue;

	/* Stores row-major normalized suitability samples as unsigned 16-bit values. */
	UPROPERTY(VisibleAnywhere, Category = "Runtime")
	TArray<uint16> BakedSamples;

	/* Advances whenever the baked data or axis mapping changes. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Runtime")
	int32 ContentRevision = 0;

	/* Rebuilds runtime samples from the source texture's red channel. */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Adaptive Environment|M7")
	bool RebuildFromSourceTexture();

	/* Samples suitability with moisture on X and slope on Y using bilinear filtering. */
	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|M7")
	bool SampleSuitability(float SlopeDegrees, float MoistureRatio, float& OutSuitabilityRatio) const;

	/* Validates axes, dimensions, and baked sample storage. */
	bool IsValidLUT(FString& OutError) const;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
