#pragma once

#include "CoreMinimal.h"
#include "AdaptiveEnvDataAssets.h"

/* Classifies one parameter-package validation finding. */
enum class EAEValidationSeverity : uint8
{
	Info,
	Warning,
	Error
};

/* Describes one actionable evidence or parameter-package validation finding. */
struct ADAPTIVEENVRUNTIME_API FAEValidationIssue
{
	/* Identifies whether the finding blocks package publication. */
	EAEValidationSeverity Severity = EAEValidationSeverity::Error;

	/* Stores a stable machine-readable M2 validation code. */
	FString Code;

	/* Explains the invalid contract or research limitation. */
	FString Message;

	/* Identifies the source, evidence, transformation, synthesis, or parameter involved. */
	FGuid RelatedId;
};

/* Collects all findings produced by one deterministic M2 validation pass. */
struct ADAPTIVEENVRUNTIME_API FAEValidationResult
{
	/* Stores findings in deterministic validation order. */
	TArray<FAEValidationIssue> Issues;

	/* Returns true when no Error finding blocks use or publication. */
	bool IsValid() const;

	/* Counts findings with the requested severity. */
	int32 Count(EAEValidationSeverity Severity) const;

	/* Appends one finding while preserving deterministic validation order. */
	void Add(EAEValidationSeverity Severity, const TCHAR* Code, const FString& Message, const FGuid& RelatedId = FGuid());
};

/* Provides deterministic M2 transformation, synthesis, validation, hashing, export, and lookup operations. */
class ADAPTIVEENVRUNTIME_API FAEParameterPackageService
{
public:
	/* Recomputes one normalized value and rejects incompatible units or invalid inputs. */
	static bool ComputeTransformation(
		const FAEEvidenceRecord& Evidence,
		const FAEEvidenceTransformation& Transformation,
		double& OutValue,
		FString& OutError);

	/* Recomputes one scalar synthesis using stable contribution ordering. */
	static bool ComputeSynthesis(
		const FAEParameterSynthesis& Synthesis,
		double& OutValue,
		double& OutMinimum,
		double& OutMaximum,
		FString& OutError);

	/* Refreshes compact quality weights, synthesis values, derived values, and the package content hash. */
	static bool RefreshPackage(UAEParameterSynthesisAsset& Package, FString& OutError);

	/* Validates source, evidence, and transformation identity and numeric contracts. */
	static FAEValidationResult ValidateEvidenceAsset(const UAELiteratureEvidenceAsset& EvidenceAsset);

	/* Validates complete provenance links, synthesis results, effective values, versions, and content hash. */
	static FAEValidationResult ValidatePackage(
		const UAELiteratureEvidenceAsset& EvidenceAsset,
		const UAEParameterSynthesisAsset& Package);

	/* Builds canonical compact JSON with stable field and array ordering for hashing and export. */
	static bool BuildCanonicalPackageJson(const UAEParameterSynthesisAsset& Package, FString& OutJson, FString& OutError);

	/* Computes the lowercase SHA-256 digest of canonical effective package content. */
	static FString ComputeContentHash(const UAEParameterSynthesisAsset& Package, FString& OutError);

	/* Writes canonical parameter-package JSON to an explicitly supplied filesystem path. */
	static bool ExportPackageJson(
		const UAEParameterSynthesisAsset& Package,
		const FString& OutputPath,
		FString& OutError);

	/* Finds one effective runtime value together with its canonical unit and parameter version. */
	static bool TryGetEffectiveParameter(
		const UAEParameterSynthesisAsset& Package,
		FName ParameterName,
		double& OutRuntimeValue,
		FString& OutUnit,
		FString& OutParameterVersion);
};
