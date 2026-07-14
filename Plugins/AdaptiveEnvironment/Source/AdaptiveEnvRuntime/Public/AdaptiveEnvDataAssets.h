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
	/** Identifies the serialized evidence asset schema. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Version")
	int32 SchemaVersion = 1;

	/** Stores bibliographic and study metadata for all cited sources. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Evidence")
	TArray<FAELiteratureSource> Sources;

	/** Stores values extracted directly from literature sources. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Evidence")
	TArray<FAEEvidenceRecord> EvidenceRecords;

	/** Stores unit and context transformations applied to evidence. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Evidence")
	TArray<FAEEvidenceTransformation> Transformations;
};

/** Stores derived parameters and their synthesis records. */
UCLASS(BlueprintType)
class ADAPTIVEENVRUNTIME_API UAEParameterSynthesisAsset final : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Describes the identity, version, and review state of this package. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Version")
	FAEParameterPackageMetadata Metadata;

	/** Stores the method and contributions used for each synthesis. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Synthesis")
	TArray<FAEParameterSynthesis> Syntheses;

	/** Stores evidence-based and effective runtime parameter values. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameters")
	TArray<FAEDerivedParameter> DerivedParameters;
};

/** Defines one reproducible experiment configuration. */
UCLASS(BlueprintType)
class ADAPTIVEENVRUNTIME_API UAEExperimentConfig final : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Uniquely identifies the experiment configuration. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FGuid ExperimentId;

	/** Identifies the functional scenario executed by the experiment. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FName ScenarioId;

	/** References the versioned parameter package used by the experiment. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameters")
	TSoftObjectPtr<UAEParameterSynthesisAsset> ParameterPackage;

	/** Provides the deterministic random seed for the experiment. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Simulation")
	int32 RandomSeed = 1337;

	/** Defines simulated hours advanced by one real second. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Simulation", meta = (ClampMin = "0.0"))
	float SimulationHoursPerRealSecond = 6.0f;

	/** Defines the experiment duration in real seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Simulation", meta = (ClampMin = "0.0"))
	float DurationRealSeconds = 600.0f;

	/** Identifies the serialized experiment schema version. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Version")
	int32 SchemaVersion = 1;
};
