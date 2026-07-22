#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AEM3Types.h"
#include "AEM4Types.h"
#include "AEM5Types.h"
#include "AdaptiveEnvTypes.h"
#include "AEHeatmapRendererComponent.generated.h"

class UAEAdaptiveEnvWorldSubsystem;

UCLASS(ClassGroup = (AdaptiveEnvironment), meta = (BlueprintSpawnableComponent))
class ADAPTIVEENVRUNTIME_API UAEHeatmapRendererComponent final : public UActorComponent
{
	GENERATED_BODY()

public:
	/* Creates a renderer driven by the World Subsystem debug refresh. */
	UAEHeatmapRendererComponent();

	/* Registers this renderer with the World Subsystem. */
	virtual void BeginPlay() override;
	/* Unregisters this renderer before its owner leaves play. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/* Draws visible behaviour cells for the requested duration in seconds. */
	void RenderDebug(const UAEAdaptiveEnvWorldSubsystem& Subsystem, float DurationSeconds) const;

	/* Selects the displayed M1 behaviour or M3 ecological layer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	EAEHeatmapDebugMode Mode = EAEHeatmapDebugMode::SmoothedActivity;

	/* Ignores values below this threshold. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (ClampMin = "0.0"))
	float MinimumValue = 0.01f;

	/* Uses this value for stable colour normalization. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (ClampMin = "0.0"))
	float FixedMaximumValue = 10.0f;

	/* Uses parameter-derived ranges for bounded M3 modes when available. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bUseModeDefaultRange = true;

	/* Draws numeric values above visible cells. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bDrawValues = false;

	/* Raises debug geometry above the ground. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	float DrawHeightCm = 20.0f;

private:
	/* Returns whether the selected layer consumes M3 snapshots. */
	bool IsM3Mode() const;
	/* Returns whether the selected layer consumes M4 snapshots. */
	bool IsM4Mode() const;
	/* Returns whether the selected layer consumes M5 snapshots. */
	bool IsM5Mode() const;
	/* Reads the selected M1 metric from one behavior snapshot. */
	float GetBehaviourDisplayValue(const FAEBehaviourCellSnapshot& Snapshot) const;
	/* Reads the selected M3 metric from one ecological snapshot. */
	float GetM3DisplayValue(const FAEM3CellSnapshot& Snapshot) const;
	/* Reads the selected M4 metric from one constraint snapshot. */
	float GetM4DisplayValue(const FAEEnvironmentConstraintSnapshot& Snapshot) const;
	/* Reads the selected M5 metric from one response snapshot. */
	float GetM5DisplayValue(const FAEEcologicalResponseSnapshot& Snapshot) const;
};
