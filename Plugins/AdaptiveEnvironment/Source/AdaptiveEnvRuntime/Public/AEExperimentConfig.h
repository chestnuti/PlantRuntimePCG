#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AEPublishedParameterBundleAsset.h"
#include "AEExperimentConfig.generated.h"

/* Defines one reproducible runtime experiment configuration. */
UCLASS(BlueprintType)
class ADAPTIVEENVRUNTIME_API UAEExperimentConfig final : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/* Uniquely identifies the experiment configuration. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FGuid ExperimentId;
	/* Identifies the functional scenario executed by the experiment. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FName ScenarioId;
	/* References the exact published M3/M4 parameter bundle used by the experiment. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameters")
	TSoftObjectPtr<UAEPublishedParameterBundleAsset> ParameterBundle;
	/* Provides the deterministic random seed for the experiment. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Simulation")
	int32 RandomSeed = 1337;
	/* Defines simulated hours advanced by one real second. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Simulation", meta = (ClampMin = "0.0"))
	float SimulationHoursPerRealSecond = 6.0f;
	/* Defines the experiment duration in real seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Simulation", meta = (ClampMin = "0.0"))
	float DurationRealSeconds = 600.0f;
	/* Identifies the serialized experiment schema version. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Version")
	int32 SchemaVersion = 2;
};
