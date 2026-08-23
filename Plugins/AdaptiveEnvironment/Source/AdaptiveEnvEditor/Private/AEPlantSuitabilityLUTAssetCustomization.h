#pragma once

#include "IDetailCustomization.h"

class UAEPlantSuitabilityLUTAsset;

class FAEPlantSuitabilityLUTAssetCustomization final : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
	FReply RebuildSelectedAssets();
	TArray<TWeakObjectPtr<UAEPlantSuitabilityLUTAsset>> SelectedAssets;
};
