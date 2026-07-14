#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AdaptiveEnvTypes.h"
#include "AEHeatmapRendererComponent.generated.h"

class UAEAdaptiveEnvWorldSubsystem;

UCLASS(ClassGroup = (AdaptiveEnvironment), meta = (BlueprintSpawnableComponent))
class ADAPTIVEENVRUNTIME_API UAEHeatmapRendererComponent final : public UActorComponent
{
	GENERATED_BODY()

public:
	/** Creates a renderer driven by the World Subsystem debug refresh. */
	UAEHeatmapRendererComponent();

	/** Registers this renderer with the World Subsystem. */
	virtual void BeginPlay() override;
	/** Unregisters this renderer before its owner leaves play. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Draws visible behaviour cells for the requested duration in seconds. */
	void RenderDebug(const UAEAdaptiveEnvWorldSubsystem& Subsystem, float DurationSeconds) const;

	/** Selects the displayed behaviour layer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	EAEHeatmapDebugMode Mode = EAEHeatmapDebugMode::SmoothedActivity;

	/** Ignores values below this threshold. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (ClampMin = "0.0"))
	float MinimumValue = 0.01f;

	/** Uses this value for stable colour normalization. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (ClampMin = "0.0"))
	float FixedMaximumValue = 10.0f;

	/** Draws numeric values above visible cells. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bDrawValues = false;

	/** Raises debug geometry above the ground. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	float DrawHeightCm = 20.0f;

private:
	/** Reads the selected debug metric from a cell snapshot. */
	float GetDisplayValue(const FAEBehaviourCellSnapshot& Snapshot) const;
};
