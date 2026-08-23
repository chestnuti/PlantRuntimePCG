#include "AdaptiveEnvEditorModule.h"

#include "AEPlantSuitabilityLUTAssetCustomization.h"
#include "AEWorldScalarFieldAssetCustomization.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"

void FAdaptiveEnvEditorModule::StartupModule()
{
	FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	PropertyEditor.RegisterCustomClassLayout(
		TEXT("AEWorldScalarFieldAsset"),
		FOnGetDetailCustomizationInstance::CreateStatic(&FAEWorldScalarFieldAssetCustomization::MakeInstance));
	PropertyEditor.RegisterCustomClassLayout(
		TEXT("AEPlantSuitabilityLUTAsset"),
		FOnGetDetailCustomizationInstance::CreateStatic(&FAEPlantSuitabilityLUTAssetCustomization::MakeInstance));
	PropertyEditor.NotifyCustomizationModuleChanged();
}

void FAdaptiveEnvEditorModule::ShutdownModule()
{
	if (FModuleManager::Get().IsModuleLoaded(TEXT("PropertyEditor")))
	{
		FPropertyEditorModule& PropertyEditor = FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
		PropertyEditor.UnregisterCustomClassLayout(TEXT("AEWorldScalarFieldAsset"));
		PropertyEditor.UnregisterCustomClassLayout(TEXT("AEPlantSuitabilityLUTAsset"));
		PropertyEditor.NotifyCustomizationModuleChanged();
	}
}

IMPLEMENT_MODULE(FAdaptiveEnvEditorModule, AdaptiveEnvEditor)
