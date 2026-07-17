#include "AEM3ParameterService.h"

#include "AEParameterPackageService.h"
#include "AdaptiveEnvDataAssets.h"

namespace AEM3ParameterServicePrivate
{
	/* Reads one unique effective parameter and verifies its exact canonical unit. */
	bool ReadParameter(
		const UAEParameterSynthesisAsset& Package,
		const FName ParameterName,
		const TCHAR* ExpectedUnit,
		double& OutValue,
		FString& OutVersion,
		FAEM3ValidationResult& OutResult)
	{
		// Reject duplicate stable names before using the M2 first-match lookup.
		int32 MatchCount = 0;
		for (const FAEDerivedParameter& Parameter : Package.DerivedParameters)
		{
			MatchCount += Parameter.ParameterName == ParameterName ? 1 : 0;
		}
		if (MatchCount != 1)
		{
			OutResult.Add(
				TEXT("AE-M3-PARAM-001"),
				FString::Printf(TEXT("Parameter %s must occur exactly once; found %d."), *ParameterName.ToString(), MatchCount),
				ParameterName);
			return false;
		}

		// Resolve the effective value through the published M2 runtime contract.
		FString Unit;
		if (!FAEParameterPackageService::TryGetEffectiveParameter(Package, ParameterName, OutValue, Unit, OutVersion))
		{
			OutResult.Add(TEXT("AE-M3-PARAM-003"), FString::Printf(TEXT("Parameter %s has an invalid effective value."), *ParameterName.ToString()), ParameterName);
			return false;
		}
		if (Unit != ExpectedUnit)
		{
			OutResult.Add(
				TEXT("AE-M3-PARAM-002"),
				FString::Printf(TEXT("Parameter %s uses unit %s; expected %s."), *ParameterName.ToString(), *Unit, ExpectedUnit),
				ParameterName);
			return false;
		}
		return true;
	}
}

/* Append one stable blocking validation issue. */
void FAEM3ValidationResult::Add(const TCHAR* Code, const FString& Message, const FName ParameterName)
{
	Issues.Add({ Code, Message, ParameterName });
}

/* Format all findings for one aggregated initialization log. */
FString FAEM3ValidationResult::ToString() const
{
	TArray<FString> Messages;
	Messages.Reserve(Issues.Num());
	for (const FAEM3ValidationIssue& Issue : Issues)
	{
		Messages.Add(FString::Printf(TEXT("%s: %s"), *Issue.Code, *Issue.Message));
	}
	const FString JoinedMessage = FString::Join(Messages, TEXT(" | "));
	return JoinedMessage;
}

