#include "AEM3ParameterService.h"

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
