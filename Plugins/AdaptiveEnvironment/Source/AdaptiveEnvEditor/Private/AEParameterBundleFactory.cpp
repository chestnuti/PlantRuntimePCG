#include "AEParameterBundleFactory.h"

#include "AEJsonBundleReader.h"
#include "AEParameterBundleService.h"
#include "AEPublishedParameterBundleAsset.h"
#include "EditorFramework/AssetImportData.h"

namespace AEParameterBundleFactoryPrivate
{
	/* Copies only published contract fields after a candidate has passed every gate. */
	void CopyBundle(const UAEPublishedParameterBundleAsset& Source, UAEPublishedParameterBundleAsset& Destination)
	{
		Destination.Format = Source.Format;
		Destination.SchemaVersion = Source.SchemaVersion;
		Destination.BundleId = Source.BundleId;
		Destination.SemanticVersion = Source.SemanticVersion;
		Destination.ParentContentHash = Source.ParentContentHash;
		Destination.SourceAuditHash = Source.SourceAuditHash;
		Destination.GeneratorVersion = Source.GeneratorVersion;
		Destination.Blocks = Source.Blocks;
		Destination.ContentHash = Source.ContentHash;
	}
}

/* Configures the factory for editor-only .aeparams.json imports. */
UAEParameterBundleFactory::UAEParameterBundleFactory()
{
	SupportedClass = UAEPublishedParameterBundleAsset::StaticClass();
	bCreateNew = false;
	bEditorImport = true;
	Formats.Add(TEXT("json;Adaptive Environment Published Parameter Bundle"));
}

/* Returns whether the filename uses the exact published bundle suffix. */
bool UAEParameterBundleFactory::FactoryCanImport(const FString& Filename)
{
	return Filename.EndsWith(TEXT(".aeparams.json"), ESearchCase::CaseSensitive);
}

/* Parses, validates, and creates one bundle asset without partial mutation. */
UObject* UAEParameterBundleFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
	bOutOperationCanceled = false;
	UAEPublishedParameterBundleAsset* Candidate = NewObject<UAEPublishedParameterBundleAsset>(GetTransientPackage());
	FString Error;
	if (!FAEJsonBundleReader::ReadFile(Filename, *Candidate, Error))
	{
		Warn->Logf(ELogVerbosity::Error, TEXT("AE-BUNDLE-100: %s"), *Error);
		return nullptr;
	}
	const FAEParameterBundleValidationResult Validation = FAEParameterBundleService::ValidateBundle(*Candidate);
	if (!Validation.IsValid())
	{
		Warn->Logf(ELogVerbosity::Error, TEXT("AE-BUNDLE-101: %s"), *Validation.ToString());
		return nullptr;
	}

	// Copy only a fully validated transient candidate into the persistent asset.
	UAEPublishedParameterBundleAsset* Asset = NewObject<UAEPublishedParameterBundleAsset>(InParent, InClass, InName, Flags);
	AEParameterBundleFactoryPrivate::CopyBundle(*Candidate, *Asset);
#if WITH_EDITORONLY_DATA
	Asset->AssetImportData->Update(Filename);
#endif
	return Asset;
}

/* Returns the single source filename used to create one bundle asset. */
bool UAEParameterBundleFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	const UAEPublishedParameterBundleAsset* Asset = Cast<UAEPublishedParameterBundleAsset>(Obj);
#if WITH_EDITORONLY_DATA
	if (Asset != nullptr && Asset->AssetImportData != nullptr)
	{
		Asset->AssetImportData->ExtractFilenames(OutFilenames);
		return OutFilenames.Num() == 1;
	}
#endif
	return false;
}

/* Updates the source filename associated with one bundle asset. */
void UAEParameterBundleFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
#if WITH_EDITORONLY_DATA
	if (UAEPublishedParameterBundleAsset* Asset = Cast<UAEPublishedParameterBundleAsset>(Obj); Asset != nullptr && Asset->AssetImportData != nullptr && NewReimportPaths.Num() == 1)
	{
		Asset->AssetImportData->UpdateFilenameOnly(NewReimportPaths[0]);
	}
#endif
}

/* Reimports through a temporary validated candidate before overwriting the asset. */
EReimportResult::Type UAEParameterBundleFactory::Reimport(UObject* Obj)
{
	UAEPublishedParameterBundleAsset* Asset = Cast<UAEPublishedParameterBundleAsset>(Obj);
#if WITH_EDITORONLY_DATA
	if (Asset == nullptr || Asset->AssetImportData == nullptr)
	{
		return EReimportResult::Failed;
	}
	const FString Filename = Asset->AssetImportData->GetFirstFilename();
	UAEPublishedParameterBundleAsset* Candidate = NewObject<UAEPublishedParameterBundleAsset>(GetTransientPackage());
	FString Error;
	if (!FAEJsonBundleReader::ReadFile(Filename, *Candidate, Error) || !FAEParameterBundleService::ValidateBundle(*Candidate).IsValid())
	{
		return EReimportResult::Failed;
	}

	// Mutate the existing asset only after the complete candidate has passed validation.
	Asset->Modify();
	AEParameterBundleFactoryPrivate::CopyBundle(*Candidate, *Asset);
	Asset->AssetImportData->Update(Filename);
	Asset->MarkPackageDirty();
	return EReimportResult::Succeeded;
#else
	return EReimportResult::Failed;
#endif
}
