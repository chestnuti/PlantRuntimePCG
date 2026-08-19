#pragma once

#include "CoreMinimal.h"
#include "AEM7Types.generated.h"

UENUM(BlueprintType)
enum class EAEPlantLifecycleState : uint8
{
	/* Marks initial health advancement toward the first target. */
	Growing,
	/* Marks health within the configured target tolerance. */
	Stable,
	/* Marks health moving toward a lower Damage-derived target. */
	Declining,
	/* Marks non-initial health moving toward a higher target. */
	Recovering,
	/* Marks health at or below the terminal threshold. */
	Dead
};

USTRUCT(BlueprintType)
struct ADAPTIVEENVRUNTIME_API FAEPlantInstanceSnapshot
{
	GENERATED_BODY()

	/* Stores the deterministic cross-frame plant identity. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M7") int64 StablePointId = 0;
	/* Identifies the species profile that owns this plant. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M7") FName SpeciesId = NAME_None;
	/* Stores the shared Grid Cell coordinate. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M7") FIntPoint CellCoordinate = FIntPoint::ZeroValue;
	/* Stores the immutable candidate position in world centimetres. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M7") FVector WorldLocation = FVector::ZeroVector;
	/* Reports current plant health in the zero-to-one range. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M7") float HealthRatio = 1.0f;
	/* Reports the current combined distribution probability. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M7") float DistributionRatio = 0.0f;
	/* Reports the selected LUT or manual environment response before biome weighting. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M7") float EnvironmentSuitabilityRatio = 1.0f;
	/* Reports the species recovery rate after final suitability scaling. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M7") float EffectiveRecoveryRatePerSimulationHour = 0.0f;
	/* Reports whether the stable candidate is currently active. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M7") bool bVisible = false;
	/* Reports the current lifecycle transition state. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M7") EAEPlantLifecycleState LifecycleState = EAEPlantLifecycleState::Growing;
	/* Reports normalized progress for downstream animation. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M7") float LifecycleProgressRatio = 0.0f;
	/* Reports real-time visual retirement progress from visible to fully masked. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M7") float DeathFadeRatio = 0.0f;
	/* Identifies the fixed simulation step that produced this state. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M7") int64 SimulationStep = 0;
};

struct ADAPTIVEENVRUNTIME_API FAEM7CandidatePoint
{
	/* Stores the deterministic positive identity used by snapshots and M8. */
	uint64 StablePointId = 0;
	/* Stores the immutable candidate world position in centimetres. */
	FVector Location = FVector::ZeroVector;
	/* Stores the normalized ecological ground normal resolved during structural projection. */
	FVector SurfaceNormal = FVector::UpVector;
	/* Stores the row-major Grid Cell coordinate containing the point. */
	FIntPoint CellCoordinate = FIntPoint::ZeroValue;
	/* Stores the deterministic unit key used for nested density selection. */
	float SelectionKey = 0.0f;
	/* Stores deterministic bounded individual health variation. */
	float HealthVariation = 0.0f;
};

struct ADAPTIVEENVRUNTIME_API FAEM7LifecycleModel
{
	/* Resolves first-input initialization or one bounded health transition step. */
	static float ResolveHealthRatio(
		float PreviousHealthRatio,
		float TargetHealthRatio,
		bool bHealthInitialized,
		double DeltaSimulationHours,
		float DeclineRatePerSimulationHour,
		float RecoveryRatePerSimulationHour);
	/* Classifies the first valid target as stable or terminal. */
	static EAEPlantLifecycleState ResolveInitialState(float HealthRatio, float DeadHealthThreshold);
	/* Resolves structural eligibility and health hysteresis into stable visibility. */
	static bool ResolveVisibility(
		bool bStructurallyEligible,
		bool bPreviouslyVisible,
		bool bHealthInitialized,
		float HealthRatio,
		float DeadHealthThreshold,
		float StateEpsilon);
	/* Advances reversible death masking in real seconds without changing ecological health. */
	static float ResolveDeathFadeRatio(
		float CurrentFadeRatio,
		bool bDead,
		float DeltaSeconds,
		float FadeDurationSeconds);
	/* Keeps a dead representation resident until its fully masked value has reached rendering. */
	static bool ResolveRenderResidence(
		bool bStructurallyEligible,
		bool bHealthAllowsResidence,
		bool bDead,
		bool bFadeCompletionRendered);
};
