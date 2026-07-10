#include "AdaptiveEnvSettings.h"

UAdaptiveEnvSettings::UAdaptiveEnvSettings()
{
	CategoryName = TEXT("Plugins");
	SectionName = TEXT("AdaptiveEnvironment");
}

FName UAdaptiveEnvSettings::GetCategoryName() const
{
	return TEXT("Plugins");
}
