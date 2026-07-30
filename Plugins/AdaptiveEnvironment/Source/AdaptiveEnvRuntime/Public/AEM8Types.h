#pragma once

#include "CoreMinimal.h"
#include "AEM7Types.h"
#include "AEM8Types.generated.h"

UENUM(BlueprintType)
enum class EAELSystemInputMode : uint8
{
	/* Uses only component-authored values and requires no upstream milestone. */
	Manual,
	/* Requires one valid M7 plant snapshot. */
	M7Driven,
	/* Uses M7 when available and otherwise keeps the manual test contract. */
	M7WithManualFallback
};

UENUM(BlueprintType)
enum class EAEPlantStructuralResponse : uint8
{
	/* Keeps the initialized mesh rigid and changes only visual state. */
	Rigid,
	/* Sheds leaves before applying material-driven soft-stem wilt. */
	SoftStemWilt,
	/* Allows only initialized branch-module breakpoints to detach. */
	HardStemBreakable
};

UENUM(BlueprintType)
enum class EAEBranchStructuralState : uint8
{
	/* Reports an undamaged initialized branch module. */
	Intact,
	/* Reports reversible visual stress without structural loss. */
	Stressed,
	/* Reports persistent dead wood that recovery cannot reverse. */
	DeadWood,
	/* Reports a persistently detached module. */
	Broken
};

UENUM(BlueprintType)
enum class EAELSystemTruncationReason : uint8
{
	/* Reports a complete generation within every configured limit. */
	None,
	/* Reports iteration clamping. */
	IterationLimit,
	/* Reports symbol expansion truncation. */
	SymbolLimit,
	/* Reports branch-segment truncation. */
	BranchLimit,
	/* Reports leaf-emitter truncation. */
	LeafEmitterLimit,
	/* Reports breakable-module truncation. */
	BreakableModuleLimit,
	/* Reports invalid grammar or Turtle stack input. */
	InvalidInput
};

USTRUCT(BlueprintType)
struct ADAPTIVEENVRUNTIME_API FAELSystemProductionRule
{
	GENERATED_BODY()

