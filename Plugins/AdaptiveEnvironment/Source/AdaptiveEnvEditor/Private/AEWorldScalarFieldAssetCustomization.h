#pragma once

#include "IDetailCustomization.h"

class UAEWorldScalarFieldAsset;

class FAEWorldScalarFieldAssetCustomization final : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
	FReply RebuildSelectedAssets();
	TArray<TWeakObjectPtr<UAEWorldScalarFieldAsset>> SelectedAssets;
};
