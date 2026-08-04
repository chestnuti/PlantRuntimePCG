#pragma once

#include "CoreMinimal.h"
#include "AEM8Types.h"
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

	/* Maps M7 species identities to representative actor classes and class budgets. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M8|Neighborhood Activation")
	TArray<FAEM8RepresentativePlantBinding> PlantBindings;
	/* Defines the player-centered activation radius in centimetres. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M8|Neighborhood Activation", meta = (ClampMin = "0.0", Units = "cm"))
	float ActivationRadiusCm = 1500.0f;
	/* Defines the larger release-request radius in centimetres. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M8|Neighborhood Activation", meta = (ClampMin = "0.0", Units = "cm"))
	float DeactivationRadiusCm = 2000.0f;
	/* Caps active representatives across every configured species. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M8|Neighborhood Activation", meta = (ClampMin = "1", ClampMax = "256"))
	int32 MaxActivePlants = 32;
	/* Caps new M8 Actors created during one selection pass. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M8|Neighborhood Activation", meta = (ClampMin = "1", ClampMax = "32"))
	int32 MaxActivationsPerPass = 2;
	/* Caps release requests processed during one neighborhood pass. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M8|Neighborhood Activation", meta = (ClampMin = "1", ClampMax = "64"))
	int32 MaxReleaseRequestsPerPass = 4;
	/* Caps expired branch component destruction during one fixed step. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M8|Neighborhood Activation", meta = (ClampMin = "1", ClampMax = "128"))
	int32 MaxDebrisCleanupsPerStep = 8;
	/* Prevents immediate release after a newly activated plant crosses the boundary. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M8|Neighborhood Activation", meta = (ClampMin = "0.0", Units = "s"))
	float MinimumActiveSeconds = 1.0f;
	/* Defines the fixed-step interval between neighborhood selection passes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M8|Neighborhood Activation", meta = (ClampMin = "0.01", Units = "s"))
	float SelectionIntervalSeconds = 0.25f;

	/* Returns the number of successfully activated persistent M8 Actors. */
	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|M8")
	int32 GetActiveRepresentativePlantCount() const;
	/* Returns actors retained only because detached branch geometry is still alive. */
	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|M8")
	int32 GetWaitingForDebrisCount() const;

	/* Advances player-neighborhood selection from the World subsystem fixed step. */
	void AdvanceNeighborhoodActivation(UAEAdaptiveEnvWorldSubsystem& Subsystem, float StepSeconds);

protected:
	/* Registers this manager with its World subsystem. */
	virtual void BeginPlay() override;
	/* Unregisters and destroys only Actors owned by this manager during World teardown. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FManagedPlantEntry
	{
		/* References the subsystem-owned recyclable actor shell. */
		TWeakObjectPtr<AActor> Actor;
		/* References the actor's M8 runtime component. */
		TWeakObjectPtr<class UAELSystemPlantComponent> PlantComponent;
		/* Identifies the species binding that acquired this shell. */
		FName SpeciesId = NAME_None;
		/* Identifies the exact pooled Blueprint class. */
		TSubclassOf<AActor> ActorClass;
		/* Stores the current pool transition state. */
		EAEM8PoolEntryState State = EAEM8PoolEntryState::Available;
		/* Stores fixed simulation time when activation completed. */
		double ActivationTimeSeconds = 0.0;
	};

	/* Activates one selected snapshot through its class-specific World pool. */
	bool ActivateSnapshot(UAEAdaptiveEnvWorldSubsystem& Subsystem, const FAEPlantInstanceSnapshot& Snapshot, const FAEM8RepresentativePlantBinding& Binding);
	/* Returns one actor immediately or places it in delayed debris release. */
	void RequestRelease(UAEAdaptiveEnvWorldSubsystem& Subsystem, int64 StablePointId, FManagedPlantEntry& Entry);
	/* Resolves one enabled unique binding for an M7 species identity. */
	const FAEM8RepresentativePlantBinding* FindBinding(FName SpeciesId) const;
	/* Counts active entries owned by one M7 species. */
	int32 CountActiveSpecies(FName SpeciesId) const;
	/* Stores active and debris-waiting plants by stable M7 identity. */
	TMap<int64, FManagedPlantEntry> ManagedPlants;
	/* Accumulates fixed simulation time until the next selection pass. */
	double SelectionAccumulatorSeconds = 0.0;
	/* Reports whether authored class prewarm requests were submitted once. */
	bool bPoolPrewarmed = false;
};