/* Load and validate the complete first-version M3 parameter contract. */
FAEM3ValidationResult FAEM3ParameterService::BuildParameterSet(
	const UAEParameterSynthesisAsset& Package,
	FAEM3ParameterSet& OutParameterSet)
{
	FAEM3ValidationResult Result;
	FAEM3ParameterSet Candidate;
	Candidate.PackageId = Package.Metadata.PackageId;
	Candidate.SemanticVersion = Package.Metadata.SemanticVersion;
	Candidate.ContentHash = Package.Metadata.ContentHash;

	// Load all required scalar values with exact canonical units.
	auto Read = [&Package, &Candidate, &Result](const TCHAR* Name, const TCHAR* Unit, double& Destination)
	{
		FString Version;
		const FName ParameterName(Name);
		if (AEM3ParameterServicePrivate::ReadParameter(Package, ParameterName, Unit, Destination, Version, Result))
		{
			Candidate.ParameterVersions.Add(ParameterName, Version);
		}
	};
	Read(TEXT("ExposurePassReferenceCount"), TEXT("count"), Candidate.ExposurePassReferenceCount);
	Read(TEXT("ExposureTravelDistanceReferenceMeters"), TEXT("m"), Candidate.ExposureTravelDistanceReferenceMeters);
	Read(TEXT("ExposureDwellReferenceSeconds"), TEXT("s"), Candidate.ExposureDwellReferenceSeconds);
	Read(TEXT("ExposureSprintDistanceReferenceMeters"), TEXT("m"), Candidate.ExposureSprintDistanceReferenceMeters);
	Read(TEXT("ExposureCollectEventReferenceCount"), TEXT("count"), Candidate.ExposureCollectEventReferenceCount);
	Read(TEXT("ExposureCombatEventReferenceCount"), TEXT("count"), Candidate.ExposureCombatEventReferenceCount);
	Read(TEXT("ExposurePassWeight"), TEXT("ratio"), Candidate.ExposurePassWeight);
	Read(TEXT("ExposureTravelDistanceWeight"), TEXT("ratio"), Candidate.ExposureTravelDistanceWeight);
	Read(TEXT("ExposureDwellWeight"), TEXT("ratio"), Candidate.ExposureDwellWeight);
	Read(TEXT("ExposureSprintWeight"), TEXT("ratio"), Candidate.ExposureSprintWeight);
	Read(TEXT("ExposureCollectEventWeight"), TEXT("ratio"), Candidate.ExposureCollectEventWeight);
	Read(TEXT("ExposureCombatEventWeight"), TEXT("ratio"), Candidate.ExposureCombatEventWeight);
	Read(TEXT("ExposureMaximum"), TEXT("ratio"), Candidate.ExposureMaximum);
	Read(TEXT("ExposureHalfLifeSimulationHours"), TEXT("h"), Candidate.ExposureHalfLifeSimulationHours);
	Read(TEXT("DamageActivationExposure"), TEXT("ratio"), Candidate.DamageActivationExposure);
	Read(TEXT("DamageSaturationExposure"), TEXT("ratio"), Candidate.DamageSaturationExposure);
	Read(TEXT("DamageMaximumRatePerSimulationHour"), TEXT("ratio/h"), Candidate.DamageMaximumRatePerSimulationHour);
	Read(TEXT("RecoveryActivationExposure"), TEXT("ratio"), Candidate.RecoveryActivationExposure);
	Read(TEXT("RecoveryDelaySimulationHours"), TEXT("h"), Candidate.RecoveryDelaySimulationHours);
	Read(TEXT("RecoveryBaseRatePerSimulationHour"), TEXT("ratio/h"), Candidate.RecoveryBaseRatePerSimulationHour);

	// Verify package identity and canonical content before accepting effective values.
	FString HashError;
	const FString ComputedHash = FAEParameterPackageService::ComputeContentHash(Package, HashError);
	if (!Candidate.PackageId.IsValid()
		|| Candidate.SemanticVersion.IsEmpty()
		|| Candidate.ContentHash.Len() != 64
		|| ComputedHash.IsEmpty()
		|| ComputedHash != Candidate.ContentHash)
	{
		Result.Add(TEXT("AE-M3-PARAM-006"), TEXT("Parameter package identity, version, or content hash is invalid."));
	}

	// Apply numeric and relationship gates only after collection to report all findings together.
	const FAEM3ValidationResult NumericResult = ValidateParameterSet(Candidate);
	Result.Issues.Append(NumericResult.Issues);
	if (Result.IsValid())
	{
		OutParameterSet = MoveTemp(Candidate);
	}
	return Result;
}

