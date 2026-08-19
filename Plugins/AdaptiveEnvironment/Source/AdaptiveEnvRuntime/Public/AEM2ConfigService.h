#pragma once

#include "CoreMinimal.h"
#include "AEActiveEnvironmentConfig.h"
#include "AEM2ConfigTypes.h"

class UAEAdaptiveEnvironmentProfile;

/* Converts and jointly validates the user-facing M2 product profile. */
class ADAPTIVEENVRUNTIME_API FAEM2ConfigService
{
public:
	static FAEM2ValidationResult ValidateProfile(const UAEAdaptiveEnvironmentProfile& Profile);
	static FAEM2ValidationResult BuildActiveConfig(const UAEAdaptiveEnvironmentProfile& Profile, uint32 NextRuntimeRevision, FAEActiveEnvironmentConfig& OutConfig);
};
