#include "AEM4ParameterService.h"

namespace AEM4ParameterServicePrivate
{
	/* Reads one exact effective value from a validated canonical M4 block. */
	bool Read(const FAEParameterBlock& Block, const TCHAR* Name, double& OutValue, FAEM4ValidationResult& Result)
	{
		for (const FAEPublishedParameter& Parameter : Block.Parameters)
		{
			if (Parameter.Name == FName(Name))
			{
				OutValue = Parameter.EffectiveValue;
				return true;
			}
		}
		Result.Add(TEXT("AE-M4-PARAM-001"), FString::Printf(TEXT("Parameter %s is missing from the validated M4 block."), Name));
		return false;
	}

	/* Returns linear slope suitability inside the two configured angle boundaries. */
	double SlopeSuitability(const double SlopeDegrees, const FAEConstraintResponseParameters& Parameters)
	{
		if (SlopeDegrees <= Parameters.SlopeFullySuitableDegrees) return 1.0;
		if (SlopeDegrees >= Parameters.SlopeUnsuitableDegrees) return 0.0;
		return 1.0 - (SlopeDegrees - Parameters.SlopeFullySuitableDegrees) /
			(Parameters.SlopeUnsuitableDegrees - Parameters.SlopeFullySuitableDegrees);
	}

	/* Returns plateau-with-linear-falloff moisture suitability. */
	double MoistureSuitability(const double MoistureRatio, const FAEConstraintResponseParameters& Parameters)
	{
		if (MoistureRatio >= Parameters.MoistureOptimalMinimumRatio && MoistureRatio <= Parameters.MoistureOptimalMaximumRatio) return 1.0;
		const double Distance = MoistureRatio < Parameters.MoistureOptimalMinimumRatio
			? Parameters.MoistureOptimalMinimumRatio - MoistureRatio
			: MoistureRatio - Parameters.MoistureOptimalMaximumRatio;
		// Clamp the linear falloff to the normalized suitability interval.
		return FMath::Clamp(1.0 - Distance / Parameters.MoistureToleranceWidthRatio, 0.0, 1.0);
	}

	/* Selects one candidate state using explicit state-dependent hysteresis boundaries. */
	EAERegionState SelectCandidate(const EAERegionState Current, const double Pressure, const FAERegionStateParameters& Parameters)
	{
		const double HalfWidth = Parameters.HysteresisWidth * 0.5;
		const double ActiveEnter = Parameters.ActiveThreshold + HalfWidth;
		const double ActiveExit = Parameters.ActiveThreshold - HalfWidth;
		const double OverusedEnter = Parameters.OverusedThreshold + HalfWidth;
		const double OverusedExit = Parameters.OverusedThreshold - HalfWidth;
		switch (Current)
		{
		case EAERegionState::Unused: return Pressure >= ActiveEnter ? EAERegionState::Active : Current;
		case EAERegionState::Active:
			if (Pressure >= OverusedEnter) return EAERegionState::Overused;
			return Pressure <= ActiveExit ? EAERegionState::Unused : Current;
		case EAERegionState::Overused: return Pressure <= OverusedExit ? EAERegionState::Recovering : Current;
		case EAERegionState::Recovering:
			if (Pressure >= OverusedEnter) return EAERegionState::Overused;
			if (Pressure <= ActiveExit) return EAERegionState::Unused;
			return Pressure < OverusedExit ? EAERegionState::Active : Current;
		default: return Current;
		}
	}
}

/* Adds one stable code and message. */
void FAEM4ValidationResult::Add(const TCHAR* Code, const FString& Message)
{
	Issues.Add(FString::Printf(TEXT("%s: %s"), Code, *Message));
}

/* Joins all findings into one concise diagnostic. */
FString FAEM4ValidationResult::ToString() const
{
	// Preserve validation order in one initialization-safe message.
	return FString::Join(Issues, TEXT(" | "));
}

/* Maps one validated M4 block into grouped values and applies relationship gates. */
FAEM4ValidationResult FAEM4ParameterService::BuildParameterSet(const FAEParameterBlockView& Block, const FAEParameterBundleIdentity& BundleIdentity, FAEM4ParameterSet& OutParameters)
{
	FAEM4ValidationResult Result;
	if (!Block.IsValid() || !BundleIdentity.BundleId.IsValid() || BundleIdentity.SemanticVersion.IsEmpty() || BundleIdentity.ContentHash.Len() != 64)
	{
		Result.Add(TEXT("AE-M4-PARAM-006"), TEXT("Bundle identity or M4 block view is invalid."));
		return Result;
	}

	FAEM4ParameterSet Candidate;
	// Map all transport names once into the two runtime responsibility groups.
	AEM4ParameterServicePrivate::Read(*Block.Block, TEXT("ActiveThreshold"), Candidate.RegionState.ActiveThreshold, Result);
	AEM4ParameterServicePrivate::Read(*Block.Block, TEXT("HysteresisWidth"), Candidate.RegionState.HysteresisWidth, Result);
	AEM4ParameterServicePrivate::Read(*Block.Block, TEXT("MoistureOptimalMaximumRatio"), Candidate.ConstraintResponse.MoistureOptimalMaximumRatio, Result);
	AEM4ParameterServicePrivate::Read(*Block.Block, TEXT("MoistureOptimalMinimumRatio"), Candidate.ConstraintResponse.MoistureOptimalMinimumRatio, Result);
	AEM4ParameterServicePrivate::Read(*Block.Block, TEXT("MoistureToleranceWidthRatio"), Candidate.ConstraintResponse.MoistureToleranceWidthRatio, Result);
	AEM4ParameterServicePrivate::Read(*Block.Block, TEXT("OverusedThreshold"), Candidate.RegionState.OverusedThreshold, Result);
	AEM4ParameterServicePrivate::Read(*Block.Block, TEXT("SlopeFullySuitableDegrees"), Candidate.ConstraintResponse.SlopeFullySuitableDegrees, Result);
	AEM4ParameterServicePrivate::Read(*Block.Block, TEXT("SlopeUnsuitableDegrees"), Candidate.ConstraintResponse.SlopeUnsuitableDegrees, Result);
	AEM4ParameterServicePrivate::Read(*Block.Block, TEXT("TransitionDebounceSimulationHours"), Candidate.RegionState.TransitionDebounceSimulationHours, Result);
	const FAEM4ValidationResult Numeric = ValidateParameterSet(Candidate);
	Result.Issues.Append(Numeric.Issues);
	if (Result.IsValid()) OutParameters = Candidate;
	return Result;
}

