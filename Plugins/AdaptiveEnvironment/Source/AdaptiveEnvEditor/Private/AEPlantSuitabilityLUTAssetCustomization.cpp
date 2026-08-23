#include "AEPlantSuitabilityLUTAssetCustomization.h"

#include "AEPlantSuitabilityLUTAsset.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Misc/MessageDialog.h"
#include "ScopedTransaction.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "AEPlantSuitabilityLUTAssetCustomization"

TSharedRef<IDetailCustomization> FAEPlantSuitabilityLUTAssetCustomization::MakeInstance()
{
	return MakeShared<FAEPlantSuitabilityLUTAssetCustomization>();
}

void FAEPlantSuitabilityLUTAssetCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);
	for (const TWeakObjectPtr<UObject>& Object : Objects)
	{
		if (UAEPlantSuitabilityLUTAsset* Asset = Cast<UAEPlantSuitabilityLUTAsset>(Object.Get()))
		{
			SelectedAssets.Add(Asset);
		}
	}

	IDetailCategoryBuilder& SourceCategory = DetailBuilder.EditCategory(TEXT("Source"));
	SourceCategory.AddCustomRow(LOCTEXT("RebuildSearchText", "Rebuild From Source Texture"))
	.WholeRowContent()
	[
		SNew(SButton)
		.Text(LOCTEXT("RebuildButton", "Rebuild From Source Texture"))
		.ToolTipText(LOCTEXT("RebuildTooltip", "Bake the source texture R channel into the runtime suitability samples."))
		.HAlign(HAlign_Center)
		.OnClicked(this, &FAEPlantSuitabilityLUTAssetCustomization::RebuildSelectedAssets)
	];
}

FReply FAEPlantSuitabilityLUTAssetCustomization::RebuildSelectedAssets()
{
	const FScopedTransaction Transaction(LOCTEXT("RebuildTransaction", "Rebuild Plant Suitability LUT"));
	int32 SuccessCount = 0;
	for (const TWeakObjectPtr<UAEPlantSuitabilityLUTAsset>& WeakAsset : SelectedAssets)
	{
		if (UAEPlantSuitabilityLUTAsset* Asset = WeakAsset.Get())
		{
			SuccessCount += Asset->RebuildFromSourceTexture() ? 1 : 0;
		}
	}

	if (SuccessCount == SelectedAssets.Num() && SuccessCount > 0)
	{
		FNotificationInfo Info(FText::Format(
			LOCTEXT("RebuildSucceeded", "Rebuilt {0} plant suitability LUT asset(s). Save the asset before PIE."),
			FText::AsNumber(SuccessCount)));
		Info.ExpireDuration = 4.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
	}
	else
	{
		FMessageDialog::Open(
			EAppMsgType::Ok,
			LOCTEXT("RebuildFailed", "Plant suitability LUT rebuild failed. Assign a non-sRGB Texture2D whose source format is G8 or BGRA8."));
	}
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
