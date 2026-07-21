#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AEParameterBundleTypes.h"
#include "AEPublishedParameterBundleAsset.generated.h"

class UAssetImportData;

/* Stores one immutable editor-imported Published Parameter Bundle v1. */
UCLASS(BlueprintType)
class ADAPTIVEENVRUNTIME_API UAEPublishedParameterBundleAsset final : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/* Creates editor-only source metadata while keeping runtime bundle fields plain. */
	UAEPublishedParameterBundleAsset();

	/* Identifies the serialized artifact type and must equal AdaptiveEnv.ParameterBundle. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Adaptive Environment|Bundle")
	FString Format = TEXT("AdaptiveEnv.ParameterBundle");

	/* Identifies the published bundle schema and must equal two. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Adaptive Environment|Bundle")
	int32 SchemaVersion = 2;

	/* Uniquely identifies the bundle lineage. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Adaptive Environment|Bundle")
	FGuid BundleId;

	/* Stores the bundle semantic version. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Adaptive Environment|Bundle")
	FString SemanticVersion;

	/* Links to the previous canonical bundle hash or remains empty for the first version. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Adaptive Environment|Bundle")
	FString ParentContentHash;

	/* Links this compact runtime bundle to its complete M2 audit artifact. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Adaptive Environment|Bundle")
	FString SourceAuditHash;

	/* Identifies the offline publisher implementation version. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Adaptive Environment|Bundle")
	FString GeneratorVersion;

	/* Stores exactly the canonical M3, M4, and M5 parameter blocks. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Adaptive Environment|Bundle")
	TArray<FAEParameterBlock> Blocks;

	/* Stores the lowercase SHA-256 digest of the canonical bundle without this field. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Adaptive Environment|Bundle")
	FString ContentHash;

#if WITH_EDITORONLY_DATA
	/* Stores the source .aeparams.json path for deterministic reimport. */
	UPROPERTY(VisibleAnywhere, Instanced, Category = "Import", meta = (ShowOnlyInnerProperties))
	TObjectPtr<UAssetImportData> AssetImportData;
#endif
};
