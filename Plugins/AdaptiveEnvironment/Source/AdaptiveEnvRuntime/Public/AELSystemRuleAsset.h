#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AEM8Types.h"
#include "AELSystemRuleAsset.generated.h"

UCLASS(BlueprintType)
class ADAPTIVEENVRUNTIME_API UAELSystemRuleAsset final : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/* Stores the stable species key used by M7 compatibility and cache identity. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FName SpeciesId = TEXT("RepresentativePlant");
	/* Stores the semantic rule-package version. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FString SemanticVersion = TEXT("1.0.0");
	/* Stores the initial grammar string. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grammar")
	FString Axiom = TEXT("F[+F]F[-F]FL");
	/* Stores context-free weighted production alternatives. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grammar")
	TArray<FAELSystemProductionRule> ProductionRules;
	/* Defines the requested number of parallel grammar rewrites. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grammar", meta = (ClampMin = "0", ClampMax = "8"))
	int32 IterationCount = 3;
	/* Defines one forward Turtle step in centimetres. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Geometry", meta = (ClampMin = "0.1"))
	float SegmentLengthCm = 35.0f;
	/* Defines the root branch radius in centimetres. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Geometry", meta = (ClampMin = "0.01"))
	float RootRadiusCm = 3.0f;
	/* Multiplies branch radius after each generated segment. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Geometry", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float RadiusDecayPerSegment = 0.94f;
	/* Defines Turtle rotation magnitude in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Geometry", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float BranchAngleDegrees = 25.0f;
	/* Defines vertices around each circular sweep ring. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Geometry", meta = (ClampMin = "3", ClampMax = "24"))
	int32 RadialSegments = 8;
	/* Defines the healthy leaf-cluster target per branch metre. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leaves", meta = (ClampMin = "0.0"))
	float LeafDensityPerMeter = 12.0f;
	/* Selects rigid, soft-wilt, or pre-segmented hard-branch response. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lifecycle")
	EAEPlantStructuralResponse StructuralResponse = EAEPlantStructuralResponse::HardStemBreakable;
	/* Caps grammar rewrites independently of the authored iteration count. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Safety", meta = (ClampMin = "0", ClampMax = "12"))
	int32 MaxIterations = 6;
	/* Caps expanded symbols before Turtle interpretation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Safety", meta = (ClampMin = "1"))
	int32 MaxSymbolCount = 20000;
	/* Caps generated branch segments. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Safety", meta = (ClampMin = "1"))
	int32 MaxBranchSegments = 4096;
	/* Caps generated leaf emitters. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Safety", meta = (ClampMin = "0"))
	int32 MaxLeafEmitters = 1024;
	/* Caps concrete HISM leaf instances created from all leaf regions. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Safety", meta = (ClampMin = "0"))
	int32 MaxLeafInstances = 8192;
	/* Caps initialized detachable branch modules per plant. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Safety", meta = (ClampMin = "0", ClampMax = "32"))
	int32 MaxBreakableBranchModules = 8;
	/* Restricts detachable roots to this branch nesting order. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Safety", meta = (ClampMin = "0", ClampMax = "8"))
	int32 MaximumBreakableBranchOrder = 2;

	/* Validates grammar, geometry, and safety limits without mutating the asset. */
	bool Validate(FString& OutError) const;
	/* Computes a stable hash from every effective rule and generation parameter. */
	uint64 ComputeContentHash() const;
};
