#include "AEM7Types.h"

/* Resolves first-input initialization or one bounded health transition step. */
float FAEM7LifecycleModel::ResolveHealthRatio(
	const float PreviousHealthRatio,
	const float TargetHealthRatio,
	const bool bHealthInitialized,
	const double DeltaSimulationHours,
	const float DeclineRatePerSimulationHour,
	const float RecoveryRatePerSimulationHour)
{
	// First effective input, including an intact unsampled baseline, avoids a synthetic transition.
	const float ClampedTarget = FMath::Clamp(TargetHealthRatio, 0.0f, 1.0f);
	if (!bHealthInitialized)
	{
		return ClampedTarget;
	}

	// Later fixed steps use the direction-specific species rate.
	const float ClampedPrevious = FMath::Clamp(PreviousHealthRatio, 0.0f, 1.0f);
	float Rate = FMath::Max(RecoveryRatePerSimulationHour, 0.0f);
	if (ClampedTarget < ClampedPrevious)
	{
		Rate = FMath::Max(DeclineRatePerSimulationHour, 0.0f);
	}
	const float StepHours = FMath::Max(static_cast<float>(DeltaSimulationHours), 0.0f);
	const float InterpolatedHealth = FMath::FInterpConstantTo(
		ClampedPrevious,
		ClampedTarget,
		StepHours,
		Rate);
	const float ClampedHealth = FMath::Clamp(InterpolatedHealth, 0.0f, 1.0f);
	return ClampedHealth;
}

/* Classifies the first valid target as stable or terminal. */
EAEPlantLifecycleState FAEM7LifecycleModel::ResolveInitialState(
	const float HealthRatio,
	const float DeadHealthThreshold)
{
	const float ClampedHealth = FMath::Clamp(HealthRatio, 0.0f, 1.0f);
	const float ClampedThreshold = FMath::Clamp(DeadHealthThreshold, 0.0f, 1.0f);
	if (ClampedHealth <= ClampedThreshold)
	{
		return EAEPlantLifecycleState::Dead;
	}
	return EAEPlantLifecycleState::Stable;
}
