#pragma once

#include "CoreMinimal.h"
#include "AEParameterBundleTypes.h"

class UAEPublishedParameterBundleAsset;

/* Validates and queries immutable Published Parameter Bundle v1 assets. */
class ADAPTIVEENVRUNTIME_API FAEParameterBundleService
{
public:
	/* Returns the exact M3 model-contract identifier. */
	static FName M3ModelContract();
	/* Returns the exact M4 model-contract identifier. */
	static FName M4ModelContract();
	/* Returns the exact M5 model-contract identifier. */
	static FName M5ModelContract();
	/* Validates header, identity, ordering, records, model contracts, and canonical hashes. */
	static FAEParameterBundleValidationResult ValidateBundle(const UAEPublishedParameterBundleAsset& Bundle);
	/* Finds one validated immutable block by exact model contract. */
	static bool FindBlock(const UAEPublishedParameterBundleAsset& Bundle, FName ModelContract, FAEParameterBlockView& OutBlock);
	/* Builds the canonical block JSON used by Python and Unreal SHA-256 checks. */
	static bool BuildCanonicalBlockJson(const FAEParameterBlock& Block, FString& OutJson, FString& OutError);
	/* Builds the canonical bundle JSON and optionally omits ContentHash for digest calculation. */
	static bool BuildCanonicalBundleJson(const UAEPublishedParameterBundleAsset& Bundle, bool bIncludeContentHash, FString& OutJson, FString& OutError);
	/* Computes the lowercase SHA-256 digest of one canonical block. */
	static FString ComputeBlockHash(const FAEParameterBlock& Block, FString& OutError);
	/* Computes the lowercase SHA-256 digest of one canonical bundle. */
	static FString ComputeContentHash(const UAEPublishedParameterBundleAsset& Bundle, FString& OutError);
};
