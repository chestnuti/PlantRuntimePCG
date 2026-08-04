#pragma once

#include "CoreMinimal.h"
#include "AEM3Types.h"
#include "AEM4Types.h"
#include "AEM5Types.h"

/* Stores one atomically committed product configuration for M3 through M5. */
struct ADAPTIVEENVRUNTIME_API FAEActiveEnvironmentConfig
{
	FName ProfileId = NAME_None;
	int32 ConfigVersion = 1;
	uint32 RuntimeRevision = 0;
	FAEM3ParameterSet M3;
	FAEM4ParameterSet M4;
	FAEM5ParameterSet M5;

	bool IsValid() const { return !ProfileId.IsNone() && ConfigVersion == 1 && RuntimeRevision > 0; }
};
