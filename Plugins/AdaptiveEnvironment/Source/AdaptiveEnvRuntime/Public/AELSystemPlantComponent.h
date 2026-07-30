#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AEM8Types.h"
#include "AELSystemPlantComponent.generated.h"

class UAELSystemRuleAsset;
class UAEAdaptiveEnvWorldSubsystem;
class UBoxComponent;
class UDynamicMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UNiagaraComponent;
class UNiagaraSystem;

UCLASS(ClassGroup = (AdaptiveEnvironment), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class ADAPTIVEENVRUNTIME_API UAELSystemPlantComponent final : public UActorComponent
{
	GENERATED_BODY()

public:
	/* Creates one subsystem-driven M8 representative plant component. */
	UAELSystemPlantComponent();

	/* Selects manual, required-M7, or fallback state input. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M8|Input")
	EAELSystemInputMode InputMode = EAELSystemInputMode::M7WithManualFallback;
	/* Selects the stable M7 candidate queried in integrated modes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M8|Input")
	int64 SourceStablePointId = 0;
	/* Supplies independent lifecycle values when M7 is unavailable or bypassed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "M8|Input")
	FAELSystemManualState ManualState;
	/* Supplies deterministic grammar, geometry, and safety configuration. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M8|Generation")
	TObjectPtr<UAELSystemRuleAsset> RuleAsset;
	/* Supplies a per-plant deterministic seed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M8|Generation")
	int32 GenerationSeed = 1337;
	/* Builds the fixed initialized plant automatically during BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M8|Generation")
	bool bGenerateOnBeginPlay = true;
	/* Supplies the bark material applied to every initialized mesh module. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M8|Rendering")
	TObjectPtr<UMaterialInterface> BarkMaterial;
	/* Supplies the optional Niagara leaf renderer consuming M8 user parameters. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M8|Rendering")
	TObjectPtr<UNiagaraSystem> LeafSystem;
	/* Multiplies the Rule Asset leaf target for this plant instance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M8|Rendering", meta = (ClampMin = "0.0"))
	float LeafDensityScale = 1.0f;
	/* Starts soft-stem wilt only after leaf shedding reaches this normalized progress. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M8|Lifecycle", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SoftWiltStartProgressRatio = 0.8f;
	/* Converts health into the persistent-dead-wood threshold. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M8|Lifecycle", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DeadWoodHealthThreshold = 0.05f;

	/* Generates and submits one fixed plant without requiring upstream runtime input. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Adaptive Environment|M8")
	bool GeneratePreview(FString& OutError);
	/* Clears all initialized mesh, collision, material, leaf, and structural state. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Adaptive Environment|M8")
	void ClearGeneratedPlant();
	/* Detaches one initialized hard-branch module and preserves its broken state. */
	UFUNCTION(BlueprintCallable, Category = "Adaptive Environment|M8")
	bool BreakBranchModule(int64 BranchModuleId);
	/* Reads one persistent branch-module state. */
	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|M8")
	EAEBranchStructuralState GetBranchModuleState(int64 BranchModuleId) const;
	/* Returns the current logical branch-segment count. */
	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|M8")
	int32 GetBranchSegmentCount() const { return GeneratedPlant.BranchSegments.Num(); }
	/* Returns the current stable leaf-emitter count. */
	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|M8")
	int32 GetLeafEmitterCount() const { return GeneratedPlant.LeafEmitters.Num(); }
	/* Returns the deterministic logical plant hash. */
	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|M8")
	int64 GetGeneratedContentHash() const { return static_cast<int64>(GeneratedPlant.ContentHash & MAX_int64); }
	/* Returns whether a fixed mesh has been initialized. */
	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|M8")
	bool IsPlantGenerated() const { return bGenerated; }

	/* Advances M8 visual state from M7 or manual fallback without rebuilding mesh geometry. */
	void AdvanceM8(const UAEAdaptiveEnvWorldSubsystem& Subsystem);

protected:
	/* Registers the component and optionally creates its independent preview. */
	virtual void BeginPlay() override;
	/* Unregisters and destroys only runtime-owned visual components. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FBranchModuleRuntime
	{
		/* Stores the initialized stable module identity. */
		int64 ModuleId = 0;
		/* Owns simplified rigid-body collision for detachable modules. */
		TWeakObjectPtr<UBoxComponent> PhysicsRoot;
		/* Displays the fixed initialized module geometry. */
		TWeakObjectPtr<UDynamicMeshComponent> MeshComponent;
		/* Stores the per-module bark material state. */
		TWeakObjectPtr<UMaterialInstanceDynamic> Material;
		/* Stores persistent structural state independent from M7 recovery. */
		EAEBranchStructuralState StructuralState = EAEBranchStructuralState::Intact;
	};

	/* Resolves M7 or manual input into one complete visual-state contract. */
	FAELSystemResolvedPlantState ResolvePlantState(const UAEAdaptiveEnvWorldSubsystem* Subsystem) const;
	/* Submits generated plain mesh buffers to runtime-owned components once. */
	bool BuildFixedMeshComponents(FString& OutError);
	/* Creates or resets the optional Niagara leaf consumer. */
	void InitializeLeafSystem();
	/* Applies material and Niagara values without touching initialized mesh topology. */
	void ApplyResolvedVisualState(const FAELSystemResolvedPlantState& State);
	/* Releases one runtime-owned component through normal Unreal destruction. */
	static void DestroyOwnedComponent(UActorComponent* Component);

	/* Keeps runtime mesh components reachable by Unreal garbage collection. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UDynamicMeshComponent>> OwnedMeshComponents;
	/* Keeps runtime collision roots reachable by Unreal garbage collection. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBoxComponent>> OwnedPhysicsRoots;
	/* Keeps the optional runtime Niagara consumer reachable. */
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> LeafComponent;
	/* Maps stable module identities to runtime visual and structural state. */
	TMap<int64, FBranchModuleRuntime> BranchModules;
	/* Owns immutable logical and fixed mesh output after generation. */
	FAELSystemGeneratedPlant GeneratedPlant;
	/* Stores the last resolved M8 visual input. */
	FAELSystemResolvedPlantState LastResolvedState;
	/* Reports whether one fixed generated plant is active. */
	bool bGenerated = false;
};