/* Validates numeric ranges, threshold ordering, hysteresis boundaries, and debounce. */
FAEM4ValidationResult FAEM4ParameterService::ValidateParameterSet(const FAEM4ParameterSet& Parameters)
{
	FAEM4ValidationResult Result;
	const FAEConstraintResponseParameters& C = Parameters.ConstraintResponse;
	const FAERegionStateParameters& S = Parameters.RegionState;
	const double Values[] = { C.SlopeFullySuitableDegrees, C.SlopeUnsuitableDegrees, C.MoistureOptimalMinimumRatio, C.MoistureOptimalMaximumRatio, C.MoistureToleranceWidthRatio, S.ActiveThreshold, S.OverusedThreshold, S.HysteresisWidth, S.TransitionDebounceSimulationHours };
	for (const double Value : Values)
	{
		if (!FMath::IsFinite(Value))
		{
			Result.Add(TEXT("AE-M4-PARAM-003"), TEXT("Every M4 parameter must be finite."));
			return Result;
		}
	}
	if (C.SlopeFullySuitableDegrees < 0.0 || C.SlopeFullySuitableDegrees >= C.SlopeUnsuitableDegrees || C.SlopeUnsuitableDegrees > 90.0 ||
		C.MoistureOptimalMinimumRatio < 0.0 || C.MoistureOptimalMinimumRatio > C.MoistureOptimalMaximumRatio || C.MoistureOptimalMaximumRatio > 1.0 ||
		C.MoistureToleranceWidthRatio <= 0.0 || S.TransitionDebounceSimulationHours < 0.0)
	{
		Result.Add(TEXT("AE-M4-PARAM-003"), TEXT("Constraint ranges, sensitivity, or debounce are invalid."));
	}
	const double HalfWidth = S.HysteresisWidth * 0.5;
	if (S.HysteresisWidth < 0.0 || S.ActiveThreshold - HalfWidth < 0.0 || S.OverusedThreshold + HalfWidth > 1.0 ||
		S.ActiveThreshold + HalfWidth >= S.OverusedThreshold - HalfWidth)
	{
		Result.Add(TEXT("AE-M4-PARAM-005"), TEXT("Shared hysteresis boundaries must be ordered and remain inside [0,1]."));
	}
	return Result;
}

/* Evaluates independent environment suitability, pressure, hysteresis, and debounce. */
FAEEnvironmentConstraintSnapshot FAEM4ParameterService::EvaluateEnvironment(const double SlopeDegrees, const double MoistureRatio, const double DeltaSimulationHours, const FAEM4ParameterSet& Parameters, FAEM4StateMemory& InOutState)
{
	FAEEnvironmentConstraintSnapshot Output;
	// Derive environment suitability and pressure without reading M3.
	const double Slope = AEM4ParameterServicePrivate::SlopeSuitability(FMath::Clamp(SlopeDegrees, 0.0, 90.0), Parameters.ConstraintResponse);
	const double Moisture = AEM4ParameterServicePrivate::MoistureSuitability(FMath::Clamp(MoistureRatio, 0.0, 1.0), Parameters.ConstraintResponse);
	const double Suitability = FMath::Clamp(Slope * Moisture, 0.0, 1.0);
	const double Pressure = 1.0 - Suitability;

	// Accumulate only a stable changed candidate and commit at the shared debounce boundary.
	const EAERegionState Candidate = AEM4ParameterServicePrivate::SelectCandidate(InOutState.CurrentState, Pressure, Parameters.RegionState);
	if (Candidate == InOutState.CurrentState)
	{
		InOutState.CandidateState = InOutState.CurrentState;
		InOutState.CandidateDurationSimulationHours = 0.0;
	}
	else
	{
		if (Candidate != InOutState.CandidateState)
		{
			InOutState.CandidateState = Candidate;
			InOutState.CandidateDurationSimulationHours = 0.0;
		}
		InOutState.CandidateDurationSimulationHours += FMath::Max(DeltaSimulationHours, 0.0);
		if (InOutState.CandidateDurationSimulationHours + UE_DOUBLE_SMALL_NUMBER >= Parameters.RegionState.TransitionDebounceSimulationHours)
		{
			InOutState.CurrentState = Candidate;
			InOutState.CandidateState = Candidate;
			InOutState.CandidateDurationSimulationHours = 0.0;
		}
	}

	Output.ConstraintPressureRatio = static_cast<float>(Pressure);
	Output.HabitatSuitabilityRatio = static_cast<float>(Suitability);
	Output.State = InOutState.CurrentState;
	return Output;
}
