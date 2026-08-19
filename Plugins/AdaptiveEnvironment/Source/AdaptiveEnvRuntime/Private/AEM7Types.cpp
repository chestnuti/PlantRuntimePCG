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

/* Resolves structural eligibility and health hysteresis into stable visibility. */
bool FAEM7LifecycleModel::ResolveVisibility(
	const bool bStructurallyEligible,
	const bool bPreviouslyVisible,
	const bool bHealthInitialized,
	const float HealthRatio,
	const float DeadHealthThreshold,
	const float StateEpsilon)
{
	if (!bStructurallyEligible)
	{
		return false;
	}

	// Clamp the removal and reappearance thresholds into one ordered health interval.
	const float ClampedHealth = FMath::Clamp(HealthRatio, 0.0f, 1.0f);
	const float RemovalThreshold = FMath::Clamp(DeadHealthThreshold, 0.0f, 1.0f);
	const float ReappearanceThreshold = FMath::Clamp(
		RemovalThreshold + FMath::Max(StateEpsilon, 0.0f),
		RemovalThreshold,
		1.0f);
	if (!bHealthInitialized)
	{
		return ClampedHealth > RemovalThreshold;
	}
	if (bPreviouslyVisible)
	{
		return ClampedHealth > RemovalThreshold;
	}
	return ClampedHealth >= ReappearanceThreshold;
}

/* Advance the visual retirement envelope independently from simulation-hour health rates. */
float FAEM7LifecycleModel::ResolveDeathFadeRatio(
	const float CurrentFadeRatio,
	const bool bDead,
	const float DeltaSeconds,
	const float FadeDurationSeconds)
{
	const float Current = FMath::Clamp(CurrentFadeRatio, 0.0f, 1.0f);
	if (!FMath::IsFinite(DeltaSeconds) || !FMath::IsFinite(FadeDurationSeconds)
		|| DeltaSeconds <= 0.0f || FadeDurationSeconds <= UE_KINDA_SMALL_NUMBER)
	{
		return bDead ? 1.0f : 0.0f;
	}
	const float DeltaRatio = DeltaSeconds / FadeDurationSeconds;
	return FMath::Clamp(Current + (bDead ? DeltaRatio : -DeltaRatio), 0.0f, 1.0f);
}

/* Resolve visual residency without changing ecological eligibility or lifecycle state. */
bool FAEM7LifecycleModel::ResolveRenderResidence(
	const bool bStructurallyEligible,
	const bool bHealthAllowsResidence,
	const bool bDead,
	const bool bFadeCompletionRendered)
{
	return bStructurallyEligible
		&& (bHealthAllowsResidence || (bDead && !bFadeCompletionRendered));
}
