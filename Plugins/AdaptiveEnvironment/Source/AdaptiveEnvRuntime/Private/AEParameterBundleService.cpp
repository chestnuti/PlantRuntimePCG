#include "AEParameterBundleService.h"

#include "AEPublishedParameterBundleAsset.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonWriter.h"

namespace AEParameterBundlePrivate
{
	/* Rotates one 32-bit SHA-256 working word to the right. */
	uint32 RotateRight(const uint32 Value, const uint32 Bits)
	{
		return (Value >> Bits) | (Value << (32U - Bits));
	}

	/* Computes a portable lowercase SHA-256 digest for deterministic bundle sealing. */
	FString ComputeSHA256(const uint8* Data, const int32 DataSize)
	{
		static constexpr uint32 RoundConstants[64] =
		{
			0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
			0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
			0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
			0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
			0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
			0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
			0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
			0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U
		};
		uint32 State[8] = { 0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU, 0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U };

		// Pad UTF-8 bytes with the SHA-256 marker and big-endian bit length.
		const int32 PaddedSize = ((DataSize + 9 + 63) / 64) * 64;
		TArray<uint8> Message;
		Message.SetNumZeroed(PaddedSize);
		if (DataSize > 0) FMemory::Memcpy(Message.GetData(), Data, DataSize);
		Message[DataSize] = 0x80U;
		const uint64 BitLength = static_cast<uint64>(DataSize) * 8ULL;
		for (int32 Index = 0; Index < 8; ++Index) Message[PaddedSize - 1 - Index] = static_cast<uint8>(BitLength >> (Index * 8));

		// Process every 512-bit block through the standard 64 SHA-256 rounds.
		for (int32 BlockOffset = 0; BlockOffset < PaddedSize; BlockOffset += 64)
		{
			uint32 Words[64] = {};
			for (int32 Index = 0; Index < 16; ++Index)
			{
				const int32 Offset = BlockOffset + Index * 4;
				Words[Index] = (static_cast<uint32>(Message[Offset]) << 24) | (static_cast<uint32>(Message[Offset + 1]) << 16) |
					(static_cast<uint32>(Message[Offset + 2]) << 8) | static_cast<uint32>(Message[Offset + 3]);
			}
			for (int32 Index = 16; Index < 64; ++Index)
			{
				const uint32 Sigma0 = RotateRight(Words[Index - 15], 7) ^ RotateRight(Words[Index - 15], 18) ^ (Words[Index - 15] >> 3);
				const uint32 Sigma1 = RotateRight(Words[Index - 2], 17) ^ RotateRight(Words[Index - 2], 19) ^ (Words[Index - 2] >> 10);
				Words[Index] = Words[Index - 16] + Sigma0 + Words[Index - 7] + Sigma1;
			}
			uint32 A = State[0], B = State[1], C = State[2], D = State[3], E = State[4], F = State[5], G = State[6], H = State[7];
			for (int32 Index = 0; Index < 64; ++Index)
			{
				const uint32 Temporary1 = H + (RotateRight(E, 6) ^ RotateRight(E, 11) ^ RotateRight(E, 25)) + ((E & F) ^ ((~E) & G)) + RoundConstants[Index] + Words[Index];
				const uint32 Temporary2 = (RotateRight(A, 2) ^ RotateRight(A, 13) ^ RotateRight(A, 22)) + ((A & B) ^ (A & C) ^ (B & C));
				H = G; G = F; F = E; E = D + Temporary1; D = C; C = B; B = A; A = Temporary1 + Temporary2;
			}
			State[0] += A; State[1] += B; State[2] += C; State[3] += D; State[4] += E; State[5] += F; State[6] += G; State[7] += H;
		}
		/* Serializes the final digest words as the lowercase bundle hash. */
		return FString::Printf(TEXT("%08x%08x%08x%08x%08x%08x%08x%08x"), State[0], State[1], State[2], State[3], State[4], State[5], State[6], State[7]);
	}

	/* Defines one exact name and unit entry in a frozen model contract. */
	struct FContractEntry
	{
		const TCHAR* Name;
		const TCHAR* Unit;
	};

