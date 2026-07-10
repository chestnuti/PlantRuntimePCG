#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AdaptiveEnvTypes.h"
#include "AdaptiveEnvDataAssets.generated.h"

/** Stores literature observations before parameter synthesis. */
UCLASS(BlueprintType)
class ADAPTIVEENVRUNTIME_API UAELiteratureEvidenceAsset final : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Version")
	int32 SchemaVersion = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Evidence")
	TArray<FAELiteratureSource> Sources;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Evidence")
	TArray<FAEEvidenceRecord> EvidenceRecords;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Evidence")
	TArray<FAEEvidenceTransformation> Transformations;
};

/** Stores derived parameters and their synthesis records. */
UCLASS(BlueprintType)
class ADAPTIVEENVRUNTIME_API UAEParameterSynthesisAsset final : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Version")
	FAEParameterPackageMetadata Metadata;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Synthesis")
	TArray<FAEParameterSynthesis> Syntheses;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameters")
	TArray<FAEDerivedParameter> DerivedParameters;
};

/** Defines one reproducible experiment configuration. */
UCLASS(BlueprintType)
class ADAPTIVEENVRUNTIME_API UAEExperimentConfig final : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FGuid ExperimentId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FName ScenarioId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameters")
	TSoftObjectPtr<UAEParameterSynthesisAsset> ParameterPackage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Simulation")
	int32 RandomSeed = 1337;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Simulation", meta = (ClampMin = "0.0"))
	float SimulationHoursPerRealSecond = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Simulation", meta = (ClampMin = "0.0"))
	float DurationRealSeconds = 600.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Version")
	int32 SchemaVersion = 1;
};
