#pragma once

#include "CoreMinimal.h"
#include "AEM8Types.h"

class UAELSystemRuleAsset;

class ADAPTIVEENVRUNTIME_API FAELSystemGenerator
{
public:
	/* Generates one deterministic logical plant and fixed module mesh buffers. */
	static bool Generate(
		const UAELSystemRuleAsset& RuleAsset,
		int32 Seed,
		int64 StablePlantId,
		FAELSystemGeneratedPlant& OutPlant,
		FString& OutError);

private:
	/* Expands one context-free grammar with deterministic weighted alternatives. */
	static bool ExpandGrammar(
		const UAELSystemRuleAsset& RuleAsset,
		FRandomStream& Random,
		FString& OutSymbols,
		EAELSystemTruncationReason& OutTruncation,
		FString& OutError);
	/* Converts expanded symbols into a stable branch graph and leaf emitters. */
	static bool InterpretTurtle(
		const UAELSystemRuleAsset& RuleAsset,
		const FString& Symbols,
		int32 Seed,
		int64 StablePlantId,
		FAELSystemGeneratedPlant& OutPlant,
		FString& OutError);
	/* Builds one capped circular sweep for all segments assigned to one module. */
	static void BuildModuleMesh(
		const UAELSystemRuleAsset& RuleAsset,
		const TArray<FAEBranchSegment>& Segments,
		int64 ModuleId,
		FAEM8MeshBuffers& OutMesh);
};

