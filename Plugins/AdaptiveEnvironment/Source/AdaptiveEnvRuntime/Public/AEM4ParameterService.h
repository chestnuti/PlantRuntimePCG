#pragma once

#include "CoreMinimal.h"
#include "AEM4Types.h"

/* Collects deterministic blocking M4 parameter findings. */
struct ADAPTIVEENVRUNTIME_API FAEM4ValidationResult
{
	/* Stores stable AE-M4-PARAM findings in validation order. */
	TArray<FString> Issues;
	/* Returns true when no finding blocks M4 use. */
	bool IsValid() const { return Issues.IsEmpty(); }
	/* Adds one stable code and message. */
	void Add(const TCHAR* Code, const FString& Message);
	/* Joins all findings into one concise diagnostic. */
	FString ToString() const;
};

/* Builds and evaluates the M4 constraint and effective-pressure contract. */
class ADAPTIVEENVRUNTIME_API FAEM4ParameterService
{
public:
	/* Maps one validated M4 block into grouped values and applies relationship gates. */
	static FAEM4ValidationResult BuildParameterSet(const FAEParameterBlockView& Block, const FAEParameterBundleIdentity& BundleIdentity, FAEM4ParameterSet& OutParameters);
	/* Validates numeric ranges, threshold ordering, hysteresis boundaries, and debounce. */
	static FAEM4ValidationResult ValidateParameterSet(const FAEM4ParameterSet& Parameters);
	/* Evaluates suitability, effective pressure, hysteresis, and debounce for one fixed step. */
	static FAEM4DecisionSnapshot EvaluateDecision(double SlopeDegrees, double MoistureRatio, const FAEM3CellSnapshot& M3, double ExposureMaximum, double DeltaSimulationHours, const FAEM4ParameterSet& Parameters, FAEM4StateMemory& InOutState);
};