	static const FContractEntry M3Contract[] =
	{
		{ TEXT("DamageActivationExposure"), TEXT("ratio") },
		{ TEXT("DamageMaximumRatePerSimulationHour"), TEXT("ratio/h") },
		{ TEXT("DamageSaturationExposure"), TEXT("ratio") },
		{ TEXT("ExposureCollectEventReferenceCount"), TEXT("count") },
		{ TEXT("ExposureCollectEventWeight"), TEXT("ratio") },
		{ TEXT("ExposureCombatEventReferenceCount"), TEXT("count") },
		{ TEXT("ExposureCombatEventWeight"), TEXT("ratio") },
		{ TEXT("ExposureDwellReferenceSeconds"), TEXT("s") },
		{ TEXT("ExposureDwellWeight"), TEXT("ratio") },
		{ TEXT("ExposureHalfLifeSimulationHours"), TEXT("h") },
		{ TEXT("ExposureMaximum"), TEXT("ratio") },
		{ TEXT("ExposurePassReferenceCount"), TEXT("count") },
		{ TEXT("ExposurePassWeight"), TEXT("ratio") },
		{ TEXT("ExposureSprintDistanceReferenceMeters"), TEXT("m") },
		{ TEXT("ExposureSprintWeight"), TEXT("ratio") },
		{ TEXT("ExposureTravelDistanceReferenceMeters"), TEXT("m") },
		{ TEXT("ExposureTravelDistanceWeight"), TEXT("ratio") },
		{ TEXT("RecoveryActivationExposure"), TEXT("ratio") },
		{ TEXT("RecoveryBaseRatePerSimulationHour"), TEXT("ratio/h") },
		{ TEXT("RecoveryDelaySimulationHours"), TEXT("h") }
	};

	static const FContractEntry M4Contract[] =
	{
		{ TEXT("ActiveThreshold"), TEXT("ratio") },
		{ TEXT("ConstraintStressSensitivity"), TEXT("ratio") },
		{ TEXT("HysteresisWidth"), TEXT("ratio") },
		{ TEXT("MoistureOptimalMaximumRatio"), TEXT("ratio") },
		{ TEXT("MoistureOptimalMinimumRatio"), TEXT("ratio") },
		{ TEXT("MoistureToleranceWidthRatio"), TEXT("ratio") },
		{ TEXT("OverusedThreshold"), TEXT("ratio") },
		{ TEXT("SlopeFullySuitableDegrees"), TEXT("degree") },
		{ TEXT("SlopeUnsuitableDegrees"), TEXT("degree") },
		{ TEXT("TransitionDebounceSimulationHours"), TEXT("h") }
	};

	/* Returns a stable lowercase GUID string. */
	FString StableGuid(const FGuid& Guid)
	{
		return Guid.ToString(EGuidFormats::DigitsWithHyphensLower);
	}

	/* Returns whether one string is a compact three-part semantic version. */
	bool IsSemanticVersion(const FString& Version)
	{
		TArray<FString> Parts;
		Version.ParseIntoArray(Parts, TEXT("."), false);
		if (Parts.Num() != 3)
		{
			return false;
		}
		for (const FString& Part : Parts)
		{
			if (Part.IsEmpty() || !Part.IsNumeric())
			{
				return false;
			}
		}
		return true;
	}

	/* Returns whether one string is a lowercase 64-character SHA-256 digest. */
	bool IsSHA256(const FString& Hash, const bool bAllowEmpty = false)
	{
		if (bAllowEmpty && Hash.IsEmpty())
		{
			return true;
		}
		if (Hash.Len() != 64)
		{
			return false;
		}
		for (const TCHAR Character : Hash)
		{
			if (!FChar::IsHexDigit(Character) || FChar::IsUpper(Character))
			{
				return false;
			}
		}
		return true;
	}

