#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameplayTagContainer.h"
#include "AdaptiveEnvTypes.generated.h"

/** Describes the ecological usage state assigned to a region. */
UENUM(BlueprintType)
enum class EAERegionState : uint8
{
	Unused,    // No meaningful recent activity.
	Active,    // Activity remains within the supported range.
	Overused,  // Activity exceeds the ecological threshold.
	Recovering // Activity has stopped while the region restores.
};

/** Describes the form of an observation extracted from literature. */
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

/** Selects the method used to combine evidence contributions. */
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

/** Reports whether a behaviour sample was accepted or rejected. */
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

/** Selects the behaviour metric drawn by the debug renderer. */
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

/** Captures one timestamped agent observation or discrete event. */
USTRUCT(BlueprintType)
struct ADAPTIVEENVRUNTIME_API FAEBehaviourSample
{
	GENERATED_BODY()

	/** Uniquely identifies the sampled agent. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour")
	FGuid AgentId;

	/** Stores the previous sampled world position in centimetres. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour")
	FVector PreviousWorldLocation = FVector::ZeroVector;

	/** Stores the current sampled world position in centimetres. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour")
	FVector WorldLocation = FVector::ZeroVector;

	/** Stores the current world velocity in centimetres per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour")
	FVector Velocity = FVector::ZeroVector;

	/** Stores the simulation timestamp in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour")
	double Timestamp = 0.0;

	/** Stores elapsed time since the previous sample in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour", meta = (ClampMin = "0.0"))
	float DeltaSeconds = 0.0f;

	/** Stores three-dimensional travel distance since the previous sample in metres. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour", meta = (ClampMin = "0.0"))
	float TravelDistanceMeters = 0.0f;

	/** Scales the contribution of a discrete behaviour event. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour", meta = (ClampMin = "0.0"))
	float EventIntensity = 1.0f;

	/** Orders samples produced by one agent. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour")
	int64 SequenceNumber = 0;

	/** Indicates whether the previous position is valid. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour")
	bool bHasPreviousLocation = false;

	/** Classifies the captured movement or event. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour")
	FGameplayTag BehaviourTag;
};

/** Defines the size, placement, and smoothing of a behaviour grid. */
USTRUCT(BlueprintType)
struct ADAPTIVEENVRUNTIME_API FAEHeatmapGridConfig
{
	GENERATED_BODY()

	/** Defines the grid centre in world XY centimetres. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
	FVector2D WorldCenter = FVector2D::ZeroVector;

	/** Defines the grid width and height in cells. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
	FIntPoint Dimensions = FIntPoint(128, 128);

	/** Defines one square cell edge in centimetres. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid", meta = (ClampMin = "1.0"))
	float CellSizeCm = 100.0f;

	/** Defines the Gaussian smoothing radius in cells. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid", meta = (ClampMin = "0"))
	int32 KernelRadiusCells = 1;

	/** Defines the Gaussian standard deviation in cells. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid", meta = (ClampMin = "0.01"))
	float KernelSigma = 0.75f;
};

/** Exposes an immutable snapshot of one behaviour grid cell. */
USTRUCT(BlueprintType)
struct ADAPTIVEENVRUNTIME_API FAEBehaviourCellSnapshot
{
	GENERATED_BODY()

	/** Stores the integer XY cell coordinate. */
	UPROPERTY(BlueprintReadOnly, Category = "Grid")
	FIntPoint Coordinate = FIntPoint::ZeroValue;

	/** Stores the cell centre in world centimetres. */
	UPROPERTY(BlueprintReadOnly, Category = "Grid")
	FVector WorldCenter = FVector::ZeroVector;

	/** Counts agent entries into this cell. */
	UPROPERTY(BlueprintReadOnly, Category = "Behaviour")
	float PassCount = 0.0f;

	/** Accumulates movement distance assigned to this cell in metres. */
	UPROPERTY(BlueprintReadOnly, Category = "Behaviour")
	float TravelDistanceMeters = 0.0f;

	/** Accumulates low-speed dwelling time in seconds. */
	UPROPERTY(BlueprintReadOnly, Category = "Behaviour")
	float DwellSeconds = 0.0f;

	/** Accumulates sprint movement distance in metres. */
	UPROPERTY(BlueprintReadOnly, Category = "Behaviour")
	float SprintDistanceMeters = 0.0f;

	/** Accumulates weighted resource collection events. */
	UPROPERTY(BlueprintReadOnly, Category = "Behaviour")
	float CollectEventCount = 0.0f;

	/** Accumulates weighted combat events. */
	UPROPERTY(BlueprintReadOnly, Category = "Behaviour")
	float CombatEventCount = 0.0f;