/* Validate first-version numeric ranges, weight normalization, and threshold ordering. */
FAEM3ValidationResult FAEM3ParameterService::ValidateParameterSet(const FAEM3ParameterSet& ParameterSet)
{
	FAEM3ValidationResult Result;

	// Reject non-finite values before range and ordering comparisons.
	const double Values[] = {
		ParameterSet.ExposurePassReferenceCount, ParameterSet.ExposureTravelDistanceReferenceMeters,
		ParameterSet.ExposureDwellReferenceSeconds, ParameterSet.ExposureSprintDistanceReferenceMeters,
		ParameterSet.ExposureCollectEventReferenceCount, ParameterSet.ExposureCombatEventReferenceCount,
		ParameterSet.ExposurePassWeight, ParameterSet.ExposureTravelDistanceWeight, ParameterSet.ExposureDwellWeight,
		ParameterSet.ExposureSprintWeight, ParameterSet.ExposureCollectEventWeight, ParameterSet.ExposureCombatEventWeight,
		ParameterSet.ExposureMaximum, ParameterSet.ExposureHalfLifeSimulationHours,
		ParameterSet.DamageActivationExposure, ParameterSet.DamageSaturationExposure,
		ParameterSet.DamageMaximumRatePerSimulationHour, ParameterSet.RecoveryActivationExposure,
		ParameterSet.RecoveryDelaySimulationHours, ParameterSet.RecoveryBaseRatePerSimulationHour
	};
	for (const double Value : Values)
	{
		if (!FMath::IsFinite(Value))
		{
			Result.Add(TEXT("AE-M3-PARAM-003"), TEXT("Every M3 parameter must be finite."));
			return Result;
		}
	}

	// Require positive denominators, half-life, and output bound.
	if (ParameterSet.ExposurePassReferenceCount <= 0.0
		|| ParameterSet.ExposureTravelDistanceReferenceMeters <= 0.0
		|| ParameterSet.ExposureDwellReferenceSeconds <= 0.0
		|| ParameterSet.ExposureSprintDistanceReferenceMeters <= 0.0
		|| ParameterSet.ExposureCollectEventReferenceCount <= 0.0
		|| ParameterSet.ExposureCombatEventReferenceCount <= 0.0
		|| ParameterSet.ExposureMaximum <= 0.0
		|| ParameterSet.ExposureHalfLifeSimulationHours <= 0.0
		|| ParameterSet.DamageMaximumRatePerSimulationHour < 0.0
		|| ParameterSet.RecoveryDelaySimulationHours < 0.0
		|| ParameterSet.RecoveryBaseRatePerSimulationHour < 0.0)
	{
		Result.Add(TEXT("AE-M3-PARAM-003"), TEXT("References, Exposure maximum, and half-life must be positive; rates and delay must be non-negative."));
	}

	// Keep effective weights explicit rather than silently normalizing M2 values.
	const double Weights[] = {
		ParameterSet.ExposurePassWeight, ParameterSet.ExposureTravelDistanceWeight,
		ParameterSet.ExposureDwellWeight, ParameterSet.ExposureSprintWeight,
		ParameterSet.ExposureCollectEventWeight, ParameterSet.ExposureCombatEventWeight
	};
	double WeightSum = 0.0;
	for (const double Weight : Weights)
	{
		if (Weight < 0.0)
		{
			Result.Add(TEXT("AE-M3-PARAM-003"), TEXT("Exposure weights must be non-negative."));
			break;
		}
		WeightSum += Weight;
	}
	if (!FMath::IsNearlyEqual(WeightSum, 1.0, 1.0e-6))
	{
		Result.Add(TEXT("AE-M3-PARAM-004"), FString::Printf(TEXT("Exposure weights must sum to 1.0; found %.12g."), WeightSum));
	}

	// Establish a Recovery zone, optional neutral band, and non-degenerate Damage ramp.
	if (ParameterSet.RecoveryActivationExposure < 0.0
		|| ParameterSet.RecoveryActivationExposure > ParameterSet.DamageActivationExposure
		|| ParameterSet.DamageActivationExposure >= ParameterSet.DamageSaturationExposure
		|| ParameterSet.DamageSaturationExposure > ParameterSet.ExposureMaximum)
	{
		Result.Add(TEXT("AE-M3-PARAM-005"), TEXT("Thresholds must satisfy 0 <= RecoveryActivation <= DamageActivation < DamageSaturation <= ExposureMaximum."));
	}
	return Result;
}
