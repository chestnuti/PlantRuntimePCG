#pragma once

#include "CoreMinimal.h"
#include "AEM7Types.h"

struct ADAPTIVEENVRUNTIME_API FAEPlantDistributionConfig
{
	/* Defines the valid half-open world XY sampling bounds in centimetres. */
	FBox2D WorldBounds = FBox2D(EForceInit::ForceInit);
	/* Defines shared Grid width and height for Cell grouping. */
	FIntPoint GridDimensions = FIntPoint::ZeroValue;
	/* Identifies the species and participates in deterministic hashing. */
	FName SpeciesId = NAME_None;
	/* Controls reproducible point generation and stable identities. */
	int32 Seed = 1337;
	/* Defines the global Poisson exclusion distance in centimetres. */
	float MinimumSpacingCm = 100.0f;
	/* Defines the requested maximum pool density per square metre. */
	float MaximumInstancesPerSquareMeter = 0.01f;
	/* Bounds rejection work relative to the requested candidate count. */
	int32 AttemptsPerExpectedPoint = 20;
	/* Caps generated candidates for one species. */
	int32 MaximumCandidateCount = 50000;
	/* Defines deterministic individual health variation in ratio units. */
	float HealthVariationAmplitude = 0.0f;
};

class ADAPTIVEENVRUNTIME_API FAEPlantDistributionService
{
public:
	/* Builds the immutable maximum candidate pool with global cross-Cell exclusion. */
	static bool GenerateStableCandidatePool(
		const FAEPlantDistributionConfig& Config,
		TArray<FAEM7CandidatePoint>& OutCandidates,
		FString& OutError);
	/* Collects unique valid row-major Cell indices occupied by one candidate pool. */
	static void CollectOccupiedCellIndices(
		const TArray<FAEM7CandidatePoint>& Candidates,
		const FIntPoint& GridDimensions,
		TArray<int32>& OutCellIndices);
	/* Applies the stable candidate identity avalanche mix. */
	static uint64 MixHash(uint64 Value);
	/* Converts stable hash bits into a normalized selection key. */
	static float HashToUnitFloat(uint64 Value);
};
