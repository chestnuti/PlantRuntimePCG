#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AEM7Types.h"
#include "AEVegetationDistributionComponent.generated.h"

class UAEAdaptiveEnvWorldSubsystem;
class UAEPlantSpeciesProfile;
class UHierarchicalInstancedStaticMeshComponent;

UCLASS(ClassGroup = (AdaptiveEnvironment), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class ADAPTIVEENVRUNTIME_API UAEVegetationDistributionComponent final : public UActorComponent
{
	GENERATED_BODY()

public:
	/* Creates a World-subsystem-driven distribution component. */
	UAEVegetationDistributionComponent();

	/* Lists versioned species profiles owned by this distribution. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M7|Distribution")
	TArray<TObjectPtr<UAEPlantSpeciesProfile>> SpeciesProfiles;
	/* Controls deterministic candidate positions and identities. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M7|Distribution")
	int32 DistributionSeed = 1337;
	/* Caps memory and construction cost for each species. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M7|Distribution", meta = (ClampMin = "1"))
	int32 MaxCandidatesPerSpecies = 50000;

	/* Explicitly rebuilds after structural settings change. */
	UFUNCTION(BlueprintCallable, Category = "Adaptive Environment|M7")
	bool RebuildStructuralDistribution();
	/* Resolves one M8-facing immutable instance snapshot. */
	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|M7")
	bool GetPlantInstanceState(int64 StablePointId, FAEPlantInstanceSnapshot& OutSnapshot) const;
	/* Returns every M7 snapshot sorted by stable identity for Blueprint selection. */
	UFUNCTION(BlueprintCallable, Category = "Adaptive Environment|M7")
	void GetPlantInstanceStates(TArray<FAEPlantInstanceSnapshot>& OutSnapshots) const;
	/* Hides or restores one M7 instance while an M8 representative owns its display. */
	bool SetM8RepresentativeOverride(int64 StablePointId, bool bM8Active);
	/* Reports immutable candidates across all species. */
	UFUNCTION(BlueprintPure, Category = "Adaptive Environment|M7")
	int32 GetStableCandidateCount() const;
	/* Collects unique row-major Cells containing at least one projected candidate. */
	void GetOccupiedCellIndices(TArray<int32>& OutCellIndices) const;

	/* Initializes the distribution from the shared Grid and ecological ground contracts. */
	void InitializeDistribution(
		const FIntPoint& GridDimensions,
		const FBox2D& GridBounds,
		float GroundTraceHalfHeightCm);
	/* Advances source Dirty, distribution Dirty, and active lifecycle Cells. */
	void AdvanceM7(
		const UAEAdaptiveEnvWorldSubsystem& Subsystem,
		const TArray<int32>& M5DirtyCellIndices,
		const TArray<int32>& DistributionDirtyCellIndices,
		double DeltaSimulationHours,
		int64 SimulationStep);
	/* Applies queued HISM changes within one frame budget. */
	void ApplyVisualBudget(int32 MaximumUpdates);

protected:
	/* Registers this component with its World subsystem. */
	virtual void BeginPlay() override;
	/* Unregisters and destroys self-owned visual components. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FSpeciesRuntime
	{
		/* References the immutable species configuration without owning it. */
		TWeakObjectPtr<UAEPlantSpeciesProfile> Profile;
		/* Owns the runtime HISM created for this species. */
		TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Instances = nullptr;
		/* Stores the immutable maximum-density candidate pool. */
		TArray<FAEM7CandidatePoint> Candidates;
		/* Stores immutable visible transforms before runtime visibility scaling. */
		TArray<FTransform> BaseWorldTransforms;
		/* Stores mutable public state aligned one-to-one with candidates. */
		TArray<FAEPlantInstanceSnapshot> Snapshots;
		/* Marks candidates that have consumed their first effective M4/M5 input or intact baseline. */
		TBitArray<> InitializedHealth;
		/* Groups candidate indices by shared row-major Cell index. */
		TArray<TArray<int32>> CandidateIndicesByCell;
		/* Tracks Cells whose health has not reached its current target. */
		TSet<int32> ActiveTransitionCells;
		/* Tracks Cells whose distribution inputs require reevaluation. */
		TSet<int32> DistributionDirtyCells;
		/* Coalesces candidate visual writes until the frame budget applies them. */
		TSet<int32> PendingVisualIndices;
	};

	/* Builds one stable species pool and HISM owner. */
	bool BuildSpeciesRuntime(UAEPlantSpeciesProfile& Profile, FSpeciesRuntime& OutRuntime, int32 SpeciesIndex);
	/* Destroys only dynamically owned HISM components. */
	void DestroyOwnedInstances();
	/* Maps one shared Cell coordinate to row-major storage. */
	int32 CoordinateToIndex(const FIntPoint& Coordinate) const;

	/* Caches shared Grid width and height for row-major mapping. */
	FIntPoint CachedGridDimensions = FIntPoint::ZeroValue;
	/* Caches shared world XY bounds used by structural generation. */
	FBox2D CachedGridBounds = FBox2D(EForceInit::ForceInit);
	/* Caches the vertical ecological ground trace half-height in centimetres. */
	float CachedGroundTraceHalfHeightCm = 0.0f;
	/* Stores one owned runtime record per accepted species profile. */
	TArray<FSpeciesRuntime> SpeciesRuntime;
	/* Maps stable identities to species and candidate array indices. */
	TMap<int64, TPair<int32, int32>> StablePointLookup;
	/* Stores stable identities whose M7 HISM representation is replaced by M8. */
	TSet<int64> M8RepresentativeOverrideIds;
	/* Reports whether at least one valid species runtime was built. */
	bool bInitialized = false;
};