	/* Computes one lowercase SHA-256 digest from UTF-8 canonical JSON. */
	FString HashUtf8(const FString& Text)
	{
		FTCHARToUTF8 Utf8(*Text);
		return ComputeSHA256(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
	}

	/* Writes one published parameter in the frozen v1 field order. */
	template <typename WriterType>
	void WriteParameter(WriterType& Writer, const FAEPublishedParameter& Parameter)
	{
		Writer.WriteObjectStart();
		Writer.WriteValue(TEXT("ParameterId"), StableGuid(Parameter.ParameterId));
		Writer.WriteValue(TEXT("Name"), Parameter.Name.ToString());
		Writer.WriteValue(TEXT("Unit"), Parameter.Unit);
		Writer.WriteValue(TEXT("EvidenceBasedValue"), Parameter.EvidenceBasedValue);
		Writer.WriteValue(TEXT("EffectiveValue"), Parameter.EffectiveValue);
		Writer.WriteValue(TEXT("PlausibleMinimum"), Parameter.PlausibleMinimum);
		Writer.WriteValue(TEXT("PlausibleMaximum"), Parameter.PlausibleMaximum);
		Writer.WriteValue(TEXT("ParameterVersion"), Parameter.ParameterVersion);
		Writer.WriteValue(TEXT("OriginLayer"), StaticEnum<EAEParameterOriginLayer>()->GetNameStringByValue(static_cast<int64>(Parameter.OriginLayer)));
		Writer.WriteObjectEnd();
	}

	/* Writes one parameter block and optionally includes its stored digest. */
	template <typename WriterType>
	void WriteBlock(WriterType& Writer, const FAEParameterBlock& Block, const bool bIncludeHash)
	{
		Writer.WriteObjectStart();
		Writer.WriteValue(TEXT("BlockId"), StableGuid(Block.BlockId));
		Writer.WriteValue(TEXT("ModelContract"), Block.ModelContract.ToString());
		Writer.WriteValue(TEXT("BlockVersion"), Block.BlockVersion);
		Writer.WriteArrayStart(TEXT("Parameters"));
		for (const FAEPublishedParameter& Parameter : Block.Parameters)
		{
			WriteParameter(Writer, Parameter);
		}
		Writer.WriteArrayEnd();
		if (bIncludeHash)
		{
			Writer.WriteValue(TEXT("BlockHash"), Block.BlockHash);
		}
		Writer.WriteObjectEnd();
	}

	/* Validates one block against its exact frozen parameter contract. */
	template <uint32 ContractCount>
	void ValidateContract(const FAEParameterBlock& Block, const FContractEntry (&Contract)[ContractCount], FAEParameterBundleValidationResult& Result)
	{
		if (Block.Parameters.Num() != static_cast<int32>(ContractCount))
		{
			Result.Add(TEXT("AE-BUNDLE-021"), FString::Printf(TEXT("Block %s must contain exactly %u parameters."), *Block.ModelContract.ToString(), ContractCount));
			return;
		}

		TSet<FGuid> ParameterIds;
		for (uint32 Index = 0; Index < ContractCount; ++Index)
		{
			const FAEPublishedParameter& Parameter = Block.Parameters[Index];
			const FContractEntry& Expected = Contract[Index];
			if (!Parameter.ParameterId.IsValid() || ParameterIds.Contains(Parameter.ParameterId))
			{
				Result.Add(TEXT("AE-BUNDLE-022"), FString::Printf(TEXT("Parameter %s has an invalid or duplicate ParameterId."), *Parameter.Name.ToString()));
			}
			ParameterIds.Add(Parameter.ParameterId);
			if (Parameter.Name.ToString() != Expected.Name || Parameter.Unit != Expected.Unit)
			{
				Result.Add(TEXT("AE-BUNDLE-023"), FString::Printf(TEXT("Parameter index %u must be %s [%s]."), Index, Expected.Name, Expected.Unit));
			}
			if (!FMath::IsFinite(Parameter.EvidenceBasedValue) || !FMath::IsFinite(Parameter.EffectiveValue) ||
				!FMath::IsFinite(Parameter.PlausibleMinimum) || !FMath::IsFinite(Parameter.PlausibleMaximum) ||
				Parameter.PlausibleMinimum > Parameter.EffectiveValue || Parameter.EffectiveValue > Parameter.PlausibleMaximum)
			{
				Result.Add(TEXT("AE-BUNDLE-024"), FString::Printf(TEXT("Parameter %s has invalid finite values or plausible bounds."), *Parameter.Name.ToString()));
			}
			if (!IsSemanticVersion(Parameter.ParameterVersion))
			{
				Result.Add(TEXT("AE-BUNDLE-025"), FString::Printf(TEXT("Parameter %s has an invalid semantic version."), *Parameter.Name.ToString()));
			}
			if (Parameter.OriginLayer != EAEParameterOriginLayer::Synthesized &&
				Parameter.OriginLayer != EAEParameterOriginLayer::ResearcherCalibrated &&
				Parameter.OriginLayer != EAEParameterOriginLayer::ExperimentOverride)
			{
				Result.Add(TEXT("AE-BUNDLE-026"), FString::Printf(TEXT("Parameter %s has an invalid OriginLayer."), *Parameter.Name.ToString()));
			}
		}
	}
}

/* Adds one stable blocking finding. */
void FAEParameterBundleValidationResult::Add(const TCHAR* Code, const FString& Message)
{
	FAEParameterBundleIssue& Issue = Issues.AddDefaulted_GetRef();
	Issue.Code = Code;
	Issue.Message = Message;
}

/* Joins all findings into one concise diagnostic message. */
FString FAEParameterBundleValidationResult::ToString() const
{
	TArray<FString> Lines;
	for (const FAEParameterBundleIssue& Issue : Issues)
	{
		Lines.Add(FString::Printf(TEXT("%s: %s"), *Issue.Code, *Issue.Message));
	}
	/* Produces a single log-ready validation summary. */
	return FString::Join(Lines, TEXT("; "));
}

/* Returns the exact M3 model-contract identifier. */
FName FAEParameterBundleService::M3ModelContract()
{
	return TEXT("AdaptiveEnv.M3.ExposureResponse.v2");
}

/* Returns the exact M4 model-contract identifier. */
FName FAEParameterBundleService::M4ModelContract()
{
	return TEXT("AdaptiveEnv.M4.ConstraintDecision.v1");
}

/* Validates header, identity, ordering, records, model contracts, and canonical hashes. */
FAEParameterBundleValidationResult FAEParameterBundleService::ValidateBundle(const UAEPublishedParameterBundleAsset& Bundle)
{
	using namespace AEParameterBundlePrivate;
	FAEParameterBundleValidationResult Result;

	// Validate the immutable bundle header before inspecting nested content.
	if (Bundle.Format != TEXT("AdaptiveEnv.ParameterBundle")) Result.Add(TEXT("AE-BUNDLE-001"), TEXT("Format must equal AdaptiveEnv.ParameterBundle."));
	if (Bundle.SchemaVersion != 1) Result.Add(TEXT("AE-BUNDLE-002"), TEXT("SchemaVersion must equal 1."));
	if (!Bundle.BundleId.IsValid()) Result.Add(TEXT("AE-BUNDLE-003"), TEXT("BundleId must be a valid GUID."));
	if (!IsSemanticVersion(Bundle.SemanticVersion)) Result.Add(TEXT("AE-BUNDLE-004"), TEXT("SemanticVersion must use major.minor.patch."));
	if (!IsSHA256(Bundle.ParentContentHash, true)) Result.Add(TEXT("AE-BUNDLE-005"), TEXT("ParentContentHash must be empty or lowercase SHA-256."));
	if (!IsSHA256(Bundle.SourceAuditHash)) Result.Add(TEXT("AE-BUNDLE-006"), TEXT("SourceAuditHash must be lowercase SHA-256."));
	if (Bundle.GeneratorVersion.IsEmpty()) Result.Add(TEXT("AE-BUNDLE-007"), TEXT("GeneratorVersion must not be empty."));
	if (!IsSHA256(Bundle.ContentHash)) Result.Add(TEXT("AE-BUNDLE-008"), TEXT("ContentHash must be lowercase SHA-256."));

	// Enforce exactly two uniquely identified blocks in canonical contract order.
	if (Bundle.Blocks.Num() != 2)
	{
		Result.Add(TEXT("AE-BUNDLE-010"), TEXT("Blocks must contain exactly the M3 and M4 contracts."));
	}
	else
	{
		if (Bundle.Blocks[0].ModelContract != M3ModelContract() || Bundle.Blocks[1].ModelContract != M4ModelContract())
		{
			Result.Add(TEXT("AE-BUNDLE-011"), TEXT("Blocks must use canonical ModelContract order M3 then M4."));
		}
		if (!Bundle.Blocks[0].BlockId.IsValid() || !Bundle.Blocks[1].BlockId.IsValid() || Bundle.Blocks[0].BlockId == Bundle.Blocks[1].BlockId)
		{
			Result.Add(TEXT("AE-BUNDLE-012"), TEXT("BlockId values must be valid and unique."));
		}
		for (const FAEParameterBlock& Block : Bundle.Blocks)
		{
			if (!IsSemanticVersion(Block.BlockVersion)) Result.Add(TEXT("AE-BUNDLE-013"), TEXT("BlockVersion must use major.minor.patch."));
			FString HashError;
			const FString ActualHash = ComputeBlockHash(Block, HashError);
			if (!HashError.IsEmpty() || Block.BlockHash != ActualHash) Result.Add(TEXT("AE-BUNDLE-014"), FString::Printf(TEXT("BlockHash mismatch for %s."), *Block.ModelContract.ToString()));
		}
		ValidateContract(Bundle.Blocks[0], M3Contract, Result);
		ValidateContract(Bundle.Blocks[1], M4Contract, Result);

		// Require ParameterId uniqueness across both model contracts, not only within each block.
		TSet<FGuid> BundleParameterIds;
		for (const FAEParameterBlock& Block : Bundle.Blocks)
		{
			for (const FAEPublishedParameter& Parameter : Block.Parameters)
			{
				if (BundleParameterIds.Contains(Parameter.ParameterId))
				{
					Result.Add(TEXT("AE-BUNDLE-027"), FString::Printf(TEXT("ParameterId is duplicated across blocks for %s."), *Parameter.Name.ToString()));
				}
				BundleParameterIds.Add(Parameter.ParameterId);
			}
		}
	}

	// Seal the complete candidate only after all nested fields are available.
	FString HashError;
	const FString ActualContentHash = ComputeContentHash(Bundle, HashError);
	if (!HashError.IsEmpty() || Bundle.ContentHash != ActualContentHash)
	{
		Result.Add(TEXT("AE-BUNDLE-030"), TEXT("ContentHash does not match the canonical bundle payload."));
	}
	return Result;
}

/* Finds one validated immutable block by exact model contract. */
bool FAEParameterBundleService::FindBlock(const UAEPublishedParameterBundleAsset& Bundle, const FName ModelContract, FAEParameterBlockView& OutBlock)
{
	OutBlock.Block = nullptr;
	for (const FAEParameterBlock& Block : Bundle.Blocks)
	{
		if (Block.ModelContract == ModelContract)
		{
			OutBlock.Block = &Block;
			return true;
		}
	}
	return false;
}

/* Builds the canonical block JSON used by Python and Unreal SHA-256 checks. */
bool FAEParameterBundleService::BuildCanonicalBlockJson(const FAEParameterBlock& Block, FString& OutJson, FString& OutError)
{
	OutJson.Reset();
	OutError.Reset();
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutJson);
	AEParameterBundlePrivate::WriteBlock(*Writer, Block, false);
	if (!Writer->Close())
	{
		OutError = TEXT("Failed to finalize canonical block JSON.");
		OutJson.Reset();
		return false;
	}
	return true;
}

