#pragma once

#include "CoreMinimal.h"
#include "AEM7Types.generated.h"

class UAEVegetationPatchComponent;

/* Stores one validated M7 species-response parameter snapshot. */
struct ADAPTIVEENVRUNTIME_API FAEM7SpeciesParameters
{
	/* Ignores M5 Damage at or below this normalized tolerance. */
	double DamageToleranceRatio = 0.15;
	/* Shapes the normalized Damage-to-health response above tolerance. */
	double DamageResponseExponent = 1.0;
	/* Retains this normalized instance density at zero health. */
	double MinimumDensityRatio = 0.10;
	/* Limits health decline per simulation hour. */
	double DeclineRatePerSimulationHour = 0.25;
	/* Limits health recovery per simulation hour. */
	double RecoveryRatePerSimulationHour = 0.10;
	/* Suppresses state revisions below this normalized health difference. */
	double HealthDirtyEpsilon = 1.0 / 255.0;
};

/* Maps one shared Grid Cell to its normalized contribution within a Patch. */
struct ADAPTIVEENVRUNTIME_API FAEWeightedPatchCell
{
	/* Stores the shared row-major Cell index. */
	int32 CellIndex = INDEX_NONE;
	/* Stores a finite non-negative aggregation weight. */
	double Weight = 0.0;
};

/* Freezes one Patch registration independently from UObject iteration order. */
struct ADAPTIVEENVRUNTIME_API FAEVegetationPatchRegistration
{
	/* Uniquely identifies the Patch across registrations and replay. */
	FGuid PatchId;
	/* Identifies the species-response profile lineage. */
	FGuid SpeciesId;
	/* Identifies the registered component generation used to reject stale commands. */
	uint32 RegistrationGeneration = 0;
	/* Stores normalized row-major Cell weights in stable order. */
	TArray<FAEWeightedPatchCell> WeightedCells;
	/* Stores the validated species-response parameters. */
	FAEM7SpeciesParameters Parameters;
	/* References the non-owning Game Thread visual consumer. */
	TWeakObjectPtr<UAEVegetationPatchComponent> Component;
};

/* Stores the smallest deterministic M7 state required across fixed steps. */
struct ADAPTIVEENVRUNTIME_API FAEVegetationPatchState
{
	/* Stores the M5-derived target health in the zero-to-one range. */
	double TargetHealthRatio = 1.0;
	/* Stores current health after bounded decline or recovery. */
	double HealthRatio = 1.0;
	/* Identifies the latest M5 response revision consumed by this Patch. */
	uint64 SourceResponseRevision = 0;
	/* Identifies the latest committed M7 Patch-state revision. */
	uint64 PatchStateRevision = 0;
	/* Identifies the latest fixed step applied to this Patch. */
	uint64 SimulationStep = 0;
};

/* Exposes one immutable M7 Patch state without redundant Stress storage. */
USTRUCT(BlueprintType)
struct ADAPTIVEENVRUNTIME_API FAEVegetationPatchSnapshot
{
	GENERATED_BODY()

	/* Identifies the queried Patch. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M7")
	FGuid PatchId;
	/* Identifies the species-response profile lineage. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M7")
	FGuid SpeciesId;
	/* Reports the current M5-derived target health. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M7")
	float TargetHealthRatio = 1.0f;
	/* Reports the current integrated Patch health. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M7")
	float HealthRatio = 1.0f;
	/* Reports density derived from current health and the species minimum. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M7")
	float DensityRatio = 1.0f;
	/* Identifies the latest M5 response revision consumed by this Patch. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M7")
	int64 SourceResponseRevision = 0;
	/* Identifies the latest committed M7 state revision. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M7")
	int64 PatchStateRevision = 0;
	/* Identifies the latest fixed M7 simulation step. */
	UPROPERTY(BlueprintReadOnly, Category = "Adaptive Environment|M7")
	int64 SimulationStep = 0;
};

/* Carries one immutable Patch result to a budgeted ISM or HISM consumer. */
struct ADAPTIVEENVRUNTIME_API FAEVegetationPatchVisualCommand
{
	/* Identifies the target Patch. */
	FGuid PatchId;
	/* Stores current health for material custom data. */
	float HealthRatio = 1.0f;
	/* Stores deterministic density derived from current health. */
	float DensityRatio = 1.0f;
	/* Identifies the M7 state represented by this command. */
	uint64 PatchStateRevision = 0;
	/* Identifies the fixed step represented by this command. */
	uint64 SimulationStep = 0;
	/* Rejects commands created for an earlier component registration. */
	uint32 RegistrationGeneration = 0;
	/* References the non-owning target component. */
	TWeakObjectPtr<UAEVegetationPatchComponent> Target;
};
