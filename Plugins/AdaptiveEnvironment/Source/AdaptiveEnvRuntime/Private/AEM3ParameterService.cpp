#include "AEM3ParameterService.h"

#include "AEParameterBundleTypes.h"

namespace AEM3ParameterServicePrivate
{
	/* Reads one exact effective value from a previously validated canonical block. */
	bool ReadParameter(
		const FAEParameterBlock& Block,
		const FName ParameterName,
		double& OutValue,
		FAEM3ValidationResult& OutResult)
	{
		for (const FAEPublishedParameter& Parameter : Block.Parameters)
		{
			if (Parameter.Name == ParameterName)
			{
				OutValue = Parameter.EffectiveValue;
				return true;
			}
		}
		OutResult.Add(TEXT("AE-M3-PARAM-001"), FString::Printf(TEXT("Parameter %s is missing from the validated M3 block."), *ParameterName.ToString()), ParameterName);
		return false;
	}
}

/* Appends one stable blocking validation issue. */
void FAEM3ValidationResult::Add(const TCHAR* Code, const FString& Message, const FName ParameterName)
{
	Issues.Add({ Code, Message, ParameterName });
}

/* Formats all findings for one aggregated initialization log. */
FString FAEM3ValidationResult::ToString() const
{
	TArray<FString> Messages;
	for (const FAEM3ValidationIssue& Issue : Issues)
	{
		Messages.Add(FString::Printf(TEXT("%s: %s"), *Issue.Code, *Issue.Message));
	}
	// Join deterministic findings for one aggregate initialization log.
	return FString::Join(Messages, TEXT(" | "));
}

/* Maps one validated M3 block into grouped values and applies cross-parameter gates. */
FAEM3ValidationResult FAEM3ParameterService::BuildParameterSet(
	const FAEParameterBlockView& BlockView,
	const FAEParameterBundleIdentity& BundleIdentity,
	FAEM3ParameterSet& OutParameterSet)
{
	FAEM3ValidationResult Result;
	if (!BlockView.IsValid() || !BundleIdentity.BundleId.IsValid() || BundleIdentity.SemanticVersion.IsEmpty() || BundleIdentity.ContentHash.Len() != 64)
	{
		Result.Add(TEXT("AE-M3-PARAM-006"), TEXT("Bundle identity or M3 block view is invalid."));
		return Result;
	}

	FAEM3ParameterSet Candidate;
	auto Read = [&BlockView, &Result](const TCHAR* Name, double& Destination)
	{
		AEM3ParameterServicePrivate::ReadParameter(*BlockView.Block, FName(Name), Destination, Result);
	};

	// Map transport names once into stable channel indices.
	Read(TEXT("ExposurePassReferenceCount"), Candidate.Channel(EAEExposureChannel::Pass).ReferenceValue);
	Read(TEXT("ExposurePassWeight"), Candidate.Channel(EAEExposureChannel::Pass).Weight);
	Read(TEXT("ExposureTravelDistanceReferenceMeters"), Candidate.Channel(EAEExposureChannel::Travel).ReferenceValue);
	Read(TEXT("ExposureTravelDistanceWeight"), Candidate.Channel(EAEExposureChannel::Travel).Weight);
	Read(TEXT("ExposureDwellReferenceSeconds"), Candidate.Channel(EAEExposureChannel::Dwell).ReferenceValue);
	Read(TEXT("ExposureDwellWeight"), Candidate.Channel(EAEExposureChannel::Dwell).Weight);
	Read(TEXT("ExposureSprintDistanceReferenceMeters"), Candidate.Channel(EAEExposureChannel::Sprint).ReferenceValue);
	Read(TEXT("ExposureSprintWeight"), Candidate.Channel(EAEExposureChannel::Sprint).Weight);
	Read(TEXT("ExposureCollectEventReferenceCount"), Candidate.Channel(EAEExposureChannel::Collect).ReferenceValue);
	Read(TEXT("ExposureCollectEventWeight"), Candidate.Channel(EAEExposureChannel::Collect).Weight);
	Read(TEXT("ExposureCombatEventReferenceCount"), Candidate.Channel(EAEExposureChannel::Combat).ReferenceValue);
	Read(TEXT("ExposureCombatEventWeight"), Candidate.Channel(EAEExposureChannel::Combat).Weight);

	// Map Exposure dynamics without retaining transport-name lookups in fixed steps.
	Read(TEXT("ExposureMaximum"), Candidate.ExposureDynamics.Maximum);
	Read(TEXT("ExposureHalfLifeSimulationHours"), Candidate.ExposureDynamics.HalfLifeSimulationHours);

	const FAEM3ValidationResult NumericResult = ValidateParameterSet(Candidate);
	Result.Issues.Append(NumericResult.Issues);
	if (Result.IsValid())
	{
		OutParameterSet = MoveTemp(Candidate);
	}
	return Result;
}

/* Validates grouped numeric ranges, weight normalization, and threshold ordering. */
FAEM3ValidationResult FAEM3ParameterService::ValidateParameterSet(const FAEM3ParameterSet& ParameterSet)
{
	FAEM3ValidationResult Result;
	double WeightSum = 0.0;

	// Validate every channel denominator and weight before using aggregate relationships.
	for (int32 Index = 0; Index < static_cast<int32>(EAEExposureChannel::Count); ++Index)
	{
		const FAEExposureChannelParameters& Channel = ParameterSet.Channels[Index];
		if (!FMath::IsFinite(Channel.ReferenceValue) || Channel.ReferenceValue <= 0.0 || !FMath::IsFinite(Channel.Weight) || Channel.Weight < 0.0)
		{
			Result.Add(TEXT("AE-M3-PARAM-003"), TEXT("Every channel reference must be finite and positive, and every weight must be finite and non-negative."));
		}
		WeightSum += Channel.Weight;
	}
	if (!FMath::IsNearlyEqual(WeightSum, 1.0, 1.0e-6))
	{
		Result.Add(TEXT("AE-M3-PARAM-004"), FString::Printf(TEXT("Exposure weights must sum to 1.0; found %.12g."), WeightSum));
	}

	// Validate finite Exposure dynamics.
	const double Values[] =
	{
		ParameterSet.ExposureDynamics.Maximum,
		ParameterSet.ExposureDynamics.HalfLifeSimulationHours
	};
	for (const double Value : Values)
	{
		if (!FMath::IsFinite(Value))
		{
			Result.Add(TEXT("AE-M3-PARAM-003"), TEXT("Every M3 dynamics value must be finite."));
			return Result;
		}
	}
	if (ParameterSet.ExposureDynamics.Maximum <= 0.0 || ParameterSet.ExposureDynamics.HalfLifeSimulationHours <= 0.0)
	{
		Result.Add(TEXT("AE-M3-PARAM-003"), TEXT("Exposure maximum and half-life must be positive."));
	}
	return Result;
}
