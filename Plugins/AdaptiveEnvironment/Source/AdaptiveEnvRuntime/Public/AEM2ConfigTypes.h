#pragma once

#include "CoreMinimal.h"
#include "AEM2ConfigTypes.generated.h"

class UAEMoistureTextureAsset;

/* Configures one named M3 behavior channel for product tuning. */
USTRUCT(BlueprintType)
struct ADAPTIVEENVRUNTIME_API FAEExposureChannelConfig
{
	GENERATED_BODY()

	FAEExposureChannelConfig() = default;
	FAEExposureChannelConfig(const double InReferenceValue, const double InWeight)
		: ReferenceValue(InReferenceValue), Weight(InWeight)
	{
	}

	/* Defines the positive normalization reference in the channel's documented unit. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Exposure", meta = (ClampMin = "0.000001"))
	double ReferenceValue = 1.0;

	/* Defines this channel's non-negative share of total Exposure. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Exposure", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double Weight = 0.0;
};

/* Provides the complete user-facing M3 configuration. */
USTRUCT(BlueprintType)
struct ADAPTIVEENVRUNTIME_API FAEM3UserConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Channels") FAEExposureChannelConfig Pass { 2.0, 0.20 };
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Channels") FAEExposureChannelConfig Travel { 5.0, 0.20 };
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Channels") FAEExposureChannelConfig Dwell { 5.0, 0.15 };
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Channels") FAEExposureChannelConfig Sprint { 2.0, 0.15 };
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Channels") FAEExposureChannelConfig Collect { 2.0, 0.15 };
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Channels") FAEExposureChannelConfig Combat { 2.0, 0.15 };

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dynamics", meta = (ClampMin = "0.000001"))
	double MaximumExposure = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dynamics", meta = (ClampMin = "0.000001"))
	double HalfLifeSimulationHours = 2.0;
};

/* Provides the complete user-facing M4 configuration. */
USTRUCT(BlueprintType)
struct ADAPTIVEENVRUNTIME_API FAEM4UserConfig
{
	GENERATED_BODY()

	/* Supplies normalized moisture when the texture and local volumes provide no value. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Moisture Input", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double DefaultMoistureRatio = 0.5;

	/* Supplies the dedicated baked R-channel moisture texture below local moisture volumes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Moisture Input")
	TObjectPtr<UAEMoistureTextureAsset> MoistureTexture;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain", meta = (ClampMin = "0.0", ClampMax = "90.0", Units = "deg")) double SlopeFullySuitableDegrees = 10.0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain", meta = (ClampMin = "0.0", ClampMax = "90.0", Units = "deg")) double SlopeUnsuitableDegrees = 45.0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Moisture", meta = (ClampMin = "0.0", ClampMax = "1.0")) double MoistureOptimalMinimumRatio = 0.30;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Moisture", meta = (ClampMin = "0.0", ClampMax = "1.0")) double MoistureOptimalMaximumRatio = 0.70;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Moisture", meta = (ClampMin = "0.000001", ClampMax = "1.0")) double MoistureToleranceWidthRatio = 0.20;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Region State", meta = (ClampMin = "0.0", ClampMax = "1.0")) double ActiveThreshold = 0.25;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Region State", meta = (ClampMin = "0.0", ClampMax = "1.0")) double OverusedThreshold = 0.75;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Region State", meta = (ClampMin = "0.0", ClampMax = "1.0")) double HysteresisWidth = 0.10;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Region State", meta = (ClampMin = "0.0")) double TransitionDebounceSimulationHours = 0.5;
};

/* Provides the complete user-facing M5 configuration. */
USTRUCT(BlueprintType)
struct ADAPTIVEENVRUNTIME_API FAEM5UserConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact Fusion", meta = (ClampMin = "0.0")) double ConstraintSensitivity = 0.5;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "0.0", ClampMax = "1.0")) double DamageActivationImpact = 0.25;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "0.0", ClampMax = "1.0")) double DamageSaturationImpact = 0.75;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "0.0")) double DamageMaximumRatePerSimulationHour = 0.20;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recovery", meta = (ClampMin = "0.0", ClampMax = "1.0")) double RecoveryActivationExposure = 0.20;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recovery", meta = (ClampMin = "0.0")) double RecoveryDelaySimulationHours = 1.0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recovery", meta = (ClampMin = "0.0")) double RecoveryBaseRatePerSimulationHour = 0.10;
};

/* Collects all blocking findings from one M2 profile validation pass. */
struct ADAPTIVEENVRUNTIME_API FAEM2ValidationResult
{
	TArray<FString> Issues;

	bool IsValid() const { return Issues.IsEmpty(); }
	void Add(const TCHAR* Code, const FString& Message);
	FString ToString() const;
};
