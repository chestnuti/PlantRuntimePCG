#include "AEM2ConfigService.h"

#include "AEAdaptiveEnvironmentProfile.h"
#include "AEM3ParameterService.h"
#include "AEM4ParameterService.h"
#include "AEM5ParameterService.h"
#include "AEWorldScalarFieldAsset.h"

void FAEM2ValidationResult::Add(const TCHAR* Code, const FString& Message)
{
	Issues.Add(FString::Printf(TEXT("%s: %s"), Code, *Message));
}

FString FAEM2ValidationResult::ToString() const
{
	return FString::Join(Issues, TEXT(" | "));
}

namespace AEM2ConfigServicePrivate
{
	void CopyChannel(const FAEExposureChannelConfig& Source, FAEExposureChannelParameters& Destination)
	{
		Destination.ReferenceValue = Source.ReferenceValue;
		Destination.Weight = Source.Weight;
	}

	FAEActiveEnvironmentConfig Convert(const UAEAdaptiveEnvironmentProfile& Profile, const uint32 RuntimeRevision)
	{
		FAEActiveEnvironmentConfig Candidate;
		Candidate.ProfileId = Profile.ProfileId;
		Candidate.ConfigVersion = Profile.ConfigVersion;
		Candidate.RuntimeRevision = RuntimeRevision;
		Candidate.DefaultMoistureRatio = Profile.M4.DefaultMoistureRatio;
		Candidate.MoistureTexture = Profile.M4.MoistureTexture;

		CopyChannel(Profile.M3.Pass, Candidate.M3.Channel(EAEExposureChannel::Pass));
		CopyChannel(Profile.M3.Travel, Candidate.M3.Channel(EAEExposureChannel::Travel));
		CopyChannel(Profile.M3.Dwell, Candidate.M3.Channel(EAEExposureChannel::Dwell));
		CopyChannel(Profile.M3.Sprint, Candidate.M3.Channel(EAEExposureChannel::Sprint));
		CopyChannel(Profile.M3.Collect, Candidate.M3.Channel(EAEExposureChannel::Collect));
		CopyChannel(Profile.M3.Combat, Candidate.M3.Channel(EAEExposureChannel::Combat));
		Candidate.M3.ExposureDynamics.Maximum = Profile.M3.MaximumExposure;
		Candidate.M3.ExposureDynamics.HalfLifeSimulationHours = Profile.M3.HalfLifeSimulationHours;

		Candidate.M4.ConstraintResponse.SlopeFullySuitableDegrees = Profile.M4.SlopeFullySuitableDegrees;
		Candidate.M4.ConstraintResponse.SlopeUnsuitableDegrees = Profile.M4.SlopeUnsuitableDegrees;
		Candidate.M4.ConstraintResponse.MoistureOptimalMinimumRatio = Profile.M4.MoistureOptimalMinimumRatio;
		Candidate.M4.ConstraintResponse.MoistureOptimalMaximumRatio = Profile.M4.MoistureOptimalMaximumRatio;
		Candidate.M4.ConstraintResponse.MoistureToleranceWidthRatio = Profile.M4.MoistureToleranceWidthRatio;
		Candidate.M4.RegionState.ActiveThreshold = Profile.M4.ActiveThreshold;
		Candidate.M4.RegionState.OverusedThreshold = Profile.M4.OverusedThreshold;
		Candidate.M4.RegionState.HysteresisWidth = Profile.M4.HysteresisWidth;
		Candidate.M4.RegionState.TransitionDebounceSimulationHours = Profile.M4.TransitionDebounceSimulationHours;

		Candidate.M5.Fusion.ConstraintSensitivity = Profile.M5.ConstraintSensitivity;
		Candidate.M5.Damage.ActivationImpact = Profile.M5.DamageActivationImpact;
		Candidate.M5.Damage.SaturationImpact = Profile.M5.DamageSaturationImpact;
		Candidate.M5.Damage.MaximumRatePerSimulationHour = Profile.M5.DamageMaximumRatePerSimulationHour;
		Candidate.M5.Recovery.ActivationExposure = Profile.M5.RecoveryActivationExposure;
		Candidate.M5.Recovery.DelaySimulationHours = Profile.M5.RecoveryDelaySimulationHours;
		Candidate.M5.Recovery.BaseRatePerSimulationHour = Profile.M5.RecoveryBaseRatePerSimulationHour;
		return Candidate;
	}

	void AppendStageIssues(const TCHAR* Stage, const FString& Issues, FAEM2ValidationResult& Result)
	{
		if (!Issues.IsEmpty())
		{
			Result.Add(TEXT("AE-M2-CONFIG-004"), FString::Printf(TEXT("%s configuration is invalid: %s"), Stage, *Issues));
		}
	}
}

FAEM2ValidationResult FAEM2ConfigService::ValidateProfile(const UAEAdaptiveEnvironmentProfile& Profile)
{
	FAEActiveEnvironmentConfig Ignored;
	return BuildActiveConfig(Profile, 1, Ignored);
}

FAEM2ValidationResult FAEM2ConfigService::BuildActiveConfig(const UAEAdaptiveEnvironmentProfile& Profile, const uint32 NextRuntimeRevision, FAEActiveEnvironmentConfig& OutConfig)
{
	FAEM2ValidationResult Result;
	if (Profile.ProfileId.IsNone())
	{
		Result.Add(TEXT("AE-M2-CONFIG-001"), TEXT("ProfileId must not be None."));
	}
	if (Profile.ConfigVersion != 2)
	{
		Result.Add(TEXT("AE-M2-CONFIG-002"), FString::Printf(TEXT("ConfigVersion %d is not supported."), Profile.ConfigVersion));
	}
	if (NextRuntimeRevision == 0)
	{
		Result.Add(TEXT("AE-M2-CONFIG-003"), TEXT("RuntimeRevision must be greater than zero."));
	}

	FAEActiveEnvironmentConfig Candidate = AEM2ConfigServicePrivate::Convert(Profile, NextRuntimeRevision);
	if (!FMath::IsFinite(Profile.M4.DefaultMoistureRatio)
		|| Profile.M4.DefaultMoistureRatio < 0.0 || Profile.M4.DefaultMoistureRatio > 1.0)
	{
		Result.Add(TEXT("AE-M2-CONFIG-005"), TEXT("M4 DefaultMoistureRatio must be finite and in [0,1]."));
	}
	if (Profile.M4.MoistureTexture != nullptr)
	{
		FString FieldError;
		if (!Profile.M4.MoistureTexture->IsValidField(FieldError))
		{
			Result.Add(TEXT("AE-M2-CONFIG-006"), FString::Printf(TEXT("M4 moisture field is invalid: %s"), *FieldError));
		}
	}
	AEM2ConfigServicePrivate::AppendStageIssues(TEXT("M3"), FAEM3ParameterService::ValidateParameterSet(Candidate.M3).ToString(), Result);
	AEM2ConfigServicePrivate::AppendStageIssues(TEXT("M4"), FAEM4ParameterService::ValidateParameterSet(Candidate.M4).ToString(), Result);
	AEM2ConfigServicePrivate::AppendStageIssues(TEXT("M5"), FAEM5ParameterService::ValidateParameterSet(Candidate.M5).ToString(), Result);

	if (Result.IsValid())
	{
		OutConfig = MoveTemp(Candidate);
	}
	return Result;
}
