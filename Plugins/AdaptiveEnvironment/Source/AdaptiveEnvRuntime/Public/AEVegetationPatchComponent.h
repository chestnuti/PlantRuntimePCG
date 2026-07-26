#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
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
	/* Receives health and deterministic density visibility updates. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Adaptive Environment|M7")
	TObjectPtr<UInstancedStaticMeshComponent> TargetInstances;

private:
	/* Converts one stable instance identity into a reproducible zero-to-one key. */
	double GetInstanceVisibilityKey(int32 InstanceIndex) const;

	uint32 RegistrationGeneration = 0;
	int32 RegisteredInstanceCount = 0;
	int32 NextPendingInstanceIndex = 0;
	TOptional<FAEVegetationPatchVisualCommand> PendingVisualCommand;
};
