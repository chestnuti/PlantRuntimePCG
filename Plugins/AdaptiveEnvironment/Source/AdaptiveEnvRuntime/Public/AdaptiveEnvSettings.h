#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "AdaptiveEnvSettings.generated.h"

class UAEPublishedParameterBundleAsset;

UCLASS(Config = AdaptiveEnvironment, DefaultConfig, meta = (DisplayName = "Adaptive Environment"))
class ADAPTIVEENVRUNTIME_API UAdaptiveEnvSettings final : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/* Creates the project settings section. */
	UAdaptiveEnvSettings();

	/* Returns the Project Settings category that contains this section. */
	virtual FName GetCategoryName() const override;

	/* Enables runtime updates in supported worlds. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Runtime")
	bool bEnableRuntime = true;

	/* Enables M3 Exposure when a valid parameter bundle is configured. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Parameters")
	bool bEnableM3 = true;

	/* Enables M4 constraint and state decisions from the same validated parameter snapshot. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Parameters")
	bool bEnableM4 = true;

	/* Enables M5 impact fusion and ecological response from the validated bundle. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Parameters")
	bool bEnableM5 = true;

	/* References the single published bundle that atomically supplies M3, M4, and M5 parameters. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Parameters")
	TSoftObjectPtr<UAEPublishedParameterBundleAsset> ParameterBundle;

	/* Controls behaviour sampling frequency in samples per second. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Runtime", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float BehaviourSampleRateHz = 10.0f;

	/* Controls evaluation snapshot frequency in snapshots per second. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Runtime", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float EvaluationRateHz = 1.0f;

	/* Defines the initial grid width in cells. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Grid", meta = (ClampMin = "1", UIMin = "1"))
	int32 GridWidth = 128;

	/* Defines the initial grid height in cells. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Grid", meta = (ClampMin = "1", UIMin = "1"))
	int32 GridHeight = 128;

	/* Defines one grid cell in centimetres. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Grid", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float CellSizeCm = 100.0f;

	/* Defines the grid centre in world XY. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Grid")
	FVector2D GridWorldCenter = FVector2D::ZeroVector;

	/* Limits fixed behaviour steps after a long frame. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Behaviour", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxBehaviourSubstepsPerFrame = 3;

	/* Starts dwell state below this speed in centimetres per second. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Behaviour", meta = (ClampMin = "0.0"))
	float DwellEnterSpeedCmPerSecond = 10.0f;

	/* Ends dwell state above this speed in centimetres per second. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Behaviour", meta = (ClampMin = "0.0"))
	float DwellExitSpeedCmPerSecond = 20.0f;

	/* Marks movement as sprint above this speed in centimetres per second. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Behaviour", meta = (ClampMin = "0.0"))
	float SprintSpeedCmPerSecond = 500.0f;

	/* Defines the activity smoothing radius in cells. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Grid", meta = (ClampMin = "0"))
	int32 ActivityKernelRadiusCells = 1;

	/* Defines the Gaussian activity kernel standard deviation in cells. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Grid", meta = (ClampMin = "0.01"))
	float ActivityKernelSigma = 0.75f;

	/* Controls debug view refresh frequency in updates per second. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Debug", meta = (ClampMin = "0.1"))
	float DebugRefreshRateHz = 5.0f;

	/* Limits cells drawn in one debug refresh. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Debug", meta = (ClampMin = "1"))
	int32 MaxDebugCells = 2048;

	/* Limits numeric labels per refresh below Unreal's shared per-Actor debug-text cap. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Debug", meta = (ClampMin = "0", ClampMax = "96"))
	int32 MaxDebugTextLabels = 96;

	/* Expands each recently changed Cell by this Chebyshev neighbourhood radius. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Debug", meta = (ClampMin = "0", ClampMax = "8"))
	int32 DebugActiveNeighbourRadiusCells = 1;

	/* Limits debug drawing around the renderer owner in centimetres. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Debug", meta = (ClampMin = "0.0"))
	float DebugDrawRadiusCm = 5000.0f;

	/* Defines simulated hours advanced by one real second. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Simulation", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float SimulationHoursPerRealSecond = 6.0f;

	/* Defines the vertical half-length used to trace ground at an M4 Cell centre in centimetres. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "M4", meta = (ClampMin = "1.0"))
	float M4GroundTraceHalfHeightCm = 100000.0f;

	/* Supplies normalized moisture when no registered source contains an M4 Cell. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "M4", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float M4DefaultMoistureRatio = 0.5f;

	/* Provides a deterministic default seed. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Simulation")
	int32 DefaultRandomSeed = 1337;

	/* Identifies the serialized settings schema version. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Version")
	int32 SettingsSchemaVersion = 8;
};
