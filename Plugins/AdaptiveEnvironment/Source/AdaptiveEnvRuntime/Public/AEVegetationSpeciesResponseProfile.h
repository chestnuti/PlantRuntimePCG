#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AEParameterBundleTypes.h"
#include "AEM7Types.h"
#include "AEVegetationSpeciesResponseProfile.generated.h"

/* Publishes one editor-authored, provenance-labelled M7 species-response contract. */
UCLASS(BlueprintType)
class ADAPTIVEENVRUNTIME_API UAEVegetationSpeciesResponseProfile final : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/* Uniquely identifies the species-response lineage. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Adaptive Environment|M7|Identity")
	FGuid SpeciesId;
	/* Identifies this reviewed profile version. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Adaptive Environment|M7|Identity")
	FString SemanticVersion = TEXT("1.0.0");
	/* Stores the canonical lowercase SHA-256 digest supplied by the research pipeline. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Adaptive Environment|M7|Identity")
	FString ContentHash;
	/* Identifies which reviewed research layer supplied the effective values. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Adaptive Environment|M7|Identity")
	EAEParameterOriginLayer OriginLayer = EAEParameterOriginLayer::ResearcherCalibrated;
	/* Ignores M5 Damage at or below this normalized tolerance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Adaptive Environment|M7|Response", meta = (ClampMin = "0.0", ClampMax = "0.999999"))
	float DamageToleranceRatio = 0.15f;
	/* Shapes normalized Damage response above tolerance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Adaptive Environment|M7|Response", meta = (ClampMin = "0.000001"))
	float DamageResponseExponent = 1.0f;
	/* Retains this normalized density when health reaches zero. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Adaptive Environment|M7|Response", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinimumDensityRatio = 0.10f;
	/* Limits health decline per simulation hour. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Adaptive Environment|M7|Dynamics", meta = (ClampMin = "0.0"))
	float DeclineRatePerSimulationHour = 0.25f;
	/* Limits health recovery per simulation hour. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Adaptive Environment|M7|Dynamics", meta = (ClampMin = "0.0"))
	float RecoveryRatePerSimulationHour = 0.10f;
	/* Suppresses revisions below this normalized health difference. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Adaptive Environment|M7|Dynamics", meta = (ClampMin = "0.000001", ClampMax = "1.0"))
	float HealthDirtyEpsilon = 0.0039215686f;

	/* Validates identity, provenance, and every numerical field. */
	bool BuildValidatedParameters(FAEM7SpeciesParameters& OutParameters, FString& OutError) const;
};
