#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "AdaptiveEnvSettings.generated.h"

class UAEAdaptiveEnvironmentProfile;

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

	/* Enables the complete M3 through M5 adaptive ecology pipeline. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Adaptive Ecology")
	bool bEnableAdaptiveEcology = true;

	/* Enables M6 path visual state and registered material outputs when M5 is active. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "M6")
	bool bEnableM6 = true;

	/* Enables deterministic M7 vegetation distribution when M4 and M5 are active. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "M7")
	bool bEnableM7 = true;

	/* Enables registered M8 representative plants, including manual fallback mode. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "M8")
	bool bEnableM8 = true;

	/* References the single product profile that atomically configures M3, M4, and M5. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Adaptive Ecology")
	TSoftObjectPtr<UAEAdaptiveEnvironmentProfile> EnvironmentProfile;

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

	/* Starts the M6 path target above this normalized M5 Damage value. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "M6|State", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float M6VisibleDamageThresholdRatio = 0.15f;

	/* Reaches a full M6 path target at this normalized M5 Damage value. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "M6|State", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float M6FullPathDamageThresholdRatio = 0.65f;

	/* Advances M6 path formation per simulation hour. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "M6|State", meta = (ClampMin = "0.0"))
	float M6FormationRatePerSimulationHour = 0.20f;

	/* Advances M6 path fading per simulation hour. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "M6|State", meta = (ClampMin = "0.0"))
	float M6FadeRatePerSimulationHour = 0.08f;

	/* Suppresses M6 state revisions below this normalized intensity difference. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "M6|State", meta = (ClampMin = "0.000001", ClampMax = "1.0"))
	float M6DirtyIntensityEpsilon = 0.0039215686f;

	/* Controls how often queued M6 commands may reach the Render Target. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "M6|Renderer", meta = (ClampMin = "0.1"))
	float M6VisualApplyRateHz = 5.0f;

	/* Limits M6 Cell texture updates applied during one visual refresh. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "M6|Renderer", meta = (ClampMin = "1"))
	int32 M6MaxVisualCommandsPerFrame = 1024;

	/* Allocates this many visual pixels along one M6 Cell edge. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "M6|Renderer", meta = (ClampMin = "1", ClampMax = "8"))
	int32 M6VisualPixelsPerCell = 4;

	/* Defines the reconstructed path half-width in world centimetres. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "M6|Renderer", meta = (ClampMin = "1.0"))
	float M6PathHalfWidthCm = 35.0f;

	/* Defines each Flow-oriented segment half-length in Cell units. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "M6|Renderer", meta = (ClampMin = "0.5", ClampMax = "1.0"))
	float M6SegmentHalfLengthCells = 0.75f;

	/* Limits M7 instance custom-data and transform writes per registered component and frame. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "M7|Renderer", meta = (ClampMin = "1"))
	int32 M7MaxInstanceUpdatesPerFrame = 2048;

	/* Caps registered representative M8 plants advanced per fixed step. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "M8", meta = (ClampMin = "1", ClampMax = "32"))
	int32 M8MaxPlantsPerStep = 5;

	/* Names the Landscape texture parameter that receives the M6 Render Target. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "M6|Material")
	FName M6PathTextureParameterName = TEXT("AE_PathHeatmapTexture");

	/* Names the Landscape vector parameter storing WorldMin XY and inverse WorldSize XY. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "M6|Material")
	FName M6GridTransformParameterName = TEXT("AE_PathGridTransform");

	/* Names the Landscape scalar parameter that enables M6 texture sampling. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "M6|Material")
	FName M6EnabledParameterName = TEXT("AE_PathHeatmapEnabled");

	/* Provides a deterministic default seed. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Simulation")
	int32 DefaultRandomSeed = 1337;

	/* Identifies the serialized settings schema version. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Version")
	int32 SettingsSchemaVersion = 14;
};
