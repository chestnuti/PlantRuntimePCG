#include "AdaptiveEnvSettings.h"

// Place adaptive environment options under the Plugins settings category.
UAdaptiveEnvSettings::UAdaptiveEnvSettings()
{
	CategoryName = TEXT("Plugins");
	SectionName = TEXT("AdaptiveEnvironment");
}

// Return the stable category used by the Project Settings UI.
FName UAdaptiveEnvSettings::GetCategoryName() const
{
	return TEXT("Plugins");
}
