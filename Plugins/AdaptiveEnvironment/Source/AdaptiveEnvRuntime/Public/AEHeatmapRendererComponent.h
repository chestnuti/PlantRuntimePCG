#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AEM3Types.h"
#include "AEM4Types.h"
#include "AEM5Types.h"
#include "AdaptiveEnvTypes.h"
#include "AEHeatmapRendererComponent.generated.h"

class UAEAdaptiveEnvWorldSubsystem;
class UAEPlantBiomeMapAsset;

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

	/* Selects the displayed behaviour, ecology, moisture, or biome layer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	EAEHeatmapDebugMode Mode = EAEHeatmapDebugMode::SmoothedActivity;

	/* Selects the M7 biome map used by biome debug modes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug|Biome")
	TObjectPtr<UAEPlantBiomeMapAsset> DebugBiomeMap;

	/* Selects the named biome shown by BiomeWeight mode. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug|Biome")
	FName DebugBiomeId = TEXT("Default");

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

	/* Evaluates selected and dominant weights without sampling a texture. */
	static bool EvaluateBiomeDebugValue(
		const UAEPlantBiomeMapAsset* BiomeMap,
		FName SelectedBiomeId,
		float ScalarValue,
		float& OutSelectedWeight,
		FName& OutDominantBiomeId,
		float& OutDominantWeight);

	/* Returns one stable debug colour from a biome identity. */
	static FLinearColor GetBiomeDebugColor(FName BiomeId);

private:
	/* Returns whether the selected layer consumes M3 snapshots. */
	bool IsM3Mode() const;
	/* Returns whether the selected layer consumes M4 snapshots. */
	bool IsM4Mode() const;
	/* Returns whether the selected layer consumes M5 snapshots. */
	bool IsM5Mode() const;
	/* Returns whether the selected layer consumes an authored M7 biome map. */
	bool IsBiomeMode() const;
	/* Reads the selected M1 metric from one behavior snapshot. */
	float GetBehaviourDisplayValue(const FAEBehaviourCellSnapshot& Snapshot) const;
	/* Reads the selected M3 metric from one ecological snapshot. */
	float GetM3DisplayValue(const FAEM3CellSnapshot& Snapshot) const;
	/* Reads the selected M4 metric from one constraint snapshot. */
	float GetM4DisplayValue(const FAEEnvironmentConstraintSnapshot& Snapshot) const;
	/* Reads the selected M5 metric from one response snapshot. */
	float GetM5DisplayValue(const FAEEcologicalResponseSnapshot& Snapshot) const;
	/* Invalidates cached biome scalar samples when the map or Grid contract changes. */
	void PrepareBiomeDebugCache(const UAEAdaptiveEnvWorldSubsystem& Subsystem) const;
	/* Samples one biome scalar value once per shared Grid Cell and map revision. */
	bool GetCachedBiomeScalarValue(const FAEEnvironmentConstraintSnapshot& Snapshot, float& OutValue) const;

	mutable TMap<int32, float> BiomeScalarValuesByCell;
	mutable TWeakObjectPtr<UAEPlantBiomeMapAsset> CachedBiomeMap;
	mutable int32 CachedBiomeRevision = 0;
	mutable FIntPoint CachedBiomeGridDimensions = FIntPoint::ZeroValue;
	mutable FBox2D CachedBiomeGridBounds;
};
