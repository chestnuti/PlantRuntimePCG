#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/EngineTypes.h"
#include "AEPlantSpeciesProfile.generated.h"

class UAEPlantBiomeMapAsset;
class UStaticMesh;

UCLASS(BlueprintType)
class ADAPTIVEENVRUNTIME_API UAEPlantSpeciesProfile final : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/* Stores the stable species key used by identity hashing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity") FName SpeciesId = TEXT("Plant");
	/* Stores the immutable research profile identity. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity") FGuid ProfileId;
	/* Stores the semantic profile version. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity") FString SemanticVersion = TEXT("1.3.0");
	/* Supplies the mesh owned by the runtime HISM. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual") TSoftObjectPtr<UStaticMesh> StaticMesh;
	/* Offsets each instance anchor along the projected ground normal in centimetres. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement", meta = (ClampMin = "-10000.0", ClampMax = "10000.0"))
	float GroundOffsetCm = 0.0f;
	/* Aligns the instance local up axis to the projected ground normal before stable yaw. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	bool bAlignToGroundNormal = false;
	/* Selects whether this species participates in collision queries or physics. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual|Collision")
	TEnumAsByte<ECollisionEnabled::Type> CollisionEnabled = ECollisionEnabled::NoCollision;
	/* Selects the collision response profile applied before the runtime HISM registers. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual|Collision")
	FName CollisionProfileName = TEXT("NoCollision");
	/* Enables overlap events only when collision is also enabled. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual|Collision")
	bool bGenerateOverlapEvents = false;
	/* Controls whether this species contributes geometry to navigation generation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual|Collision")
	bool bCanEverAffectNavigation = false;
	/* Supplies the optional species-specific biome prior. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Distribution") TObjectPtr<UAEPlantBiomeMapAsset> BiomeMap;
	/* Defines the global Poisson exclusion distance in centimetres. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Distribution", meta = (ClampMin = "1.0")) float MinimumSpacingCm = 150.0f;
	/* Defines maximum pool density per square metre. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Distribution", meta = (ClampMin = "0.000001")) float MaximumInstancesPerSquareMeter = 0.02f;
	/* Caps rejection attempts relative to expected candidates. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Distribution", meta = (ClampMin = "1")) int32 PoissonAttemptsPerExpectedPoint = 20;
	/* Defines health loss per simulation hour. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lifecycle", meta = (ClampMin = "0.0")) float DeclineRatePerSimulationHour = 0.5f;
	/* Defines health recovery per simulation hour. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lifecycle", meta = (ClampMin = "0.0")) float RecoveryRatePerSimulationHour = 0.25f;
	/* Defines the health threshold for terminal state. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lifecycle", meta = (ClampMin = "0.0", ClampMax = "1.0")) float DeadHealthThreshold = 0.1f;
	/* Defines the health hysteresis width above the dead threshold for reappearance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lifecycle", meta = (ClampMin = "0.0", ClampMax = "1.0")) float StateEpsilon = 0.1f;
	/* Defines deterministic per-candidate health variation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lifecycle", meta = (ClampMin = "0.0", ClampMax = "0.49")) float HealthVariationAmplitude = 0.05f;

	/* Validates all required structural and lifecycle values. */
	bool IsValidProfile(FString& OutError) const;
};
