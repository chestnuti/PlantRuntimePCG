#include "AEPublishedParameterBundleAsset.h"

#if WITH_EDITOR
#include "EditorFramework/AssetImportData.h"
#endif

/* Creates editor-only source metadata while keeping runtime bundle fields plain. */
UAEPublishedParameterBundleAsset::UAEPublishedParameterBundleAsset()
{
#if WITH_EDITORONLY_DATA
	AssetImportData = CreateDefaultSubobject<UAssetImportData>(TEXT("AssetImportData"));
#endif
}
