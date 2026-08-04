#include "AEM5ParameterService.h"

/* Adds one stable M5 validation finding. */
void FAEM5ValidationResult::Add(const TCHAR* Code, const FString& Message) { Issues.Add(FString::Printf(TEXT("%s: %s"), Code, *Message)); }

/* Joins M5 findings for one initialization log. */
FString FAEM5ValidationResult::ToString() const { return FString::Join(Issues, TEXT(" | ")); }

/* Validates M5 ranges and ordered response thresholds. */
FAEM5ValidationResult FAEM5ParameterService::ValidateParameterSet(const FAEM5ParameterSet& P)
{
	FAEM5ValidationResult Result;
	const double Values[] = { P.Fusion.ConstraintSensitivity, P.Damage.ActivationImpact, P.Damage.SaturationImpact, P.Damage.MaximumRatePerSimulationHour, P.Recovery.ActivationExposure, P.Recovery.DelaySimulationHours, P.Recovery.BaseRatePerSimulationHour };
	for (const double Value : Values) if (!FMath::IsFinite(Value)) { Result.Add(TEXT("AE-M5-PARAM-003"), TEXT("Every M5 parameter must be finite.")); return Result; }
	if (P.Fusion.ConstraintSensitivity < 0.0 || P.Damage.ActivationImpact < 0.0 || P.Damage.ActivationImpact >= P.Damage.SaturationImpact || P.Damage.SaturationImpact > 1.0 || P.Damage.MaximumRatePerSimulationHour < 0.0 || P.Recovery.ActivationExposure < 0.0 || P.Recovery.ActivationExposure > 1.0 || P.Recovery.DelaySimulationHours < 0.0 || P.Recovery.BaseRatePerSimulationHour < 0.0)
	{
		Result.Add(TEXT("AE-M5-PARAM-005"), TEXT("M5 thresholds and rates are invalid."));
	}
	return Result;
}

/* Evaluates mutually exclusive Damage or Recovery for one fixed step. */
FAEEcologicalResponseSnapshot FAEM5ParameterService::EvaluateResponse(const double Exposure, const double ExposureMaximum, const double ConstraintPressureRatio, const double HabitatSuitabilityRatio, const double DeltaSimulationHours, const FAEM5ParameterSet& P, FAEM5StateMemory& State)
{
	FAEEcologicalResponseSnapshot Output;
	const double NormalizedExposure = FMath::Clamp(Exposure / FMath::Max(ExposureMaximum, UE_DOUBLE_SMALL_NUMBER), 0.0, 1.0);
	const double EffectiveImpact = FMath::Clamp(NormalizedExposure * (1.0 + FMath::Clamp(ConstraintPressureRatio, 0.0, 1.0) * P.Fusion.ConstraintSensitivity), 0.0, 1.0);
	const double Step = FMath::Max(DeltaSimulationHours, 0.0);
	double DamageRate = 0.0;
	double RecoveryRate = 0.0;
	// Damage has priority; Recovery requires continuous low Exposure.
	if (EffectiveImpact >= P.Damage.ActivationImpact)
	{
		const double Alpha = FMath::Clamp((EffectiveImpact - P.Damage.ActivationImpact) / (P.Damage.SaturationImpact - P.Damage.ActivationImpact), 0.0, 1.0);
		DamageRate = Alpha * P.Damage.MaximumRatePerSimulationHour;
		State.LowExposureDurationSimulationHours = 0.0;
	}
	else if (NormalizedExposure < P.Recovery.ActivationExposure)
	{
		State.LowExposureDurationSimulationHours += Step;
		if (State.LowExposureDurationSimulationHours + UE_DOUBLE_SMALL_NUMBER >= P.Recovery.DelaySimulationHours)
		{
			RecoveryRate = P.Recovery.BaseRatePerSimulationHour * FMath::Clamp(HabitatSuitabilityRatio, 0.0, 1.0);
		}
	}
	else
	{
		State.LowExposureDurationSimulationHours = 0.0;
	}
	const double PreviousDamage = State.DamageRatio;
	State.DamageRatio = FMath::Clamp(State.DamageRatio + (DamageRate - RecoveryRate) * Step, 0.0, 1.0);
	Output.EffectiveImpactRatio = static_cast<float>(EffectiveImpact);
	Output.DamageRatio = static_cast<float>(State.DamageRatio);
	Output.RecoveryRatio = PreviousDamage > UE_DOUBLE_SMALL_NUMBER ? static_cast<float>(FMath::Clamp((PreviousDamage - State.DamageRatio) / PreviousDamage, 0.0, 1.0)) : 0.0f;
	Output.DamageRatePerSimulationHour = static_cast<float>(DamageRate);
	Output.RecoveryRatePerSimulationHour = static_cast<float>(RecoveryRate);
	return Output;
}
