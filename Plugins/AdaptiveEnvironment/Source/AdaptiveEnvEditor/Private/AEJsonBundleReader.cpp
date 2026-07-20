#include "AEJsonBundleReader.h"

#include "AEPublishedParameterBundleAsset.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace AEJsonBundleReaderPrivate
{
	/* Requires one object to contain exactly the supplied case-sensitive keys. */
	bool HasExactFields(const TSharedPtr<FJsonObject>& Object, const TArray<FString>& Expected, const TCHAR* Context, FString& OutError)
	{
		if (!Object.IsValid() || Object->Values.Num() != Expected.Num())
		{
			OutError = FString::Printf(TEXT("%s must contain exactly %d fields."), Context, Expected.Num());
			return false;
		}
		for (const FString& Field : Expected)
		{
			if (!Object->HasField(Field))
			{
				OutError = FString::Printf(TEXT("%s is missing exact field %s."), Context, *Field);
				return false;
			}
		}
		return true;
	}

	/* Parses one required lowercase hyphenated GUID string. */
	bool ReadGuid(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, FGuid& OutGuid, FString& OutError)
	{
		FString Text;
		if (!Object->TryGetStringField(Field, Text) || Text != Text.ToLower() || !FGuid::ParseExact(Text, EGuidFormats::DigitsWithHyphens, OutGuid) || !OutGuid.IsValid())
		{
			OutError = FString::Printf(TEXT("%s must be a valid lowercase hyphenated GUID."), Field);
			return false;
		}
		return true;
	}

	/* Parses one strict published parameter record. */
	bool ReadParameter(const TSharedPtr<FJsonObject>& Object, FAEPublishedParameter& OutParameter, FString& OutError)
	{
		static const TArray<FString> Fields = { TEXT("ParameterId"), TEXT("Name"), TEXT("Unit"), TEXT("EvidenceBasedValue"), TEXT("EffectiveValue"), TEXT("PlausibleMinimum"), TEXT("PlausibleMaximum"), TEXT("ParameterVersion"), TEXT("OriginLayer") };
		if (!HasExactFields(Object, Fields, TEXT("Parameter"), OutError) || !ReadGuid(Object, TEXT("ParameterId"), OutParameter.ParameterId, OutError)) return false;
		FString Name;
		FString Origin;
		if (!Object->TryGetStringField(TEXT("Name"), Name) || Name.IsEmpty() || !Object->TryGetStringField(TEXT("Unit"), OutParameter.Unit) ||
			!Object->TryGetNumberField(TEXT("EvidenceBasedValue"), OutParameter.EvidenceBasedValue) || !Object->TryGetNumberField(TEXT("EffectiveValue"), OutParameter.EffectiveValue) ||
			!Object->TryGetNumberField(TEXT("PlausibleMinimum"), OutParameter.PlausibleMinimum) || !Object->TryGetNumberField(TEXT("PlausibleMaximum"), OutParameter.PlausibleMaximum) ||
			!Object->TryGetStringField(TEXT("ParameterVersion"), OutParameter.ParameterVersion) || !Object->TryGetStringField(TEXT("OriginLayer"), Origin))
		{
			OutError = TEXT("Parameter contains a missing field or incorrect JSON type.");
			return false;
		}
		OutParameter.Name = FName(Name);
		if (Origin == TEXT("Synthesized")) OutParameter.OriginLayer = EAEParameterOriginLayer::Synthesized;
		else if (Origin == TEXT("ResearcherCalibrated")) OutParameter.OriginLayer = EAEParameterOriginLayer::ResearcherCalibrated;
		else if (Origin == TEXT("ExperimentOverride")) OutParameter.OriginLayer = EAEParameterOriginLayer::ExperimentOverride;
		else
		{
			OutError = FString::Printf(TEXT("Unknown OriginLayer %s."), *Origin);
			return false;
		}
		return true;
	}

	/* Parses one strict model-contract block. */
	bool ReadBlock(const TSharedPtr<FJsonObject>& Object, FAEParameterBlock& OutBlock, FString& OutError)
	{
		static const TArray<FString> Fields = { TEXT("BlockId"), TEXT("ModelContract"), TEXT("BlockVersion"), TEXT("Parameters"), TEXT("BlockHash") };
		if (!HasExactFields(Object, Fields, TEXT("Block"), OutError) || !ReadGuid(Object, TEXT("BlockId"), OutBlock.BlockId, OutError)) return false;
		FString Contract;
		if (!Object->TryGetStringField(TEXT("ModelContract"), Contract) || !Object->TryGetStringField(TEXT("BlockVersion"), OutBlock.BlockVersion) || !Object->TryGetStringField(TEXT("BlockHash"), OutBlock.BlockHash))
		{
			OutError = TEXT("Block contains a missing field or incorrect JSON type.");
			return false;
		}
		OutBlock.ModelContract = FName(Contract);
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Object->TryGetArrayField(TEXT("Parameters"), Values))
		{
			OutError = TEXT("Parameters must be an array.");
			return false;
		}
		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			FAEPublishedParameter& Parameter = OutBlock.Parameters.AddDefaulted_GetRef();
			if (!ReadParameter(Value->AsObject(), Parameter, OutError)) return false;
		}
		return true;
	}
}

