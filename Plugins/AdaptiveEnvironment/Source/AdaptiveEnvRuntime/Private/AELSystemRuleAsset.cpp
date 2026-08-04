#include "AELSystemRuleAsset.h"

/* Validate one authored M8 rule package before deterministic generation. */
bool UAELSystemRuleAsset::Validate(FString& OutError) const
{
	OutError.Reset();
	if (SpeciesId.IsNone() || Axiom.IsEmpty())
	{
		OutError = TEXT("SpeciesId and Axiom are required.");
		return false;
	}
	if (IterationCount < 0 || MaxIterations < 0 || MaxSymbolCount <= 0
		|| MaxBranchSegments <= 0 || MaxLeafEmitters < 0 || MaxLeafInstances < 0
		|| MaxBreakableBranchModules < 0)
	{
		OutError = TEXT("M8 safety limits are invalid.");
		return false;
	}
	if (!FMath::IsFinite(SegmentLengthCm) || SegmentLengthCm <= 0.0f
		|| !FMath::IsFinite(RootRadiusCm) || RootRadiusCm <= 0.0f
		|| !FMath::IsFinite(RadiusDecayPerSegment) || RadiusDecayPerSegment <= 0.0f
		|| RadiusDecayPerSegment > 1.0f || RadialSegments < 3)
	{
		OutError = TEXT("M8 geometry parameters are invalid.");
		return false;
	}
	for (const FAELSystemProductionRule& Rule : ProductionRules)
	{
		if (Rule.Predecessor.Len() != 1 || Rule.Successor.IsEmpty()
			|| !FMath::IsFinite(Rule.Weight) || Rule.Weight <= 0.0f)
		{
			OutError = TEXT("Every production requires one predecessor, a successor, and positive finite weight.");
			return false;
		}
	}
	return true;
}

/* Hash canonical M8 asset content for cache invalidation and replay evidence. */
uint64 UAELSystemRuleAsset::ComputeContentHash() const
{
	uint64 Hash = GetTypeHash(SpeciesId);
	Hash = HashCombineFast(Hash, GetTypeHash(SemanticVersion));
	Hash = HashCombineFast(Hash, GetTypeHash(Axiom));
	Hash = HashCombineFast(Hash, GetTypeHash(IterationCount));
	Hash = HashCombineFast(Hash, GetTypeHash(SegmentLengthCm));
	Hash = HashCombineFast(Hash, GetTypeHash(RootRadiusCm));
	Hash = HashCombineFast(Hash, GetTypeHash(RadiusDecayPerSegment));
	Hash = HashCombineFast(Hash, GetTypeHash(BranchAngleDegrees));
	Hash = HashCombineFast(Hash, GetTypeHash(RadialSegments));
	Hash = HashCombineFast(Hash, GetTypeHash(LeafDensityPerMeter));
	Hash = HashCombineFast(Hash, GetTypeHash(MaxLeafInstances));
	Hash = HashCombineFast(Hash, GetTypeHash(static_cast<uint8>(StructuralResponse)));
	for (const FAELSystemProductionRule& Rule : ProductionRules)
	{
		Hash = HashCombineFast(Hash, GetTypeHash(Rule.Predecessor));
		Hash = HashCombineFast(Hash, GetTypeHash(Rule.Successor));
		Hash = HashCombineFast(Hash, GetTypeHash(Rule.Weight));
	}
	return Hash;
}
