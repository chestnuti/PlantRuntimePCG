#pragma once

#include "CoreMinimal.h"
#include "AEM5Types.h"

/* Collects deterministic M5 validation findings. */
struct ADAPTIVEENVRUNTIME_API FAEM5ValidationResult
{
	/* Stores stable issue codes and diagnostic messages. */
	TArray<FString> Issues;
	/* Returns true when no validation issue was recorded. */
	bool IsValid() const { return Issues.IsEmpty(); }
	/* Appends one stable issue code and its diagnostic message. */
	void Add(const TCHAR* Code, const FString& Message);
	/* Formats all issues for logs and rejected bundle reports. */
	FString ToString() const;
};

/* Validates M5 parameters and evaluates ecological response without UObject access. */
class ADAPTIVEENVRUNTIME_API FAEM5ParameterService
{
public:
	/* Validates the numeric domain and cross-field ordering of typed values. */
	static FAEM5ValidationResult ValidateParameterSet(const FAEM5ParameterSet& Parameters);
	/* Fuses immutable M3/M4 inputs and advances only caller-owned M5 state. */
	static FAEEcologicalResponseSnapshot EvaluateResponse(double Exposure, double ExposureMaximum, double ConstraintPressureRatio, double HabitatSuitabilityRatio, double DeltaSimulationHours, const FAEM5ParameterSet& Parameters, FAEM5StateMemory& InOutState);
};
