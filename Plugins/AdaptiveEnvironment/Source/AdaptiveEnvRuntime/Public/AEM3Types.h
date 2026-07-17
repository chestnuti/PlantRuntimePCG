#pragma once

#include "CoreMinimal.h"
#include "AEM3Types.generated.h"

/* Stores validated effective M3 values and their immutable parameter-package identity. */
struct ADAPTIVEENVRUNTIME_API FAEM3ParameterSet
{
	/* Identifies the source parameter-package lineage. */
	FGuid PackageId;
	/* Stores the source parameter-package semantic version. */
	FString SemanticVersion;
	/* Stores the canonical source parameter-package SHA-256 hash. */
	FString ContentHash;
	/* Stores effective parameter versions by stable parameter name. */
	TMap<FName, FString> ParameterVersions;

	/* Normalizes Cell entry count into Pass Exposure. */
	double ExposurePassReferenceCount = 1.0;
	/* Normalizes non-sprint three-dimensional travel distance in metres. */
	double ExposureTravelDistanceReferenceMeters = 1.0;
	/* Normalizes low-speed dwelling duration in seconds. */
	double ExposureDwellReferenceSeconds = 1.0;
	/* Normalizes sprint three-dimensional travel distance in metres. */
	double ExposureSprintDistanceReferenceMeters = 1.0;
	/* Normalizes weighted collection-event count. */
	double ExposureCollectEventReferenceCount = 1.0;
	/* Normalizes weighted combat-event count. */
	double ExposureCombatEventReferenceCount = 1.0;

	/* Weights normalized Pass Exposure in the total increment. */
	double ExposurePassWeight = 0.0;
	/* Weights normalized non-sprint Travel Exposure in the total increment. */
	double ExposureTravelDistanceWeight = 0.0;
	/* Weights normalized Dwell Exposure in the total increment. */
	double ExposureDwellWeight = 0.0;
	/* Weights normalized Sprint Exposure in the total increment. */
	double ExposureSprintWeight = 0.0;
	/* Weights normalized collection-event Exposure in the total increment. */
	double ExposureCollectEventWeight = 0.0;
	/* Weights normalized combat-event Exposure in the total increment. */
	double ExposureCombatEventWeight = 0.0;

	/* Caps accumulated Cell Exposure as a non-negative ratio. */
	double ExposureMaximum = 1.0;
	/* Defines the Exposure half-life in simulated hours. */
	double ExposureHalfLifeSimulationHours = 1.0;
	/* Starts Damage when Exposure reaches this ratio. */
	double DamageActivationExposure = 0.5;
	/* Saturates Damage when Exposure reaches this ratio. */
	double DamageSaturationExposure = 1.0;
	/* Caps Damage growth per simulated hour. */
	double DamageMaximumRatePerSimulationHour = 0.0;
	/* Starts the low-Exposure recovery timer below this ratio. */
	double RecoveryActivationExposure = 0.25;
	/* Defines the continuous low-Exposure delay before Recovery in simulated hours. */
	double RecoveryDelaySimulationHours = 0.0;
	/* Defines the first-version constant Recovery rate per simulated hour. */
	double RecoveryBaseRatePerSimulationHour = 0.0;
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

