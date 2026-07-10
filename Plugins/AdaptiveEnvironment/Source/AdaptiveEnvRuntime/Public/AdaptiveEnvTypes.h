#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameplayTagContainer.h"
#include "AdaptiveEnvTypes.generated.h"

UENUM(BlueprintType)
enum class EAERegionState : uint8
{
	Unused,
	Active,
	Overused,
	Recovering
};

UENUM(BlueprintType)
enum class EAEEvidenceType : uint8
{
	PointEstimate,
	Range,
	TimeSeries,
	ResponseCurve,
	RegressionCoefficient,
	QualitativeConclusion
};

UENUM(BlueprintType)
enum class EAESynthesisMethod : uint8
{
	ArithmeticMean,
	WeightedMean,
	Median,
	ConfidenceWeightedMean,
	SampleSizeWeightedMean,
	ExpertJudgement,
	CurveFitting,
	ManualCalibration
};

USTRUCT(BlueprintType)
struct ADAPTIVEENVRUNTIME_API FAEBehaviourSample
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour")
	FGuid AgentId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour")
	FVector WorldLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour")
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour")
	double Timestamp = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour", meta = (ClampMin = "0.0"))
	float DeltaSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour", meta = (ClampMin = "0.0"))
	float EventIntensity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour")
	FGameplayTag BehaviourTag;
};

USTRUCT(BlueprintType)
struct ADAPTIVEENVRUNTIME_API FAELiteratureSource
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evidence")
	FGuid SourceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evidence")
	FString CitationKey;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evidence")
	FString Title;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evidence")
	FString Authors;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evidence")
	int32 PublicationYear = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evidence")
	FString DOI;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evidence")
	FString StudyLocation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evidence")
	FString StudiedSpecies;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evidence")
	FString StudyDesign;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evidence")
	FString Notes;
};

USTRUCT(BlueprintType)
struct ADAPTIVEENVRUNTIME_API FAEEvidenceRecord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evidence")
	FGuid EvidenceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evidence")
	FGuid SourceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evidence")
	FString ParameterConcept;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evidence")
	EAEEvidenceType EvidenceType = EAEEvidenceType::PointEstimate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evidence")
	double RawValue = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evidence")
	double RawMinimum = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evidence")
	double RawMaximum = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evidence")
	FString RawUnit;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evidence")
	FString Species;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evidence")
	FString PageOrTable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evidence")
	FString ExtractionNotes;
};

USTRUCT(BlueprintType)
struct ADAPTIVEENVRUNTIME_API FAEEvidenceTransformation
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transformation")
	FGuid TransformationId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transformation")
	FGuid EvidenceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transformation")
	FString MethodName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transformation")
	FString MethodVersion = TEXT("1.0.0");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transformation")
	FString InputUnit;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transformation")
	FString OutputUnit;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transformation")
	FString Formula;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transformation")
	TMap<FName, double> Constants;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transformation")
	double NormalizedValue = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transformation")
	FString Assumptions;
};

USTRUCT(BlueprintType)
struct ADAPTIVEENVRUNTIME_API FAESynthesisContribution
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synthesis")
	FGuid EvidenceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synthesis")
	FGuid TransformationId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synthesis")
	double NormalizedValue = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synthesis", meta = (ClampMin = "0.0"))
	double AssignedWeight = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synthesis")
	bool bIncluded = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synthesis")
	FString InclusionReason;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synthesis")
	FString ExclusionReason;
};

USTRUCT(BlueprintType)
struct ADAPTIVEENVRUNTIME_API FAEParameterSynthesis
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synthesis")
	FGuid SynthesisId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synthesis")
	FString SynthesisVersion = TEXT("1.0.0");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synthesis")
	FName TargetParameter;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synthesis")
	EAESynthesisMethod Method = EAESynthesisMethod::WeightedMean;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synthesis")
	TArray<FAESynthesisContribution> Contributions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synthesis")
	FString Formula;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synthesis")
	FString WeightingRationale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synthesis")
	double ComputedValue = 0.0;
};

USTRUCT(BlueprintType)
struct ADAPTIVEENVRUNTIME_API FAEDerivedParameter
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameter")
	FName ParameterName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameter")
	FGuid SynthesisId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameter")
	FString ParameterVersion = TEXT("1.0.0");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameter")
	double EvidenceBasedValue = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameter")
	double RuntimeValue = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameter")
	double PlausibleMinimum = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameter")
	double PlausibleMaximum = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameter")
	bool bHasManualAdjustment = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameter")
	FString AdjustmentReason;
};

USTRUCT(BlueprintType)
struct ADAPTIVEENVRUNTIME_API FAEParameterPackageMetadata
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Version")
	FGuid PackageId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Version")
	FString SemanticVersion = TEXT("1.0.0");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Version")
	FString ParentVersion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Version")
	FString CreatedBy;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Version")
	FString ReviewStatus = TEXT("Draft");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Version")
	FString ChangeSummary;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Version")
	FString ContentHash;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Version")
	int32 SchemaVersion = 1;
};