	/** Stores the normalized dominant horizontal travel direction. */
	UPROPERTY(BlueprintReadOnly, Category = "Behaviour")
	FVector2D FlowDirection = FVector2D::ZeroVector;

	/** Stores directional consistency from zero to one. */
	UPROPERTY(BlueprintReadOnly, Category = "Behaviour")
	float FlowMagnitude = 0.0f;

	/** Stores spatially smoothed raw activity around this cell. */
	UPROPERTY(BlueprintReadOnly, Category = "Behaviour")
	float SmoothedActivity = 0.0f;

	/** Stores the latest contributing simulation timestamp in seconds. */
	UPROPERTY(BlueprintReadOnly, Category = "Behaviour")
	double LastUpdatedTime = 0.0;
};

/** Tracks accepted and rejected behaviour sample totals. */
USTRUCT(BlueprintType)
struct ADAPTIVEENVRUNTIME_API FAEBehaviourGridStats
{
	GENERATED_BODY()

	/** Counts samples accepted by the grid. */
	UPROPERTY(BlueprintReadOnly, Category = "Statistics")
	int64 AcceptedSampleCount = 0;

	/** Counts all samples rejected by the grid or queue. */
	UPROPERTY(BlueprintReadOnly, Category = "Statistics")
	int64 RejectedSampleCount = 0;

	/** Counts samples rejected for duplicate sequence numbers. */
	UPROPERTY(BlueprintReadOnly, Category = "Statistics")
	int64 DuplicateSampleCount = 0;

	/** Counts samples rejected for sequence or timestamp order. */
	UPROPERTY(BlueprintReadOnly, Category = "Statistics")
	int64 OutOfOrderSampleCount = 0;

	/** Counts valid samples that fall outside the grid. */
	UPROPERTY(BlueprintReadOnly, Category = "Statistics")
	int64 OutOfBoundsSampleCount = 0;
};

/** Describes one citable literature source and its study context. */
USTRUCT(BlueprintType)
struct ADAPTIVEENVRUNTIME_API FAELiteratureSource
{
	GENERATED_BODY()

	/** Uniquely identifies the literature source. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evidence")
	FGuid SourceId;

	/** Stores the bibliography key used by the thesis project. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evidence")
	FString CitationKey;

	/** Stores the published work title. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evidence")
	FString Title;

	/** Stores the formatted author list. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evidence")
	FString Authors;

	/** Stores the publication year. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evidence")
	int32 PublicationYear = 0;

	/** Stores the digital object identifier when available. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evidence")
	FString DOI;

	/** Describes the geographic or controlled study location. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evidence")
	FString StudyLocation;

	/** Lists species represented by the study. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evidence")
	FString StudiedSpecies;

	/** Describes the experimental or observational design. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evidence")
	FString StudyDesign;

	/** Stores additional source-level interpretation notes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evidence")
	FString Notes;
};

/** Stores one value or conclusion extracted from a literature source. */
USTRUCT(BlueprintType)
struct ADAPTIVEENVRUNTIME_API FAEEvidenceRecord
{
	GENERATED_BODY()

	/** Uniquely identifies the extracted evidence record. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evidence")
	FGuid EvidenceId;

	/** Links this record to its literature source. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evidence")
	FGuid SourceId;

	/** Names the parameter concept represented by the evidence. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evidence")
	FString ParameterConcept;

	/** Describes the statistical or qualitative evidence form. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evidence")
	EAEEvidenceType EvidenceType = EAEEvidenceType::PointEstimate;

	/** Stores the primary value in its original unit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evidence")
	double RawValue = 0.0;

	/** Stores the original lower bound when the evidence is a range. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evidence")
	double RawMinimum = 0.0;

	/** Stores the original upper bound when the evidence is a range. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evidence")
	double RawMaximum = 0.0;

	/** Stores the unit used by the source. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evidence")
	FString RawUnit;

	/** Identifies the species represented by this record. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evidence")
	FString Species;

	/** Locates the value by page, table, figure, or equation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evidence")
	FString PageOrTable;

	/** Records extraction assumptions and interpretation details. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evidence")
	FString ExtractionNotes;
};

/** Describes a reproducible normalization applied to one evidence record. */
USTRUCT(BlueprintType)
struct ADAPTIVEENVRUNTIME_API FAEEvidenceTransformation
{
	GENERATED_BODY()

