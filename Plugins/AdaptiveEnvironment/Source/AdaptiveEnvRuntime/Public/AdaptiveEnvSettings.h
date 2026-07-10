#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "AdaptiveEnvSettings.generated.h"

UCLASS(Config = AdaptiveEnvironment, DefaultConfig, meta = (DisplayName = "Adaptive Environment"))
class ADAPTIVEENVRUNTIME_API UAdaptiveEnvSettings final : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UAdaptiveEnvSettings();

	virtual FName GetCategoryName() const override;

	/** Enables runtime updates in supported worlds. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Runtime")
	bool bEnableRuntime = true;

	/** Controls behaviour sampling frequency. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Runtime", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float BehaviourSampleRateHz = 10.0f;

	/** Controls evaluation snapshot frequency. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Runtime", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float EvaluationRateHz = 1.0f;

	/** Defines the initial grid width. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Grid", meta = (ClampMin = "1", UIMin = "1"))
	int32 GridWidth = 128;

	/** Defines the initial grid height. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Grid", meta = (ClampMin = "1", UIMin = "1"))
	int32 GridHeight = 128;

	/** Defines one grid cell in centimetres. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Grid", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float CellSizeCm = 100.0f;

	/** Converts real seconds to simulated hours. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Simulation", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float SimulationHoursPerRealSecond = 6.0f;

	/** Provides a deterministic default seed. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Simulation")
	int32 DefaultRandomSeed = 1337;

	/** Tracks the settings schema. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Version")
	int32 SettingsSchemaVersion = 1;
};
