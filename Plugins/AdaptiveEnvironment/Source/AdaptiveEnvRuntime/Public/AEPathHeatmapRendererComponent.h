#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AEM6Types.h"
#include "AEPathHeatmapRendererComponent.generated.h"

class ALandscapeProxy;
class UAEAdaptiveEnvWorldSubsystem;
class UTextureRenderTarget2D;

/* Applies immutable M6 path commands to a Landscape-bound Render Target. */
UCLASS(ClassGroup = (AdaptiveEnvironment), meta = (BlueprintSpawnableComponent))
class ADAPTIVEENVRUNTIME_API UAEPathHeatmapRendererComponent final : public UActorComponent
{
	GENERATED_BODY()

public:
	/* Creates a non-ticking visual consumer. */
	UAEPathHeatmapRendererComponent();
	/* Registers this renderer with the World Subsystem. */
	virtual void BeginPlay() override;
	/* Disables material output and unregisters before teardown. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/* Initializes the Render Target and Landscape material binding. */
	bool InitializeVisualOutput(const FIntPoint& GridDimensions, const FBox2D& GridWorldBounds);
	/* Coalesces immutable M6 commands by destination Cell. */
	void EnqueueVisualCommands(TConstArrayView<FAEPathHeatmapVisualCommand> Commands);
	/* Applies a bounded number of queued commands to the Render Target. */
	void ApplyVisualBudget(int32 MaxCommands);
	/* Clears queued state and disables the material output. */
	void ResetVisualOutput();
	/* Returns the runtime Render Target supplied to the Landscape material. */
	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|M6")
	UTextureRenderTarget2D* GetPathHeatmapRenderTarget() const { return PathHeatmapRenderTarget; }

	/* Receives the Landscape whose material exposes the documented M6 parameters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Adaptive Environment|M6")
	TObjectPtr<ALandscapeProxy> TargetLandscape;

private:
	/* Binds the M6 texture and spatial constants to the target Landscape material. */
	bool BindLandscapeMaterialParameters();
	/* Disables material sampling without changing M6 numerical state. */
	void DisableMaterialOutput();
	/* Rebuilds the pending-index lookup after removing applied commands. */
	void RebuildPendingCommandLookup();

	/* Stores the runtime texture sampled through AE_PathHeatmapTexture. */
	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> PathHeatmapRenderTarget;
	/* Stores grid width and height used by row-major commands. */
	FIntPoint TextureDimensions = FIntPoint::ZeroValue;
	/* Encodes WorldMin XY and inverse WorldSize XY for material UV mapping. */
	FLinearColor GridTransform = FLinearColor::Black;
	/* Stores latest queued command for each pending Cell in stable insertion order. */
	TArray<FAEPathHeatmapVisualCommand> PendingCommands;
	/* Maps one pending Cell index to its command-array position. */
	TMap<int32, int32> PendingCommandPositions;
};
