#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AEM6Types.h"
#include "AEPathHeatmapRendererComponent.generated.h"

class ALandscapeProxy;
class UAEAdaptiveEnvWorldSubsystem;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UMeshComponent;
class UTextureRenderTarget2D;

/* Identifies one Mesh material slot that samples the shared M6 Render Target. */
USTRUCT(BlueprintType)
struct ADAPTIVEENVRUNTIME_API FAEPathMeshMaterialBinding
{
	GENERATED_BODY()

	/* Receives a runtime material instance with the documented M6 parameters. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Adaptive Environment|M6")
	TObjectPtr<UMeshComponent> MeshComponent;

	/* Selects one material slot on the target Mesh Component. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Adaptive Environment|M6", meta = (ClampMin = "0"))
	int32 MaterialSlotIndex = 0;
};

/* Applies immutable M6 path commands to a shared material-facing Render Target. */
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

	/* Initializes the Render Target and all configured material outputs. */
	bool InitializeVisualOutput(const FIntPoint& GridDimensions, const FBox2D& GridWorldBounds);
	/* Coalesces immutable M6 commands by destination Cell. */
	void EnqueueVisualCommands(TConstArrayView<FAEPathHeatmapVisualCommand> Commands);
	/* Applies a bounded number of queued commands to the Render Target. */
	void ApplyVisualBudget(int32 MaxCommands);
	/* Clears queued state and disables the material output. */
	void ResetVisualOutput();
	/* Returns the runtime Render Target supplied to every bound material. */
	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|M6")
	UTextureRenderTarget2D* GetPathHeatmapRenderTarget() const { return PathHeatmapRenderTarget; }
	/* Rebuilds Landscape and Mesh material bindings without resetting M6 state. */
	UFUNCTION(BlueprintCallable, Category = "Adaptive Environment|M6")
	bool RefreshMaterialBindings();
	/* Returns the number of Landscape or Mesh material outputs currently bound. */
	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|M6")
	int32 GetBoundMaterialOutputCount() const { return BoundMaterialOutputCount; }

	/* Optionally receives a Landscape whose material exposes the documented M6 parameters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Adaptive Environment|M6")
	TObjectPtr<ALandscapeProxy> TargetLandscape;
	/* Explicitly selects Mesh material slots that receive the shared M6 parameters. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Adaptive Environment|M6")
	TArray<FAEPathMeshMaterialBinding> TargetMeshMaterials;

private:
	struct FRuntimeMeshMaterialBinding
	{
		TWeakObjectPtr<UMeshComponent> MeshComponent;
		int32 MaterialSlotIndex = INDEX_NONE;
		TWeakObjectPtr<UMaterialInterface> OriginalMaterial;
		TWeakObjectPtr<UMaterialInstanceDynamic> DynamicMaterial;
	};

	/* Replaces all output bindings with the current Landscape and Mesh configuration. */
	int32 BindMaterialOutputs();
	/* Binds the M6 texture and spatial constants to the optional Landscape material. */
	bool BindLandscapeMaterialParameters();
	/* Creates validated dynamic material instances for explicit Mesh material slots. */
	int32 BindMeshMaterialParameters(int32& OutRejectedBindingCount);
	/* Applies the shared M6 texture contract to one runtime Mesh material. */
	void ApplyMeshMaterialParameters(UMaterialInstanceDynamic& Material) const;
	/* Verifies that one Mesh material declares the three required M6 parameters. */
	bool SupportsRequiredMeshMaterialParameters(const UMaterialInterface& Material) const;
	/* Disables material sampling without changing M6 numerical state. */
	void DisableMaterialOutput();
	/* Restores only Mesh slots still owned by this Renderer. */
	void ReleaseMeshMaterialBindings();
	/* Rebuilds the pending-index lookup after removing applied commands. */
	void RebuildPendingCommandLookup();

	/* Stores the runtime texture sampled through AE_PathHeatmapTexture. */
	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> PathHeatmapRenderTarget;
	/* Stores grid width and height used by row-major commands. */
	FIntPoint TextureDimensions = FIntPoint::ZeroValue;
	/* Encodes WorldMin XY and inverse WorldSize XY for material UV mapping. */
	FLinearColor GridTransform = FLinearColor::Black;
	/* Counts valid Landscape and Mesh outputs bound to the current Render Target. */
	int32 BoundMaterialOutputCount = 0;
	/* Tracks the Landscape that currently receives the material contract. */
	TWeakObjectPtr<ALandscapeProxy> BoundLandscape;
	/* Tracks the runtime Mesh material instances and their replaced materials. */
	TArray<FRuntimeMeshMaterialBinding> RuntimeMeshMaterialBindings;
	/* Stores latest queued command for each pending Cell in stable insertion order. */
	TArray<FAEPathHeatmapVisualCommand> PendingCommands;
	/* Maps one pending Cell index to its command-array position. */
	TMap<int32, int32> PendingCommandPositions;
};
