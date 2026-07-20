#pragma once

#include "CoreMinimal.h"
#include "AEM3Types.h"

struct FAEParameterBlockView;
struct FAEParameterBundleIdentity;

/* Describes one blocking M3 parameter-contract finding. */
struct ADAPTIVEENVRUNTIME_API FAEM3ValidationIssue
{
	/* Stores a stable machine-readable M3 validation code. */
	FString Code;
	/* Explains the invalid parameter-package contract. */
	FString Message;
	/* Identifies the affected parameter when one exists. */
	FName ParameterName;
};

/* Collects deterministic findings from one M3 parameter validation pass. */
struct ADAPTIVEENVRUNTIME_API FAEM3ValidationResult
{
	/* Stores findings in stable validation order. */
	TArray<FAEM3ValidationIssue> Issues;

	/* Returns true when no finding blocks M3 use. */
	bool IsValid() const { return Issues.IsEmpty(); }
	/* Appends one stable blocking finding. */
	void Add(const TCHAR* Code, const FString& Message, FName ParameterName = NAME_None);
	/* Joins all findings into one concise initialization message. */
	FString ToString() const;
};

/* Builds one immutable validated M3 parameter snapshot from an M2 package. */
class ADAPTIVEENVRUNTIME_API FAEM3ParameterService
{
public:
	/* Maps one validated M3 block into grouped values and applies cross-parameter gates. */
	static FAEM3ValidationResult BuildParameterSet(
		const FAEParameterBlockView& Block,
		const FAEParameterBundleIdentity& BundleIdentity,
		FAEM3ParameterSet& OutParameterSet);

	/* Validates numeric ranges and relationships on an already populated parameter set. */
	static FAEM3ValidationResult ValidateParameterSet(const FAEM3ParameterSet& ParameterSet);
};
