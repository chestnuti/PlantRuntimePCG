#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "AEM7Types.h"
#include "AEVegetationPatchComponent.generated.h"

class UAEAdaptiveEnvWorldSubsystem;
class UAEPlantVisualResponseProfile;
class UAEVegetationSpeciesResponseProfile;
class UInstancedStaticMeshComponent;

/* Registers one ISM or HISM Patch and applies budgeted M7 visual commands. */
UCLASS(ClassGroup = (AdaptiveEnvironment), meta = (BlueprintSpawnableComponent))
class ADAPTIVEENVRUNTIME_API UAEVegetationPatchComponent final : public UActorComponent
{
	GENERATED_BODY()

public:
	/* Creates a non-ticking visual consumer. */
	UAEVegetationPatchComponent();
	/* Generates missing runtime identity and registers with the World scheduler. */
	virtual void BeginPlay() override;
	/* Unregisters before the target instance component can be destroyed. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/* Builds one validated instance-weighted registration for the shared Grid. */
	bool BuildPatchRegistration(
		const FIntPoint& GridDimensions,
		const FBox2D& GridWorldBounds,
		FAEVegetationPatchRegistration& OutRegistration,
		FString& OutError);
	/* Coalesces the latest valid immutable command for this Patch. */
	void EnqueueVisualCommand(const FAEVegetationPatchVisualCommand& Command);
	/* Applies at most MaxInstanceUpdates through per-instance custom data. */
	int32 ApplyVisualBudget(int32 MaxInstanceUpdates);
	/* Clears pending visual work without changing the target instance collection. */
	void ResetVisualOutput();
	/* Assigns one same-Actor ISM/HISM as the runtime target without creating an inline component. */
	UFUNCTION(BlueprintCallable, Category = "Adaptive Environment|M7")
	bool SetTargetInstancesComponent(UInstancedStaticMeshComponent* InTargetInstances);
	/* Returns the currently resolved runtime ISM/HISM target. */
	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|M7")
	UInstancedStaticMeshComponent* GetTargetInstancesComponent() const
	{
		return ResolvedTargetInstances;
	}
	/* Supports legacy Blueprint property reads without driving runtime registration. */
	UFUNCTION(
		BlueprintGetter,
		meta = (
			DeprecatedFunction,
			DeprecationMessage =
				"Use GetTargetInstancesComponent."))
	/* Supports legacy Blueprint property reads without driving runtime registration. */
	UInstancedStaticMeshComponent* GetDeprecatedTargetInstances() const;
	/* Supports legacy Blueprint property writes without driving runtime registration. */
	UFUNCTION(
		BlueprintSetter,
		meta = (
			DeprecatedFunction,
			DeprecationMessage =
				"Use SetTargetInstancesComponent."))
	/* Supports legacy Blueprint property writes without driving runtime registration. */
	void SetDeprecatedTargetInstances(
		UInstancedStaticMeshComponent* InTargetInstances);
	/* Rebuilds the Patch registration after the target instance collection changes. */
	UFUNCTION(BlueprintCallable, Category = "Adaptive Environment|M7")
	void RefreshPatchRegistration();
	/* Returns the current stable Patch identity. */
	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|M7")
	FGuid GetPatchId() const { return PatchId; }

	/* Stores an editor-authored stable identity; a runtime identity is generated only when empty. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Adaptive Environment|M7|Identity")
	FGuid PatchId;
	/* Supplies the traceable species-response contract. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Adaptive Environment|M7")
	TObjectPtr<UAEVegetationSpeciesResponseProfile> SpeciesProfile;
	/* Supplies material-facing custom-data slot choices only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Adaptive Environment|M7")
	TObjectPtr<UAEPlantVisualResponseProfile> VisualProfile;
	/* Selects one ISM/HISM sibling component owned by the same Actor. */
	UPROPERTY(
		EditAnywhere,
		Category = "Adaptive Environment|M7",
		meta = (
			UseComponentPicker,
			AllowedClasses = "/Script/Engine.InstancedStaticMeshComponent"))
	FComponentReference TargetInstancesReference;

private:
	/* Resolves the runtime override or editor component reference to one same-Actor target. */
	bool ResolveTargetInstances(FString& OutError);
	/* Converts one stable instance identity into a reproducible zero-to-one key. */
	double GetInstanceVisibilityKey(int32 InstanceIndex) const;

	/* Retains legacy serialized data while preventing it from driving runtime registration. */
	UPROPERTY(
		BlueprintReadWrite,
		Category = "Adaptive Environment|M7",
		meta = (
			AllowPrivateAccess = "true",
			BlueprintGetter = "GetDeprecatedTargetInstances",
			BlueprintSetter = "SetDeprecatedTargetInstances",
			DeprecatedProperty,
			DeprecationMessage =
				"Use TargetInstancesReference or SetTargetInstancesComponent."))
	TObjectPtr<UInstancedStaticMeshComponent> TargetInstances_DEPRECATED;
	/* Stores the resolved target without serializing an inline component object. */
	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> ResolvedTargetInstances;
	/* Rejects visual commands created for an earlier registration of this component. */
	uint32 RegistrationGeneration = 0;
	/* Freezes the target instance count used by the current spatial registration. */
	int32 RegisteredInstanceCount = 0;
	/* Stores the next stable instance index to receive budgeted custom data. */
	int32 NextPendingInstanceIndex = 0;
	/* Coalesces the newest immutable visual command awaiting budgeted application. */
	TOptional<FAEVegetationPatchVisualCommand> PendingVisualCommand;
};
