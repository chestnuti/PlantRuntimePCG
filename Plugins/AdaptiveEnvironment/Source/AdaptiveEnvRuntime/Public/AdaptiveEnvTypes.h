#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameplayTagContainer.h"
#include "AdaptiveEnvTypes.generated.h"

/* Describes the ecological usage state assigned to a region. */
UENUM(BlueprintType)
enum class EAERegionState : uint8
{
	Unused,    // No meaningful recent activity.
	Active,    // Activity remains within the supported range.
	Overused,  // Activity exceeds the ecological threshold.
	Recovering // Activity has stopped while the region restores.
};

/* Reports whether a behaviour sample was accepted or rejected. */
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

/* Selects the M1 behaviour or committed M3-M5 metric drawn by the debug renderer. */
UENUM(BlueprintType)
enum class EAEHeatmapDebugMode : uint8
{
	None,
	PassCount,
	TravelDistance,
	DwellTime,
	SmoothedActivity,
	Flow,
	PassExposure,
	TravelExposure,
	DwellExposure,
	SprintExposure,
	CollectExposure,
	CombatExposure,
	CurrentExposure,
	ConstraintPressure,
	HabitatSuitability,
	EnvironmentState,
	EffectiveImpact,
	Damage,
	Recovery,
	DamageRate,
	RecoveryRate
};

/* Captures one timestamped agent observation or discrete event. */
USTRUCT(BlueprintType)
struct ADAPTIVEENVRUNTIME_API FAEBehaviourSample
{
	GENERATED_BODY()

	/* Uniquely identifies the sampled agent. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour")
	FGuid AgentId;

	/* Stores the previous sampled world position in centimetres. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour")
	FVector PreviousWorldLocation = FVector::ZeroVector;

	/* Stores the current sampled world position in centimetres. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour")
	FVector WorldLocation = FVector::ZeroVector;

	/* Stores the current world velocity in centimetres per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour")
	FVector Velocity = FVector::ZeroVector;

	/* Stores the simulation timestamp in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour")
	double Timestamp = 0.0;

	/* Stores elapsed time since the previous sample in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour", meta = (ClampMin = "0.0"))
	float DeltaSeconds = 0.0f;

	/* Stores three-dimensional travel distance since the previous sample in metres. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour", meta = (ClampMin = "0.0"))
	float TravelDistanceMeters = 0.0f;

	/* Scales the contribution of a discrete behaviour event. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour", meta = (ClampMin = "0.0"))
	float EventIntensity = 1.0f;

	/* Orders samples produced by one agent. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour")
	int64 SequenceNumber = 0;

	/* Indicates whether the previous position is valid. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour")
	bool bHasPreviousLocation = false;

	/* Classifies the captured movement or event. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour")
	FGameplayTag BehaviourTag;
};

/* Defines the size, placement, and smoothing of a behaviour grid. */
USTRUCT(BlueprintType)
struct ADAPTIVEENVRUNTIME_API FAEHeatmapGridConfig
{
	GENERATED_BODY()

	/* Defines the grid centre in world XY centimetres. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
	FVector2D WorldCenter = FVector2D::ZeroVector;

	/* Defines the grid width and height in cells. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
	FIntPoint Dimensions = FIntPoint(128, 128);

	/* Defines one square cell edge in centimetres. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid", meta = (ClampMin = "1.0"))
	float CellSizeCm = 100.0f;

	/* Defines the Gaussian smoothing radius in cells. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid", meta = (ClampMin = "0"))
	int32 KernelRadiusCells = 1;

	/* Defines the Gaussian standard deviation in cells. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid", meta = (ClampMin = "0.01"))
	float KernelSigma = 0.75f;
};

/* Exposes an immutable snapshot of one behaviour grid cell. */
USTRUCT(BlueprintType)
struct ADAPTIVEENVRUNTIME_API FAEBehaviourCellSnapshot
{
	GENERATED_BODY()

	/* Stores the integer XY cell coordinate. */
	UPROPERTY(BlueprintReadOnly, Category = "Grid")
	FIntPoint Coordinate = FIntPoint::ZeroValue;

	/* Stores the cell centre in world centimetres. */
	UPROPERTY(BlueprintReadOnly, Category = "Grid")
	FVector WorldCenter = FVector::ZeroVector;

	/* Counts agent entries into this cell. */
	UPROPERTY(BlueprintReadOnly, Category = "Behaviour")
	float PassCount = 0.0f;

	/* Accumulates movement distance assigned to this cell in metres. */
	UPROPERTY(BlueprintReadOnly, Category = "Behaviour")
	float TravelDistanceMeters = 0.0f;

	/* Accumulates low-speed dwelling time in seconds. */
	UPROPERTY(BlueprintReadOnly, Category = "Behaviour")
	float DwellSeconds = 0.0f;

	/* Accumulates sprint movement distance in metres. */
	UPROPERTY(BlueprintReadOnly, Category = "Behaviour")
	float SprintDistanceMeters = 0.0f;

	/* Accumulates weighted resource collection events. */
	UPROPERTY(BlueprintReadOnly, Category = "Behaviour")
	float CollectEventCount = 0.0f;

	/* Accumulates weighted combat events. */
	UPROPERTY(BlueprintReadOnly, Category = "Behaviour")
	float CombatEventCount = 0.0f;

	/* Stores the normalized dominant horizontal travel direction. */
	UPROPERTY(BlueprintReadOnly, Category = "Behaviour")
	FVector2D FlowDirection = FVector2D::ZeroVector;

	/* Stores directional consistency from zero to one. */
	UPROPERTY(BlueprintReadOnly, Category = "Behaviour")
	float FlowMagnitude = 0.0f;

	/* Stores spatially smoothed raw activity around this cell. */
	UPROPERTY(BlueprintReadOnly, Category = "Behaviour")
	float SmoothedActivity = 0.0f;

	/* Stores the latest contributing simulation timestamp in seconds. */
	UPROPERTY(BlueprintReadOnly, Category = "Behaviour")
	double LastUpdatedTime = 0.0;
};

/* Tracks accepted and rejected behaviour sample totals. */
USTRUCT(BlueprintType)
struct ADAPTIVEENVRUNTIME_API FAEBehaviourGridStats
{
	GENERATED_BODY()

	/* Counts samples accepted by the grid. */
	UPROPERTY(BlueprintReadOnly, Category = "Statistics")
	int64 AcceptedSampleCount = 0;

	/* Counts all samples rejected by the grid or queue. */
	UPROPERTY(BlueprintReadOnly, Category = "Statistics")
	int64 RejectedSampleCount = 0;

	/* Counts samples rejected for duplicate sequence numbers. */
	UPROPERTY(BlueprintReadOnly, Category = "Statistics")
	int64 DuplicateSampleCount = 0;

	/* Counts samples rejected for sequence or timestamp order. */
	UPROPERTY(BlueprintReadOnly, Category = "Statistics")
	int64 OutOfOrderSampleCount = 0;

	/* Counts valid samples that fall outside the grid. */
	UPROPERTY(BlueprintReadOnly, Category = "Statistics")
	int64 OutOfBoundsSampleCount = 0;
};

/* Describes one citable literature source and its study context. */
