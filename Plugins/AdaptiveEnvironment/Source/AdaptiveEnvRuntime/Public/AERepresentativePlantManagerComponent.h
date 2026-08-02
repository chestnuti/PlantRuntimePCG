#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AERepresentativePlantManagerComponent.generated.h"

class UAEAdaptiveEnvWorldSubsystem;
class AActor;

UCLASS(ClassGroup = (AdaptiveEnvironment), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class ADAPTIVEENVRUNTIME_API UAERepresentativePlantManagerComponent final : public UActorComponent
{
	GENERATED_BODY()

public:
	/* Creates a non-ticking M8 neighborhood activation manager. */
	UAERepresentativePlantManagerComponent();

	/* Supplies the Actor class containing one M8 L-System plant component. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M8|Neighborhood Activation")
	TSubclassOf<AActor> RepresentativePlantActorClass;
	/* Defines the player-centered activation radius in centimetres. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M8|Neighborhood Activation", meta = (ClampMin = "0.0", Units = "cm"))
	float ActivationRadiusCm = 1500.0f;
	/* Caps persistent M8 Actors created by this manager. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M8|Neighborhood Activation", meta = (ClampMin = "1", ClampMax = "256"))
	int32 MaxActivePlants = 32;
	/* Caps new M8 Actors created during one selection pass. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M8|Neighborhood Activation", meta = (ClampMin = "1", ClampMax = "32"))
	int32 MaxActivationsPerPass = 2;
	/* Defines the fixed-step interval between neighborhood selection passes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M8|Neighborhood Activation", meta = (ClampMin = "0.01", Units = "s"))
	float SelectionIntervalSeconds = 0.25f;

	/* Returns the number of successfully activated persistent M8 Actors. */
	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|M8")
	int32 GetActiveRepresentativePlantCount() const;

	/* Advances player-neighborhood selection from the World subsystem fixed step. */
	void AdvanceNeighborhoodActivation(const UAEAdaptiveEnvWorldSubsystem& Subsystem, float StepSeconds);

protected:
	/* Registers this manager with its World subsystem. */
	virtual void BeginPlay() override;
	/* Unregisters and destroys only Actors owned by this manager during World teardown. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/* Spawns and binds one persistent M8 Actor for a selected M7 snapshot. */
	bool ActivateSnapshot(const struct FAEPlantInstanceSnapshot& Snapshot);

	/* Keeps manager-owned representative Actors referenced for their complete lifetime. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> ActiveRepresentativePlants;
	/* Prevents duplicate activation of one stable M7 identity. */
	TSet<int64> ActiveStablePointIds;
	/* Accumulates fixed simulation time until the next selection pass. */
	double SelectionAccumulatorSeconds = 0.0;
};