/* Reads UTF-8 JSON, rejects unknown or missing fields, and populates one candidate. */
bool FAEJsonBundleReader::ReadFile(const FString& Filename, UAEPublishedParameterBundleAsset& OutBundle, FString& OutError)
{
	OutError.Reset();
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *Filename))
	{
		OutError = FString::Printf(TEXT("Failed to read UTF-8 bundle file %s."), *Filename);
		return false;
	}
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutError = TEXT("Bundle file is not valid JSON.");
		return false;
	}
	static const TArray<FString> Fields = { TEXT("Format"), TEXT("SchemaVersion"), TEXT("BundleId"), TEXT("SemanticVersion"), TEXT("ParentContentHash"), TEXT("SourceAuditHash"), TEXT("GeneratorVersion"), TEXT("Blocks"), TEXT("ContentHash") };
	if (!AEJsonBundleReaderPrivate::HasExactFields(Root, Fields, TEXT("Bundle"), OutError) || !AEJsonBundleReaderPrivate::ReadGuid(Root, TEXT("BundleId"), OutBundle.BundleId, OutError)) return false;
	double SchemaVersion = 0.0;
	if (!Root->TryGetStringField(TEXT("Format"), OutBundle.Format) || !Root->TryGetNumberField(TEXT("SchemaVersion"), SchemaVersion) || FMath::FloorToDouble(SchemaVersion) != SchemaVersion ||
		!Root->TryGetStringField(TEXT("SemanticVersion"), OutBundle.SemanticVersion) || !Root->TryGetStringField(TEXT("ParentContentHash"), OutBundle.ParentContentHash) ||
		!Root->TryGetStringField(TEXT("SourceAuditHash"), OutBundle.SourceAuditHash) || !Root->TryGetStringField(TEXT("GeneratorVersion"), OutBundle.GeneratorVersion) || !Root->TryGetStringField(TEXT("ContentHash"), OutBundle.ContentHash))
	{
		OutError = TEXT("Bundle header contains a missing field or incorrect JSON type.");
		return false;
	}
	OutBundle.SchemaVersion = static_cast<int32>(SchemaVersion);
	const TArray<TSharedPtr<FJsonValue>>* Blocks = nullptr;
	if (!Root->TryGetArrayField(TEXT("Blocks"), Blocks))
	{
		OutError = TEXT("Blocks must be an array.");
		return false;
	}
	for (const TSharedPtr<FJsonValue>& Value : *Blocks)
	{
		FAEParameterBlock& Block = OutBundle.Blocks.AddDefaulted_GetRef();
		if (!AEJsonBundleReaderPrivate::ReadBlock(Value->AsObject(), Block, OutError)) return false;
	}
	return true;
}