	/** Uniquely identifies the transformation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transformation")
	FGuid TransformationId;

	/** Links the transformation to its input evidence. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transformation")
	FGuid EvidenceId;

	/** Names the normalization or conversion method. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transformation")
	FString MethodName;

	/** Versions the method implementation or rule. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transformation")
	FString MethodVersion = TEXT("1.0.0");

	/** Stores the unit expected by the transformation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transformation")
	FString InputUnit;

	/** Stores the normalized output unit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transformation")
	FString OutputUnit;

	/** Stores the formula or rule used to calculate the output. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transformation")
	FString Formula;

	/** Stores named constants required by the formula. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transformation")
	TMap<FName, double> Constants;

	/** Stores the calculated value in the output unit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transformation")
	double NormalizedValue = 0.0;

	/** Records assumptions required by the transformation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transformation")
	FString Assumptions;
};

/** Records one evidence value and weight used by a synthesis. */
USTRUCT(BlueprintType)
struct ADAPTIVEENVRUNTIME_API FAESynthesisContribution
{
	GENERATED_BODY()

	/** Links the contribution to its source evidence. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synthesis")
	FGuid EvidenceId;

	/** Links the contribution to its normalized transformation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synthesis")
	FGuid TransformationId;

	/** Stores the normalized value used by the synthesis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synthesis")
	double NormalizedValue = 0.0;

	/** Stores the non-negative weight assigned by the selected method. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synthesis", meta = (ClampMin = "0.0"))
	double AssignedWeight = 1.0;

	/** Controls whether this contribution affects the result. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synthesis")
	bool bIncluded = true;

	/** Explains why an active contribution is applicable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synthesis")
	FString InclusionReason;

	/** Explains why an inactive contribution is excluded. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synthesis")
	FString ExclusionReason;
};

/** Describes how multiple evidence contributions produce one parameter. */
USTRUCT(BlueprintType)
struct ADAPTIVEENVRUNTIME_API FAEParameterSynthesis
{
	GENERATED_BODY()

	/** Uniquely identifies the synthesis record. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synthesis")
	FGuid SynthesisId;

	/** Versions the synthesis method, inputs, and weights. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synthesis")
	FString SynthesisVersion = TEXT("1.0.0");

	/** Names the runtime parameter produced by the synthesis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synthesis")
	FName TargetParameter;

	/** Selects the algorithm used to combine contributions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synthesis")
	EAESynthesisMethod Method = EAESynthesisMethod::WeightedMean;

	/** Stores all included and excluded evidence contributions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synthesis")
	TArray<FAESynthesisContribution> Contributions;

	/** Stores the exact combination formula or algorithm description. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synthesis")
	FString Formula;

	/** Explains how contribution weights were selected. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synthesis")
	FString WeightingRationale;

	/** Stores the value calculated from active contributions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synthesis")
	double ComputedValue = 0.0;
};

/** Stores one evidence-based parameter and its effective runtime value. */
USTRUCT(BlueprintType)
struct ADAPTIVEENVRUNTIME_API FAEDerivedParameter
{
	GENERATED_BODY()

	/** Names the parameter consumed by runtime systems. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameter")
	FName ParameterName;

	/** Links the parameter to its synthesis record. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameter")
	FGuid SynthesisId;

	/** Versions the parameter value and adjustment history. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameter")
	FString ParameterVersion = TEXT("1.0.0");

	/** Stores the unadjusted value produced by evidence synthesis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameter")
	double EvidenceBasedValue = 0.0;

	/** Stores the effective value consumed by runtime systems. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameter")
	double RuntimeValue = 0.0;

	/** Stores the lowest plausible value for validation and scans. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameter")
	double PlausibleMinimum = 0.0;

	/** Stores the highest plausible value for validation and scans. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameter")
	double PlausibleMaximum = 0.0;

	/** Indicates whether the runtime value includes manual calibration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameter")
	bool bHasManualAdjustment = false;

	/** Explains the method and reason for manual calibration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameter")
	FString AdjustmentReason;
};

/** Identifies and versions a complete parameter package. */
USTRUCT(BlueprintType)
struct ADAPTIVEENVRUNTIME_API FAEParameterPackageMetadata
{
	GENERATED_BODY()

	/** Uniquely identifies the parameter package lineage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Version")
	FGuid PackageId;

	/** Stores the semantic package version. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Version")
	FString SemanticVersion = TEXT("1.0.0");

	/** Identifies the package version used as the parent. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Version")
	FString ParentVersion;

	/** Identifies the researcher or tool that created the package. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Version")
	FString CreatedBy;

	/** Stores the current review or approval state. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Version")
	FString ReviewStatus = TEXT("Draft");

	/** Summarizes changes from the parent package. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Version")
	FString ChangeSummary;

	/** Stores the canonical hash of effective package content. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Version")
	FString ContentHash;

	/** Identifies the serialized package schema version. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Version")
	int32 SchemaVersion = 1;
};
