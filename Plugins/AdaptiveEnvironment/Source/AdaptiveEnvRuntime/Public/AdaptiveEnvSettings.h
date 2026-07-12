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

	/** Defines the grid centre in world XY. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Grid")
	FVector2D GridWorldCenter = FVector2D::ZeroVector;

	/** Limits fixed behaviour steps after a long frame. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Behaviour", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxBehaviourSubstepsPerFrame = 3;

	/** Starts dwell state below this speed. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Behaviour", meta = (ClampMin = "0.0"))
	float DwellEnterSpeedCmPerSecond = 10.0f;

	/** Ends dwell state above this speed. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Behaviour", meta = (ClampMin = "0.0"))
	float DwellExitSpeedCmPerSecond = 20.0f;

	/** Marks movement as sprint above this speed. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Behaviour", meta = (ClampMin = "0.0"))
	float SprintSpeedCmPerSecond = 500.0f;

	/** Defines the activity smoothing radius. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Grid", meta = (ClampMin = "0"))
	int32 ActivityKernelRadiusCells = 1;

	/** Defines the Gaussian activity kernel width. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Grid", meta = (ClampMin = "0.01"))
	float ActivityKernelSigma = 0.75f;

	/** Controls debug view refresh frequency. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Debug", meta = (ClampMin = "0.1"))
	float DebugRefreshRateHz = 5.0f;

	/** Limits cells drawn in one debug refresh. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Debug", meta = (ClampMin = "1"))
	int32 MaxDebugCells = 2048;

	/** Limits debug drawing around the renderer owner. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Debug", meta = (ClampMin = "0.0"))
	float DebugDrawRadiusCm = 5000.0f;

	/** Converts real seconds to simulated hours. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Simulation", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float SimulationHoursPerRealSecond = 6.0f;

	/** Provides a deterministic default seed. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Simulation")
	int32 DefaultRandomSeed = 1337;

	/** Tracks the settings schema. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Version")
	int32 SettingsSchemaVersion = 2;
};
