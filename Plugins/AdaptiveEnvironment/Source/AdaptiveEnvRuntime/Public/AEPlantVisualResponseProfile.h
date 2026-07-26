#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AEPlantVisualResponseProfile.generated.h"

/* Defines material-facing M7 custom-data slots without owning ecology parameters. */
UCLASS(BlueprintType)
class ADAPTIVEENVRUNTIME_API UAEPlantVisualResponseProfile final : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/* Selects the per-instance custom-data slot that receives HealthRatio. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Adaptive Environment|M7|Visual", meta = (ClampMin = "0"))
	int32 HealthCustomDataIndex = 0;
	/* Selects the per-instance custom-data slot that receives binary density visibility. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Adaptive Environment|M7|Visual", meta = (ClampMin = "0"))
	int32 DensityVisibilityCustomDataIndex = 1;
};
