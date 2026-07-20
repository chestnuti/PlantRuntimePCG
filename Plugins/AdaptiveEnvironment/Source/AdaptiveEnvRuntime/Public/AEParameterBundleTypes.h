#pragma once

#include "CoreMinimal.h"
#include "AEParameterBundleTypes.generated.h"

/* Identifies the effective-value layer published by the offline M2 tool. */
UENUM(BlueprintType)
enum class EAEParameterOriginLayer : uint8
{
	Synthesized,
	ResearcherCalibrated,
	ExperimentOverride
};

/* Stores one published scalar parameter and its compact research provenance. */
USTRUCT(BlueprintType)
struct ADAPTIVEENVRUNTIME_API FAEPublishedParameter
{
	GENERATED_BODY()

	/* Uniquely identifies this parameter record across published bundles. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Adaptive Environment|Parameters")
	FGuid ParameterId;

	/* Stores the case-sensitive model parameter name. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Adaptive Environment|Parameters")
	FName Name;

	/* Stores the exact canonical unit consumed by the model contract. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Adaptive Environment|Parameters")
	FString Unit;

	/* Preserves the synthesized evidence-based value before calibration or override. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Adaptive Environment|Parameters")
	double EvidenceBasedValue = 0.0;

	/* Stores the finite value consumed by the runtime model. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Adaptive Environment|Parameters")
	double EffectiveValue = 0.0;

	/* Defines the inclusive plausible lower bound in the canonical unit. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Adaptive Environment|Parameters")
	double PlausibleMinimum = 0.0;

	/* Defines the inclusive plausible upper bound in the canonical unit. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Adaptive Environment|Parameters")
	double PlausibleMaximum = 0.0;

	/* Identifies the semantic version of this parameter record. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Adaptive Environment|Parameters")
	FString ParameterVersion;

	/* Identifies which reviewed layer supplied EffectiveValue. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Adaptive Environment|Parameters")
	EAEParameterOriginLayer OriginLayer = EAEParameterOriginLayer::Synthesized;
};

/* Stores one versioned model contract and its canonical parameter records. */
USTRUCT(BlueprintType)
struct ADAPTIVEENVRUNTIME_API FAEParameterBlock
{
	GENERATED_BODY()

	/* Uniquely identifies this model-contract block. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Adaptive Environment|Parameters")
	FGuid BlockId;

	/* Identifies the exact runtime model contract implemented by this block. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Adaptive Environment|Parameters")
	FName ModelContract;

	/* Identifies the semantic version of this block content. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Adaptive Environment|Parameters")
	FString BlockVersion;

	/* Stores parameters in canonical case-sensitive name order. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Adaptive Environment|Parameters")
	TArray<FAEPublishedParameter> Parameters;

	/* Stores the lowercase SHA-256 digest of the canonical block without this field. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Adaptive Environment|Parameters")
	FString BlockHash;
};

/* Identifies one validated bundle independently from its UObject lifetime. */
struct ADAPTIVEENVRUNTIME_API FAEParameterBundleIdentity
{
	/* Uniquely identifies the published bundle lineage. */
	FGuid BundleId;
	/* Stores the bundle semantic version. */
	FString SemanticVersion;
	/* Stores the canonical bundle SHA-256 digest. */
	FString ContentHash;
};

/* Provides a non-owning immutable view over one parameter block. */
struct ADAPTIVEENVRUNTIME_API FAEParameterBlockView
{
	/* Points to the validated block owned by a bundle asset. */
	const FAEParameterBlock* Block = nullptr;

	/* Returns whether this view references a block. */
	bool IsValid() const { return Block != nullptr; }
};

/* Describes one stable blocking bundle-contract finding. */
struct ADAPTIVEENVRUNTIME_API FAEParameterBundleIssue
{
	/* Stores a stable machine-readable AE-BUNDLE validation code. */
	FString Code;
	/* Explains the rejected field or relationship. */
	FString Message;
};

/* Collects deterministic findings from one bundle validation pass. */
struct ADAPTIVEENVRUNTIME_API FAEParameterBundleValidationResult
{
	/* Stores findings in validation order. */
	TArray<FAEParameterBundleIssue> Issues;

	/* Returns true when no finding blocks bundle use. */
	bool IsValid() const { return Issues.IsEmpty(); }
	/* Adds one stable blocking finding. */
	void Add(const TCHAR* Code, const FString& Message);
	/* Joins all findings into one concise diagnostic message. */
	FString ToString() const;
};