/* Builds the canonical bundle JSON and optionally omits ContentHash for digest calculation. */
bool FAEParameterBundleService::BuildCanonicalBundleJson(const UAEPublishedParameterBundleAsset& Bundle, const bool bIncludeContentHash, FString& OutJson, FString& OutError)
{
	OutJson.Reset();
	OutError.Reset();
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutJson);
	Writer->WriteObjectStart();
	Writer->WriteValue(TEXT("Format"), Bundle.Format);
	Writer->WriteValue(TEXT("SchemaVersion"), Bundle.SchemaVersion);
	Writer->WriteValue(TEXT("BundleId"), AEParameterBundlePrivate::StableGuid(Bundle.BundleId));
	Writer->WriteValue(TEXT("SemanticVersion"), Bundle.SemanticVersion);
	Writer->WriteValue(TEXT("ParentContentHash"), Bundle.ParentContentHash);
	Writer->WriteValue(TEXT("SourceAuditHash"), Bundle.SourceAuditHash);
	Writer->WriteValue(TEXT("GeneratorVersion"), Bundle.GeneratorVersion);
	Writer->WriteArrayStart(TEXT("Blocks"));
	for (const FAEParameterBlock& Block : Bundle.Blocks)
	{
		AEParameterBundlePrivate::WriteBlock(*Writer, Block, true);
	}
	Writer->WriteArrayEnd();
	if (bIncludeContentHash)
	{
		Writer->WriteValue(TEXT("ContentHash"), Bundle.ContentHash);
	}
	Writer->WriteObjectEnd();
	if (!Writer->Close())
	{
		OutError = TEXT("Failed to finalize canonical bundle JSON.");
		OutJson.Reset();
		return false;
	}
	return true;
}

/* Computes the lowercase SHA-256 digest of one canonical block. */
FString FAEParameterBundleService::ComputeBlockHash(const FAEParameterBlock& Block, FString& OutError)
{
	FString Json;
	if (!BuildCanonicalBlockJson(Block, Json, OutError))
	{
		return FString();
	}
	/* Hashes only the canonical representation used by the interchange contract. */
	return AEParameterBundlePrivate::HashUtf8(Json);
}

/* Computes the lowercase SHA-256 digest of one canonical bundle. */
FString FAEParameterBundleService::ComputeContentHash(const UAEPublishedParameterBundleAsset& Bundle, FString& OutError)
{
	FString Json;
	if (!BuildCanonicalBundleJson(Bundle, false, Json, OutError))
	{
		return FString();
	}
	/* Hashes the bundle with ContentHash excluded from its own canonical payload. */
	return AEParameterBundlePrivate::HashUtf8(Json);
}
