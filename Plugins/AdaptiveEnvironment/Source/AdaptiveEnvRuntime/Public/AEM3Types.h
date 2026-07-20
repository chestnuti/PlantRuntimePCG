#pragma once

#include "CoreMinimal.h"
#include "AEM3Types.generated.h"

/* Identifies one fixed M3 behavior channel index. */
enum class EAEExposureChannel : uint8
{
	Pass,
	Travel,
	Dwell,
	Sprint,
	Collect,
	Combat,
	Count
};

/* Stores one channel normalization reference and contribution weight. */
struct ADAPTIVEENVRUNTIME_API FAEExposureChannelParameters
{
	/* Normalizes the channel fact in count, seconds, or metres according to its fixed index. */
	double ReferenceValue = 1.0;
	/* Weights the normalized channel contribution in total Exposure. */
	double Weight = 0.0;
};

/* Stores accumulated Exposure dynamics shared by all behavior channels. */
struct ADAPTIVEENVRUNTIME_API FAEExposureDynamicsParameters
{
	/* Caps accumulated Cell Exposure as a non-negative ratio. */
	double Maximum = 1.0;
	/* Defines the Exposure half-life in simulated hours. */
	double HalfLifeSimulationHours = 1.0;
};

/* Stores the piecewise-linear ecological Damage response. */
struct ADAPTIVEENVRUNTIME_API FAEDamageResponseParameters
{
	/* Starts Damage when Exposure reaches this ratio. */
	double ActivationExposure = 0.5;
	/* Saturates Damage when Exposure reaches this ratio. */
	double SaturationExposure = 1.0;
	/* Caps Damage growth per simulated hour. */
	double MaximumRatePerSimulationHour = 0.0;
};

/* Stores the delayed low-Exposure ecological Recovery response. */
struct ADAPTIVEENVRUNTIME_API FAERecoveryResponseParameters
{
	/* Starts the low-Exposure recovery timer below this ratio. */
	double ActivationExposure = 0.25;
	/* Defines the continuous low-Exposure delay before Recovery in simulated hours. */
	double DelaySimulationHours = 0.0;
	/* Defines the constant Recovery rate per simulated hour. */
	double BaseRatePerSimulationHour = 0.0;
};

/* Stores validated effective M3 values grouped by model responsibility. */
struct ADAPTIVEENVRUNTIME_API FAEM3ParameterSet
{
	/* Stores six channel contracts at stable EAEExposureChannel indices. */
	TStaticArray<FAEExposureChannelParameters, static_cast<int32>(EAEExposureChannel::Count)> Channels;
	/* Stores accumulated Exposure decay and cap parameters. */
	FAEExposureDynamicsParameters ExposureDynamics;
	/* Stores the Damage response parameters. */
	FAEDamageResponseParameters DamageResponse;
	/* Stores the Recovery response parameters. */
	FAERecoveryResponseParameters RecoveryResponse;

	/* Returns mutable parameters for one fixed channel. */
	FAEExposureChannelParameters& Channel(EAEExposureChannel Channel) { return Channels[static_cast<int32>(Channel)]; }
	/* Returns immutable parameters for one fixed channel. */
	const FAEExposureChannelParameters& Channel(EAEExposureChannel Channel) const { return Channels[static_cast<int32>(Channel)]; }
};

/* Stores cumulative raw M1 facts already consumed by one M3 Cell. */
struct ADAPTIVEENVRUNTIME_API FAERawBehaviourTotals
{
	/* Stores cumulative Cell entries. */
	double PassCount = 0.0;
	/* Stores cumulative three-dimensional travel distance in metres. */
	double TravelDistanceMeters = 0.0;
	/* Stores cumulative low-speed dwelling duration in seconds. */
	double DwellSeconds = 0.0;
	/* Stores cumulative sprint distance in metres and remains a subset of Travel Distance. */
	double SprintDistanceMeters = 0.0;
	/* Stores cumulative collection-event intensity as a weighted count. */
	double CollectEventCount = 0.0;
	/* Stores cumulative combat-event intensity as a weighted count. */
	double CombatEventCount = 0.0;
};

/* Exposes immutable M3 Exposure and ecological response values for one Cell. */
USTRUCT(BlueprintType)
struct ADAPTIVEENVRUNTIME_API FAEM3CellSnapshot
{
	GENERATED_BODY()

	/* Stores the integer XY Cell coordinate. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M3")
	FIntPoint Coordinate = FIntPoint::ZeroValue;

	/* Stores the Cell centre in world centimetres. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M3")
	FVector WorldCenter = FVector::ZeroVector;

	/* Stores the latest normalized Pass contribution from zero to one. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M3")
	float PassExposure = 0.0f;

	/* Stores the latest normalized non-sprint Travel contribution from zero to one. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M3")
	float TravelExposure = 0.0f;

	/* Stores the latest normalized Dwell contribution from zero to one. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M3")
	float DwellExposure = 0.0f;

	/* Stores the latest normalized Sprint contribution from zero to one. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M3")
	float SprintExposure = 0.0f;

	/* Stores the latest normalized weighted collection-event contribution from zero to one. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M3")
	float CollectExposure = 0.0f;

	/* Stores the latest normalized weighted combat-event contribution from zero to one. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M3")
	float CombatExposure = 0.0f;

	/* Stores accumulated and decayed total Exposure. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M3")
	float CurrentExposure = 0.0f;

	/* Stores continuous ecological Damage from zero to one. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M3")
	float EcologicalDamageRatio = 0.0f;

	/* Stores the current Damage rate per simulated hour. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M3")
	float DamageRatePerSimulationHour = 0.0f;

	/* Stores the current Recovery rate per simulated hour. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M3")
	float RecoveryRatePerSimulationHour = 0.0f;

	/* Stores continuous duration below the Recovery threshold in simulated hours. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M3")
	float LowExposureDurationSimulationHours = 0.0f;

	/* Identifies the raw behavior revision consumed by this Cell. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M3")
	int64 SourceBehaviourRevision = 0;

	/* Identifies the latest Exposure state change affecting this Cell. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M3")
	int64 ExposureRevision = 0;

	/* Identifies the latest ecological response change affecting this Cell. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M3")
	int64 ResponseRevision = 0;
};
