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

UENUM(BlueprintType)
enum class EAEBehaviourSubmitResult : uint8
{
	Accepted,
	InvalidAgent,
	InvalidLocation,
	InvalidTimestamp,
	InvalidTag,
	DuplicateSequence,
	OutOfOrder
};

UENUM(BlueprintType)
enum class EAEHeatmapDebugMode : uint8
{
	None,
	PassCount,
	TravelDistance,
	DwellTime,
	SmoothedActivity,
	Flow
};

USTRUCT(BlueprintType)
struct ADAPTIVEENVRUNTIME_API FAEBehaviourSample
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour")
	FGuid AgentId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour")
	FVector PreviousWorldLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour")
	FVector WorldLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour")
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour")
	double Timestamp = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour", meta = (ClampMin = "0.0"))
	float DeltaSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour", meta = (ClampMin = "0.0"))
	float TravelDistanceMeters = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour", meta = (ClampMin = "0.0"))
	float EventIntensity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour")
	int64 SequenceNumber = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour")
	bool bHasPreviousLocation = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour")
	FGameplayTag BehaviourTag;
};

USTRUCT(BlueprintType)
struct ADAPTIVEENVRUNTIME_API FAEHeatmapGridConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
	FVector2D WorldCenter = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
	FIntPoint Dimensions = FIntPoint(128, 128);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid", meta = (ClampMin = "1.0"))
	float CellSizeCm = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid", meta = (ClampMin = "0"))
	int32 KernelRadiusCells = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid", meta = (ClampMin = "0.01"))
	float KernelSigma = 0.75f;
};

USTRUCT(BlueprintType)
struct ADAPTIVEENVRUNTIME_API FAEBehaviourCellSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Grid")
	FIntPoint Coordinate = FIntPoint::ZeroValue;

	UPROPERTY(BlueprintReadOnly, Category = "Grid")
	FVector WorldCenter = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Behaviour")
	float PassCount = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Behaviour")
	float TravelDistanceMeters = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Behaviour")
	float DwellSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Behaviour")
	float SprintDistanceMeters = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Behaviour")
	float CollectEventCount = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Behaviour")
	float CombatEventCount = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Behaviour")
	FVector2D FlowDirection = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Behaviour")
	float FlowMagnitude = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Behaviour")
	float SmoothedActivity = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Behaviour")
	double LastUpdatedTime = 0.0;
};

USTRUCT(BlueprintType)
struct ADAPTIVEENVRUNTIME_API FAEBehaviourGridStats
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Statistics")
	int64 AcceptedSampleCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Statistics")
	int64 RejectedSampleCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Statistics")
	int64 DuplicateSampleCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Statistics")
	int64 OutOfOrderSampleCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Statistics")
	int64 OutOfBoundsSampleCount = 0;
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
