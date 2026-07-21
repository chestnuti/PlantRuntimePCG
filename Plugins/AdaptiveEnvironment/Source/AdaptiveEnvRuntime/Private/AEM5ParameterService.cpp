#include "AEM5ParameterService.h"

namespace AEM5ParameterServicePrivate
{
	/* Reads one exact M5 value from a validated block. */
	bool Read(const FAEParameterBlock& Block, const TCHAR* Name, double& OutValue, FAEM5ValidationResult& Result)
	{
		for (const FAEPublishedParameter& Parameter : Block.Parameters)
		{
			if (Parameter.Name == FName(Name)) { OutValue = Parameter.EffectiveValue; return true; }
		}
		Result.Add(TEXT("AE-M5-PARAM-001"), FString::Printf(TEXT("Parameter %s is missing from the validated M5 block."), Name));
		return false;
	}
}

/* Adds one stable M5 validation finding. */
void FAEM5ValidationResult::Add(const TCHAR* Code, const FString& Message) { Issues.Add(FString::Printf(TEXT("%s: %s"), Code, *Message)); }

/* Joins M5 findings for one initialization log. */
FString FAEM5ValidationResult::ToString() const { return FString::Join(Issues, TEXT(" | ")); }

/* Maps one validated M5 block into typed runtime values. */
FAEM5ValidationResult FAEM5ParameterService::BuildParameterSet(const FAEParameterBlockView& Block, const FAEParameterBundleIdentity& BundleIdentity, FAEM5ParameterSet& OutParameters)
{
	FAEM5ValidationResult Result;
	if (!Block.IsValid() || !BundleIdentity.BundleId.IsValid() || BundleIdentity.ContentHash.Len() != 64)
	{
		Result.Add(TEXT("AE-M5-PARAM-006"), TEXT("Bundle identity or M5 block view is invalid."));
		return Result;
	}
	FAEM5ParameterSet Candidate;
	// Map the canonical transport names into responsibility groups.
	AEM5ParameterServicePrivate::Read(*Block.Block, TEXT("ConstraintSensitivity"), Candidate.Fusion.ConstraintSensitivity, Result);
	AEM5ParameterServicePrivate::Read(*Block.Block, TEXT("DamageActivationImpact"), Candidate.Damage.ActivationImpact, Result);
	AEM5ParameterServicePrivate::Read(*Block.Block, TEXT("DamageMaximumRatePerSimulationHour"), Candidate.Damage.MaximumRatePerSimulationHour, Result);
	AEM5ParameterServicePrivate::Read(*Block.Block, TEXT("DamageSaturationImpact"), Candidate.Damage.SaturationImpact, Result);
	AEM5ParameterServicePrivate::Read(*Block.Block, TEXT("RecoveryActivationExposure"), Candidate.Recovery.ActivationExposure, Result);
	AEM5ParameterServicePrivate::Read(*Block.Block, TEXT("RecoveryBaseRatePerSimulationHour"), Candidate.Recovery.BaseRatePerSimulationHour, Result);
	AEM5ParameterServicePrivate::Read(*Block.Block, TEXT("RecoveryDelaySimulationHours"), Candidate.Recovery.DelaySimulationHours, Result);
	const FAEM5ValidationResult Numeric = ValidateParameterSet(Candidate);
	Result.Issues.Append(Numeric.Issues);
	if (Result.IsValid()) OutParameters = Candidate;
	return Result;
}

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
