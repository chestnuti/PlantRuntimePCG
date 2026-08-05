#pragma once

#include "CoreMinimal.h"
#include "AEM3Types.h"
#include "AEM4Types.h"
#include "AEM5Types.h"

class UAEMoistureTextureAsset;

/* Stores one atomically committed product configuration for M3 through M5. */
struct ADAPTIVEENVRUNTIME_API FAEActiveEnvironmentConfig
{
	FName ProfileId = NAME_None;
	int32 ConfigVersion = 2;
	uint32 RuntimeRevision = 0;
	double DefaultMoistureRatio = 0.5;
	TWeakObjectPtr<UAEMoistureTextureAsset> MoistureTexture;
	FAEM3ParameterSet M3;
	FAEM4ParameterSet M4;
	FAEM5ParameterSet M5;

	bool IsValid() const { return !ProfileId.IsNone() && ConfigVersion == 2 && RuntimeRevision > 0; }
};
