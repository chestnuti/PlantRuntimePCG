#pragma once

#include "CoreMinimal.h"

class UAEPublishedParameterBundleAsset;

/* Strictly parses Published Parameter Bundle v1 JSON into a transient asset. */
class FAEJsonBundleReader
{
public:
	/* Reads UTF-8 JSON, rejects unknown or missing fields, and populates one candidate. */
	static bool ReadFile(const FString& Filename, UAEPublishedParameterBundleAsset& OutBundle, FString& OutError);
};