	/* Stores the single predecessor symbol rewritten in parallel. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Adaptive Environment|M8|Grammar")
	FString Predecessor = TEXT("F");
	/* Stores the successor string emitted for the predecessor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Adaptive Environment|M8|Grammar")
	FString Successor = TEXT("F");
	/* Defines deterministic weighted choice among rules sharing one predecessor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Adaptive Environment|M8|Grammar", meta = (ClampMin = "0.0"))
	float Weight = 1.0f;
};

USTRUCT(BlueprintType)
struct ADAPTIVEENVRUNTIME_API FAELSystemManualState
{
	GENERATED_BODY()

	/* Supplies normalized plant health when M7 is unavailable or bypassed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Adaptive Environment|M8|Manual", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HealthRatio = 1.0f;
	/* Supplies manual lifecycle direction for independent testing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Adaptive Environment|M8|Manual")
	EAEPlantLifecycleState LifecycleState = EAEPlantLifecycleState::Stable;
	/* Supplies normalized progress inside the selected manual lifecycle state. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Adaptive Environment|M8|Manual", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LifecycleProgressRatio = 1.0f;
	/* Supplies manual visibility when no M7 snapshot is consumed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Adaptive Environment|M8|Manual")
	bool bVisible = true;
};

USTRUCT(BlueprintType)
struct ADAPTIVEENVRUNTIME_API FAELSystemResolvedPlantState
{
	GENERATED_BODY()

	/* Identifies an optional M7 candidate or the component-owned manual plant. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M8")
	int64 StablePointId = 0;
	/* Identifies the active species contract. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M8")
	FName SpeciesId = NAME_None;
	/* Stores normalized effective health used only by M8 visual mapping. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M8")
	float HealthRatio = 1.0f;
	/* Stores effective lifecycle direction. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M8")
	EAEPlantLifecycleState LifecycleState = EAEPlantLifecycleState::Stable;
	/* Stores normalized progress in the effective lifecycle state. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M8")
	float LifecycleProgressRatio = 1.0f;
	/* Reports whether the complete M8 plant should be visible. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M8")
	bool bVisible = true;
	/* Identifies the source fixed simulation step, or zero for manual input. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M8")
	int64 SourceSimulationStep = 0;
};

USTRUCT(BlueprintType)
struct ADAPTIVEENVRUNTIME_API FAEBranchSegment
{
	GENERATED_BODY()

	/* Stores the deterministic segment identity. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M8")
	int64 BranchId = 0;
	/* Stores the deterministic parent segment identity, or zero for the root. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M8")
	int64 ParentBranchId = 0;
	/* Groups the segment into one initialized detachable module. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M8")
	int64 BranchModuleId = 0;
	/* Stores the plant-local segment start in centimetres. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M8")
	FVector StartLocationCm = FVector::ZeroVector;
	/* Stores the plant-local segment end in centimetres. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M8")
	FVector EndLocationCm = FVector::ZeroVector;
	/* Stores the generated base radius in centimetres. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M8")
	float StartRadiusCm = 1.0f;
	/* Stores the generated tip radius in centimetres. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M8")
	float EndRadiusCm = 0.9f;
	/* Stores the zero-based derivation depth that created the segment. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M8")
	int32 DerivationDepth = 0;
	/* Stores the zero-based branch nesting order. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M8")
	int32 BranchOrder = 0;
	/* Reports whether this segment contributes one Niagara leaf emitter. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M8")
	bool bSupportsLeaves = false;
};

USTRUCT(BlueprintType)
struct ADAPTIVEENVRUNTIME_API FAELeafEmitterDescriptor
{
	GENERATED_BODY()

	/* Stores a deterministic leaf-cluster identity rather than one leaf identity. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M8")
	int64 EmitterId = 0;
	/* Identifies the segment that owns this emitter. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M8")
	int64 OwnerBranchId = 0;
	/* Identifies the detachable module that owns this emitter. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M8")
	int64 OwnerBranchModuleId = 0;
	/* Stores the plant-local emitter transform. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M8")
	FTransform LocalTransform = FTransform::Identity;
	/* Stores the along-branch emission length in centimetres. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M8")
	float EmitterLengthCm = 0.0f;
	/* Stores the emission radius in centimetres. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M8")
	float EmitterRadiusCm = 0.0f;
	/* Stores the healthy target number of leaves per metre. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M8")
	float DensityPerMeter = 0.0f;
	/* Stores the deterministic Niagara seed for this cluster. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M8")
	int32 Seed = 0;
};

struct ADAPTIVEENVRUNTIME_API FAEM8MeshBuffers
{
	/* Stores plant-local vertices in centimetres. */
	TArray<FVector3f> Vertices;
	/* Stores triangle vertex indices in counter-clockwise order. */
	TArray<FIntVector> Triangles;
	/* Stores one unit normal per vertex. */
	TArray<FVector3f> Normals;
	/* Stores one primary UV coordinate per vertex. */
	TArray<FVector2f> UV0;
};

struct ADAPTIVEENVRUNTIME_API FAELSystemGeneratedPlant
{
	/* Stores the stable logical branch graph. */
	TArray<FAEBranchSegment> BranchSegments;
	/* Stores stable leaf-cluster emission contracts. */
	TArray<FAELeafEmitterDescriptor> LeafEmitters;
	/* Stores one fixed mesh buffer for each branch module, including module zero. */
	TMap<int64, FAEM8MeshBuffers> ModuleMeshes;
	/* Reports why generation stopped before consuming all requested work. */
	EAELSystemTruncationReason TruncationReason = EAELSystemTruncationReason::None;
	/* Stores the canonical deterministic content hash of logical output. */
	uint64 ContentHash = 0;
	/* Reports the final expanded symbol count. */
	int32 ExpandedSymbolCount = 0;
};

