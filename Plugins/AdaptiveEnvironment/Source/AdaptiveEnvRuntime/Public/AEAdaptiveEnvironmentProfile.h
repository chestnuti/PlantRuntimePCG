#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AEM2ConfigTypes.h"
#include "AEAdaptiveEnvironmentProfile.generated.h"

/* Stores the single product configuration entry for M3, M4, and M5. */
UCLASS(BlueprintType)
class ADAPTIVEENVRUNTIME_API UAEAdaptiveEnvironmentProfile final : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FName ProfileId = TEXT("Default");

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Version")
	int32 ConfigVersion = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M3 Exposure")
	FAEM3UserConfig M3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M4 Environment")
	FAEM4UserConfig M4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M5 Ecology")
	FAEM5UserConfig M5;
};
