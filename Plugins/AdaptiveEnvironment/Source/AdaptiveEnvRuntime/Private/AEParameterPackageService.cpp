#include "AEParameterPackageService.h"

#include "Algo/Sort.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonWriter.h"

namespace AEParameterPackagePrivate
{
	constexpr double NumericTolerance = 1.0e-9;

	/* Rotates one 32-bit SHA-256 working word to the right. */
	uint32 RotateRight(const uint32 Value, const uint32 Bits)
	{
		return (Value >> Bits) | (Value << (32U - Bits));
	}

	/* Computes a portable lowercase SHA-256 digest for deterministic package sealing. */
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

		uint32 State[8] =
		{
			0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
			0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U
		};

		// Pad the UTF-8 input with one marker byte and a big-endian 64-bit bit length.
		const int32 PaddedSize = ((DataSize + 9 + 63) / 64) * 64;
		TArray<uint8> Message;
		Message.SetNumZeroed(PaddedSize);
		if (DataSize > 0)
		{
			FMemory::Memcpy(Message.GetData(), Data, DataSize);
		}
		Message[DataSize] = 0x80U;
		const uint64 BitLength = static_cast<uint64>(DataSize) * 8ULL;
		for (int32 Index = 0; Index < 8; ++Index)
		{
			Message[PaddedSize - 1 - Index] = static_cast<uint8>(BitLength >> (Index * 8));
		}

		// Process every 512-bit block through the standard 64 SHA-256 rounds.
		for (int32 BlockOffset = 0; BlockOffset < PaddedSize; BlockOffset += 64)
		{
			uint32 Words[64] = {};
			for (int32 Index = 0; Index < 16; ++Index)
			{
				const int32 Offset = BlockOffset + Index * 4;
				Words[Index] =
					(static_cast<uint32>(Message[Offset]) << 24) |
					(static_cast<uint32>(Message[Offset + 1]) << 16) |
					(static_cast<uint32>(Message[Offset + 2]) << 8) |
					static_cast<uint32>(Message[Offset + 3]);
			}
			for (int32 Index = 16; Index < 64; ++Index)
			{
				const uint32 Sigma0 = RotateRight(Words[Index - 15], 7) ^ RotateRight(Words[Index - 15], 18) ^ (Words[Index - 15] >> 3);
				const uint32 Sigma1 = RotateRight(Words[Index - 2], 17) ^ RotateRight(Words[Index - 2], 19) ^ (Words[Index - 2] >> 10);
				Words[Index] = Words[Index - 16] + Sigma0 + Words[Index - 7] + Sigma1;
			}

			uint32 A = State[0];
			uint32 B = State[1];
			uint32 C = State[2];
			uint32 D = State[3];
			uint32 E = State[4];
			uint32 F = State[5];
			uint32 G = State[6];
			uint32 H = State[7];

			for (int32 Index = 0; Index < 64; ++Index)
			{
				const uint32 UpperSigma1 = RotateRight(E, 6) ^ RotateRight(E, 11) ^ RotateRight(E, 25);
				const uint32 Choice = (E & F) ^ ((~E) & G);
				const uint32 Temporary1 = H + UpperSigma1 + Choice + RoundConstants[Index] + Words[Index];
				const uint32 UpperSigma0 = RotateRight(A, 2) ^ RotateRight(A, 13) ^ RotateRight(A, 22);
				const uint32 Majority = (A & B) ^ (A & C) ^ (B & C);
				const uint32 Temporary2 = UpperSigma0 + Majority;

				H = G;
				G = F;
				F = E;
				E = D + Temporary1;
				D = C;
				C = B;
				B = A;
				A = Temporary1 + Temporary2;
			}

			State[0] += A;
			State[1] += B;
			State[2] += C;
			State[3] += D;
			State[4] += E;
			State[5] += F;
			State[6] += G;
			State[7] += H;
		}

		// Emit exactly eight zero-padded lowercase 32-bit words.
		return FString::Printf(
			TEXT("%08x%08x%08x%08x%08x%08x%08x%08x"),
			State[0], State[1], State[2], State[3], State[4], State[5], State[6], State[7]);
	}

	/* Returns one stable lowercase GUID string for sorting and JSON output. */
	FString StableGuid(const FGuid& Guid)
	{
		return Guid.ToString(EGuidFormats::DigitsWithHyphensLower);
	}

	/* Returns true when a compact semantic version contains three non-negative integer parts. */
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

	/* Returns true when a non-empty digest contains exactly 64 hexadecimal characters. */
	bool IsSHA256(const FString& Digest)
	{
		if (Digest.Len() != 64)
		{
			return false;
		}

		for (const TCHAR Character : Digest)
		{
			if (!FChar::IsHexDigit(Character))
			{
				return false;
			}
		}

		return true;
	}

	/* Returns true when one compact score is finite and inside the inclusive zero-to-one range. */
	bool IsUnitScore(const float Score)
	{
		/* Rejects non-finite rubric scores before checking the closed interval. */
		return FMath::IsFinite(Score) && Score >= 0.0f && Score <= 1.0f;
	}

	/* Writes one canonical contribution object using a stable field order. */
	template <typename WriterType>
	void WriteContribution(WriterType& Writer, const FAESynthesisContribution& Contribution)
	{
		Writer.WriteObjectStart();
		Writer.WriteValue(TEXT("evidenceId"), StableGuid(Contribution.EvidenceId));
		Writer.WriteValue(TEXT("transformationId"), StableGuid(Contribution.TransformationId));
		Writer.WriteValue(TEXT("normalizedValue"), Contribution.NormalizedValue);
		Writer.WriteValue(TEXT("normalizedUnit"), Contribution.NormalizedUnit);
		Writer.WriteValue(TEXT("contextRelevance"), Contribution.ContextRelevance);
		Writer.WriteValue(TEXT("studyQuality"), Contribution.StudyQuality);
		Writer.WriteValue(TEXT("researcherConfidence"), Contribution.ResearcherConfidence);
		Writer.WriteValue(TEXT("assignedWeight"), Contribution.AssignedWeight);
		Writer.WriteValue(TEXT("included"), Contribution.bIncluded);
		Writer.WriteValue(TEXT("inclusionReason"), Contribution.InclusionReason);
		Writer.WriteValue(TEXT("exclusionReason"), Contribution.ExclusionReason);
		Writer.WriteValue(TEXT("scoringNotes"), Contribution.ScoringNotes);
		Writer.WriteObjectEnd();
	}

	/* Builds deterministic package JSON and optionally includes audit metadata and the stored digest. */
	bool BuildPackageJson(
		const UAEParameterSynthesisAsset& Package,
		const bool bIncludeAuditMetadata,
		FString& OutJson,
		FString& OutError)
	{
		OutJson.Reset();
		OutError.Reset();

		// Copy and sort serialized arrays so editor ordering cannot change the package hash.
		TArray<const FAEParameterSynthesis*> SortedSyntheses;
		for (const FAEParameterSynthesis& Synthesis : Package.Syntheses)
		{
			SortedSyntheses.Add(&Synthesis);
		}
		SortedSyntheses.Sort([](const FAEParameterSynthesis& Left, const FAEParameterSynthesis& Right)
		{
			return Left.TargetParameter.ToString() < Right.TargetParameter.ToString();
		});

		TArray<const FAEDerivedParameter*> SortedParameters;
		for (const FAEDerivedParameter& Parameter : Package.DerivedParameters)
		{
			SortedParameters.Add(&Parameter);
		}
		SortedParameters.Sort([](const FAEDerivedParameter& Left, const FAEDerivedParameter& Right)
		{
			return Left.ParameterName.ToString() < Right.ParameterName.ToString();
		});

		// Stream fields in a fixed order instead of serializing unordered maps.
		TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutJson);
		Writer->WriteObjectStart();
		Writer->WriteObjectStart(TEXT("metadata"));
		Writer->WriteValue(TEXT("packageId"), StableGuid(Package.Metadata.PackageId));
		Writer->WriteValue(TEXT("semanticVersion"), Package.Metadata.SemanticVersion);
		Writer->WriteValue(TEXT("parentVersion"), Package.Metadata.ParentVersion);
		Writer->WriteValue(TEXT("schemaVersion"), Package.Metadata.SchemaVersion);
		if (bIncludeAuditMetadata)
		{
			Writer->WriteValue(TEXT("createdAtUtc"), Package.Metadata.CreatedAt.ToIso8601());
			Writer->WriteValue(TEXT("createdBy"), Package.Metadata.CreatedBy);
			Writer->WriteValue(TEXT("reviewStatus"), Package.Metadata.ReviewStatus);
			Writer->WriteValue(TEXT("changeSummary"), Package.Metadata.ChangeSummary);
			Writer->WriteValue(TEXT("contentHash"), Package.Metadata.ContentHash);
		}
		Writer->WriteObjectEnd();

		Writer->WriteArrayStart(TEXT("syntheses"));
		for (const FAEParameterSynthesis* Synthesis : SortedSyntheses)
		{
			Writer->WriteObjectStart();
			Writer->WriteValue(TEXT("synthesisId"), StableGuid(Synthesis->SynthesisId));
			Writer->WriteValue(TEXT("synthesisVersion"), Synthesis->SynthesisVersion);
			Writer->WriteValue(TEXT("targetParameter"), Synthesis->TargetParameter.ToString());
			Writer->WriteValue(TEXT("outputUnit"), Synthesis->OutputUnit);
			Writer->WriteValue(TEXT("method"), StaticEnum<EAESynthesisMethod>()->GetNameStringByValue(static_cast<int64>(Synthesis->Method)));
			Writer->WriteValue(TEXT("formula"), Synthesis->Formula);
			Writer->WriteValue(TEXT("weightingRationale"), Synthesis->WeightingRationale);
			Writer->WriteValue(TEXT("computedValue"), Synthesis->ComputedValue);
			Writer->WriteValue(TEXT("computedMinimum"), Synthesis->ComputedMinimum);
			Writer->WriteValue(TEXT("computedMaximum"), Synthesis->ComputedMaximum);
			Writer->WriteValue(TEXT("reviewStatus"), Synthesis->ReviewStatus);

			TArray<FAESynthesisContribution> SortedContributions = Synthesis->Contributions;
			SortedContributions.Sort([](const FAESynthesisContribution& Left, const FAESynthesisContribution& Right)
			{
				const FString LeftKey = StableGuid(Left.EvidenceId) + StableGuid(Left.TransformationId);
				const FString RightKey = StableGuid(Right.EvidenceId) + StableGuid(Right.TransformationId);
				return LeftKey < RightKey;
			});

			Writer->WriteArrayStart(TEXT("contributions"));
			for (const FAESynthesisContribution& Contribution : SortedContributions)
			{
				WriteContribution(*Writer, Contribution);
			}
			Writer->WriteArrayEnd();
			Writer->WriteObjectEnd();
		}
		Writer->WriteArrayEnd();

		Writer->WriteArrayStart(TEXT("derivedParameters"));
		for (const FAEDerivedParameter* Parameter : SortedParameters)
		{
			Writer->WriteObjectStart();
			Writer->WriteValue(TEXT("parameterId"), StableGuid(Parameter->ParameterId));
			Writer->WriteValue(TEXT("parameterName"), Parameter->ParameterName.ToString());
			Writer->WriteValue(TEXT("description"), Parameter->Description);
			Writer->WriteValue(TEXT("unit"), Parameter->Unit);
			Writer->WriteValue(TEXT("synthesisId"), StableGuid(Parameter->SynthesisId));
			Writer->WriteValue(TEXT("parameterVersion"), Parameter->ParameterVersion);
			Writer->WriteValue(TEXT("evidenceBasedValue"), Parameter->EvidenceBasedValue);
			Writer->WriteValue(TEXT("runtimeValue"), Parameter->RuntimeValue);
			Writer->WriteValue(TEXT("plausibleMinimum"), Parameter->PlausibleMinimum);
			Writer->WriteValue(TEXT("plausibleMaximum"), Parameter->PlausibleMaximum);
			Writer->WriteValue(TEXT("hasManualAdjustment"), Parameter->bHasManualAdjustment);
			Writer->WriteValue(TEXT("adjustmentMethod"), Parameter->AdjustmentMethod);
			Writer->WriteValue(TEXT("adjustmentReason"), Parameter->AdjustmentReason);
			Writer->WriteObjectEnd();
		}
		Writer->WriteArrayEnd();
		Writer->WriteObjectEnd();

		if (!Writer->Close())
		{
			OutError = TEXT("Failed to finalize canonical parameter-package JSON.");
			OutJson.Reset();
			return false;
		}

		return true;
	}
}

/* Returns true when no Error finding blocks use or publication. */
bool FAEValidationResult::IsValid() const
{
	return Count(EAEValidationSeverity::Error) == 0;
}

/* Counts findings with the requested severity. */
int32 FAEValidationResult::Count(const EAEValidationSeverity Severity) const
{
	int32 MatchingCount = 0;
	for (const FAEValidationIssue& Issue : Issues)
	{
		if (Issue.Severity == Severity)
		{
			++MatchingCount;
		}
	}
	return MatchingCount;
}

/* Appends one finding while preserving deterministic validation order. */
void FAEValidationResult::Add(
	const EAEValidationSeverity Severity,
	const TCHAR* Code,
	const FString& Message,
	const FGuid& RelatedId)
{
	FAEValidationIssue& Issue = Issues.AddDefaulted_GetRef();
	Issue.Severity = Severity;
	Issue.Code = Code;
	Issue.Message = Message;
	Issue.RelatedId = RelatedId;
}

/* Recomputes one normalized value and rejects incompatible units or invalid inputs. */
bool FAEParameterPackageService::ComputeTransformation(
	const FAEEvidenceRecord& Evidence,
	const FAEEvidenceTransformation& Transformation,
	double& OutValue,
	FString& OutError)
{
	OutValue = 0.0;
	OutError.Reset();

	// Validate identity, units, and finite raw values before applying a conversion.
	if (Transformation.EvidenceId != Evidence.EvidenceId)
	{
		OutError = TEXT("Transformation does not reference the supplied evidence record.");
		return false;
	}
	if (!Evidence.RawUnit.Equals(Transformation.InputUnit, ESearchCase::CaseSensitive))
	{
		OutError = TEXT("Transformation input unit does not match the evidence raw unit.");
		return false;
	}
	if (Transformation.OutputUnit.IsEmpty() || !FMath::IsFinite(Evidence.RawValue))
	{
		OutError = TEXT("Transformation requires a finite raw value and non-empty output unit.");
		return false;
	}

	// Apply only explicit deterministic methods; custom results require auditable metadata.
	switch (Transformation.Method)
	{
	case EAETransformationMethod::Identity:
		OutValue = Evidence.RawValue;
		break;
	case EAETransformationMethod::PercentToRatio:
		OutValue = Evidence.RawValue / 100.0;
		break;
	case EAETransformationMethod::CentimetersToMeters:
		OutValue = Evidence.RawValue / 100.0;
		break;
	case EAETransformationMethod::HoursToSeconds:
		OutValue = Evidence.RawValue * 3600.0;
		break;
	case EAETransformationMethod::PerHourToPerSecond:
		OutValue = Evidence.RawValue / 3600.0;
		break;
	case EAETransformationMethod::RangeMidpoint:
		if (!FMath::IsFinite(Evidence.RawMinimum) || !FMath::IsFinite(Evidence.RawMaximum) || Evidence.RawMinimum > Evidence.RawMaximum)
		{
			OutError = TEXT("Range midpoint transformation requires finite ordered bounds.");
			return false;
		}
		OutValue = (Evidence.RawMinimum + Evidence.RawMaximum) * 0.5;
		break;
	case EAETransformationMethod::CustomVersioned:
		if (Transformation.Formula.IsEmpty() || Transformation.Assumptions.IsEmpty() || !FMath::IsFinite(Transformation.NormalizedValue))
		{
			OutError = TEXT("Custom transformation requires formula, assumptions, and a finite stored result.");
			return false;
		}
		OutValue = Transformation.NormalizedValue;
		break;
	default:
		OutError = TEXT("Unsupported evidence transformation method.");
		return false;
	}

	if (!FMath::IsFinite(OutValue))
	{
		OutError = TEXT("Transformation produced a non-finite value.");
		return false;
	}

	return true;
}

/* Recomputes one scalar synthesis using stable contribution ordering. */
bool FAEParameterPackageService::ComputeSynthesis(
	const FAEParameterSynthesis& Synthesis,
	double& OutValue,
	double& OutMinimum,
	double& OutMaximum,
	FString& OutError)
{
	OutValue = 0.0;
	OutMinimum = 0.0;
	OutMaximum = 0.0;
	OutError.Reset();

	// Select included contributions and sort them by stable provenance identity.
	TArray<const FAESynthesisContribution*> Included;
	for (const FAESynthesisContribution& Contribution : Synthesis.Contributions)
	{
		if (Contribution.bIncluded)
		{
			Included.Add(&Contribution);
		}
	}
	Included.Sort([](const FAESynthesisContribution& Left, const FAESynthesisContribution& Right)
	{
		const FString LeftKey = AEParameterPackagePrivate::StableGuid(Left.EvidenceId) + AEParameterPackagePrivate::StableGuid(Left.TransformationId);
		const FString RightKey = AEParameterPackagePrivate::StableGuid(Right.EvidenceId) + AEParameterPackagePrivate::StableGuid(Right.TransformationId);
		return LeftKey < RightKey;
	});

	if (Included.IsEmpty())
	{
		OutError = TEXT("Synthesis has no included contributions.");
		return false;
	}

	// Validate canonical units, numeric inputs, and non-negative weights.
	for (const FAESynthesisContribution* Contribution : Included)
	{
		if (!FMath::IsFinite(Contribution->NormalizedValue) || !FMath::IsFinite(Contribution->AssignedWeight) || Contribution->AssignedWeight < 0.0)
		{
			OutError = TEXT("Synthesis contains a non-finite value or invalid weight.");
			return false;
		}
		if (!Contribution->NormalizedUnit.Equals(Synthesis.OutputUnit, ESearchCase::CaseSensitive))
		{
			OutError = TEXT("Synthesis contribution unit does not match the output unit.");
			return false;
		}
	}

	OutMinimum = Included[0]->NormalizedValue;
	OutMaximum = Included[0]->NormalizedValue;
	for (const FAESynthesisContribution* Contribution : Included)
	{
		OutMinimum = FMath::Min(OutMinimum, Contribution->NormalizedValue);
		OutMaximum = FMath::Max(OutMaximum, Contribution->NormalizedValue);
	}

	// Apply only the three scalar methods accepted by the M2 MVP.
	switch (Synthesis.Method)
	{
	case EAESynthesisMethod::ArithmeticMean:
		for (const FAESynthesisContribution* Contribution : Included)
		{
			OutValue += Contribution->NormalizedValue;
		}
		OutValue /= static_cast<double>(Included.Num());
		break;

	case EAESynthesisMethod::WeightedMean:
	{
		double WeightedSum = 0.0;
		double TotalWeight = 0.0;
		for (const FAESynthesisContribution* Contribution : Included)
		{
			WeightedSum += Contribution->NormalizedValue * Contribution->AssignedWeight;
			TotalWeight += Contribution->AssignedWeight;
		}
		if (TotalWeight <= AEParameterPackagePrivate::NumericTolerance)
		{
			OutError = TEXT("Weighted synthesis has zero total weight.");
			return false;
		}
		OutValue = WeightedSum / TotalWeight;
		break;
	}

	case EAESynthesisMethod::Median:
	{
		TArray<double> Values;
		for (const FAESynthesisContribution* Contribution : Included)
		{
			Values.Add(Contribution->NormalizedValue);
		}
		Values.Sort();
		const int32 Middle = Values.Num() / 2;
		OutValue = Values.Num() % 2 == 0
			? (Values[Middle - 1] + Values[Middle]) * 0.5
			: Values[Middle];
		break;
	}

	default:
		OutError = TEXT("Synthesis method is planned but not implemented by the M2 MVP.");
		return false;
	}

	/* Prevents a successful synthesis from publishing a non-finite result. */
	return FMath::IsFinite(OutValue);
}

/* Refreshes compact quality weights, synthesis values, derived values, and the package content hash. */
bool FAEParameterPackageService::RefreshPackage(UAEParameterSynthesisAsset& Package, FString& OutError)
{
	OutError.Reset();

	// Rebuild final compact weights from the three auditable score components.
	for (FAEParameterSynthesis& Synthesis : Package.Syntheses)
	{
		for (FAESynthesisContribution& Contribution : Synthesis.Contributions)
		{
			Contribution.AssignedWeight =
				static_cast<double>(Contribution.ContextRelevance) *
				static_cast<double>(Contribution.StudyQuality) *
				static_cast<double>(Contribution.ResearcherConfidence);
		}

		// Recompute stored synthesis snapshots from current contributions.
		if (!ComputeSynthesis(
			Synthesis,
			Synthesis.ComputedValue,
			Synthesis.ComputedMinimum,
			Synthesis.ComputedMaximum,
			OutError))
		{
			return false;
		}
	}

	// Refresh evidence-based values while preserving explicit manual runtime adjustments.
	for (FAEDerivedParameter& Parameter : Package.DerivedParameters)
	{
		const FAEParameterSynthesis* Synthesis = Package.Syntheses.FindByPredicate([&Parameter](const FAEParameterSynthesis& Candidate)
		{
			return Candidate.SynthesisId == Parameter.SynthesisId;
		});
		if (Synthesis == nullptr)
		{
			OutError = FString::Printf(TEXT("Parameter %s references a missing synthesis."), *Parameter.ParameterName.ToString());
			return false;
		}

		Parameter.EvidenceBasedValue = Synthesis->ComputedValue;
		if (!Parameter.bHasManualAdjustment)
		{
			Parameter.RuntimeValue = Parameter.EvidenceBasedValue;
		}
	}

	// Seal the refreshed effective package with a canonical SHA-256 digest.
	Package.Metadata.ContentHash = ComputeContentHash(Package, OutError);
	return !Package.Metadata.ContentHash.IsEmpty();
}

/* Validates source, evidence, and transformation identity and numeric contracts. */
FAEValidationResult FAEParameterPackageService::ValidateEvidenceAsset(const UAELiteratureEvidenceAsset& EvidenceAsset)
{
	FAEValidationResult Result;
	TSet<FGuid> SourceIds;
	TSet<FGuid> EvidenceIds;
	TSet<FGuid> TransformationIds;
	TMap<FGuid, const FAEEvidenceRecord*> EvidenceById;

	// Validate the serialized schema and every stable source identity.
	if (EvidenceAsset.SchemaVersion != 2)
	{
		Result.Add(EAEValidationSeverity::Error, TEXT("AE-M2-001"), TEXT("Evidence asset schema must be version 2."));
	}
	for (const FAELiteratureSource& Source : EvidenceAsset.Sources)
	{
		if (!Source.SourceId.IsValid() || SourceIds.Contains(Source.SourceId))
		{
			Result.Add(EAEValidationSeverity::Error, TEXT("AE-M2-002"), TEXT("Literature source GUID is invalid or duplicated."), Source.SourceId);
		}
		SourceIds.Add(Source.SourceId);
		if (Source.Title.IsEmpty() || Source.Authors.IsEmpty() || (Source.DOI.IsEmpty() && Source.DatasetDOI.IsEmpty()))
		{
			Result.Add(EAEValidationSeverity::Error, TEXT("AE-M2-003"), TEXT("Literature source requires title, authors, and a publication or dataset DOI."), Source.SourceId);
		}
		if (!Source.SourceFileRelativePath.IsEmpty() && !FPaths::IsRelative(Source.SourceFileRelativePath))
		{
			Result.Add(EAEValidationSeverity::Error, TEXT("AE-M2-004"), TEXT("Literature source file path must be project-relative."), Source.SourceId);
		}
		if (!Source.SourceFileSHA256.IsEmpty() && !AEParameterPackagePrivate::IsSHA256(Source.SourceFileSHA256))
		{
			Result.Add(EAEValidationSeverity::Error, TEXT("AE-M2-005"), TEXT("Literature source SHA-256 must contain 64 hexadecimal characters."), Source.SourceId);
		}
	}

	// Validate raw evidence without changing source values or units.
	for (const FAEEvidenceRecord& Evidence : EvidenceAsset.EvidenceRecords)
	{
		if (!Evidence.EvidenceId.IsValid() || EvidenceIds.Contains(Evidence.EvidenceId))
		{
			Result.Add(EAEValidationSeverity::Error, TEXT("AE-M2-006"), TEXT("Evidence GUID is invalid or duplicated."), Evidence.EvidenceId);
		}
		EvidenceIds.Add(Evidence.EvidenceId);
		EvidenceById.Add(Evidence.EvidenceId, &Evidence);
		if (!SourceIds.Contains(Evidence.SourceId))
		{
			Result.Add(EAEValidationSeverity::Error, TEXT("AE-M2-007"), TEXT("Evidence references a missing literature source."), Evidence.EvidenceId);
		}
		if (Evidence.ParameterConcept.IsEmpty() || Evidence.RawUnit.IsEmpty() || !FMath::IsFinite(Evidence.RawValue))
		{
			Result.Add(EAEValidationSeverity::Error, TEXT("AE-M2-008"), TEXT("Evidence requires a parameter concept, raw unit, and finite raw value."), Evidence.EvidenceId);
		}
		if (Evidence.EvidenceType == EAEEvidenceType::Range &&
			(!FMath::IsFinite(Evidence.RawMinimum) || !FMath::IsFinite(Evidence.RawMaximum) || Evidence.RawMinimum > Evidence.RawMaximum))
		{
			Result.Add(EAEValidationSeverity::Error, TEXT("AE-M2-009"), TEXT("Range evidence requires finite ordered bounds."), Evidence.EvidenceId);
		}
		if (Evidence.PageOrTable.IsEmpty() && Evidence.DatasetLocator.IsEmpty())
		{
			Result.Add(EAEValidationSeverity::Error, TEXT("AE-M2-010"), TEXT("Evidence requires a page/table or dataset locator."), Evidence.EvidenceId);
		}
	}

	// Validate each transformation link and recompute every standard conversion.
	for (const FAEEvidenceTransformation& Transformation : EvidenceAsset.Transformations)
	{
		if (!Transformation.TransformationId.IsValid() || TransformationIds.Contains(Transformation.TransformationId))
		{
			Result.Add(EAEValidationSeverity::Error, TEXT("AE-M2-011"), TEXT("Transformation GUID is invalid or duplicated."), Transformation.TransformationId);
		}
		TransformationIds.Add(Transformation.TransformationId);

		const FAEEvidenceRecord* const* Evidence = EvidenceById.Find(Transformation.EvidenceId);
		if (Evidence == nullptr)
		{
			Result.Add(EAEValidationSeverity::Error, TEXT("AE-M2-012"), TEXT("Transformation references missing evidence."), Transformation.TransformationId);
			continue;
		}
		if (!AEParameterPackagePrivate::IsSemanticVersion(Transformation.MethodVersion))
		{
			Result.Add(EAEValidationSeverity::Error, TEXT("AE-M2-013"), TEXT("Transformation method version must use major.minor.patch form."), Transformation.TransformationId);
		}

		double RecomputedValue = 0.0;
		FString Error;
		if (!ComputeTransformation(**Evidence, Transformation, RecomputedValue, Error))
		{
			Result.Add(EAEValidationSeverity::Error, TEXT("AE-M2-014"), Error, Transformation.TransformationId);
		}
		else if (!FMath::IsNearlyEqual(RecomputedValue, Transformation.NormalizedValue, AEParameterPackagePrivate::NumericTolerance))
		{
			Result.Add(EAEValidationSeverity::Error, TEXT("AE-M2-015"), TEXT("Stored normalized value does not match deterministic recomputation."), Transformation.TransformationId);
		}
	}

	return Result;
}

/* Validates complete provenance links, synthesis results, effective values, versions, and content hash. */
FAEValidationResult FAEParameterPackageService::ValidatePackage(
	const UAELiteratureEvidenceAsset& EvidenceAsset,
	const UAEParameterSynthesisAsset& Package)
{
	FAEValidationResult Result = ValidateEvidenceAsset(EvidenceAsset);
	TMap<FGuid, const FAEEvidenceRecord*> EvidenceById;
	TMap<FGuid, const FAEEvidenceTransformation*> TransformationById;
	TMap<FGuid, const FAEParameterSynthesis*> SynthesisById;
	TSet<FGuid> SynthesisIds;
	TSet<FGuid> ParameterIds;
	TSet<FName> ParameterNames;

	for (const FAEEvidenceRecord& Evidence : EvidenceAsset.EvidenceRecords)
	{
		EvidenceById.Add(Evidence.EvidenceId, &Evidence);
	}
	for (const FAEEvidenceTransformation& Transformation : EvidenceAsset.Transformations)
	{
		TransformationById.Add(Transformation.TransformationId, &Transformation);
	}

	// Validate immutable package identity and publication metadata.
	if (!Package.Metadata.PackageId.IsValid())
	{
		Result.Add(EAEValidationSeverity::Error, TEXT("AE-M2-020"), TEXT("Parameter package requires a valid package GUID."));
	}
	if (Package.Metadata.SchemaVersion != 2)
	{
		Result.Add(EAEValidationSeverity::Error, TEXT("AE-M2-021"), TEXT("Parameter package schema must be version 2."));
	}
	if (!AEParameterPackagePrivate::IsSemanticVersion(Package.Metadata.SemanticVersion))
	{
		Result.Add(EAEValidationSeverity::Error, TEXT("AE-M2-022"), TEXT("Parameter package version must use major.minor.patch form."));
	}
	if (Package.Metadata.CreatedBy.IsEmpty() || Package.Metadata.CreatedAt == FDateTime())
	{
		Result.Add(EAEValidationSeverity::Error, TEXT("AE-M2-023"), TEXT("Parameter package requires creator and creation time."));
	}

	// Validate every synthesis, provenance link, compact score, unit, and stored result.
	for (const FAEParameterSynthesis& Synthesis : Package.Syntheses)
	{
		if (!Synthesis.SynthesisId.IsValid() || SynthesisIds.Contains(Synthesis.SynthesisId))
		{
			Result.Add(EAEValidationSeverity::Error, TEXT("AE-M2-024"), TEXT("Synthesis GUID is invalid or duplicated."), Synthesis.SynthesisId);
		}
		SynthesisIds.Add(Synthesis.SynthesisId);
		SynthesisById.Add(Synthesis.SynthesisId, &Synthesis);
		if (Synthesis.TargetParameter.IsNone() || Synthesis.OutputUnit.IsEmpty() || !AEParameterPackagePrivate::IsSemanticVersion(Synthesis.SynthesisVersion))
		{
			Result.Add(EAEValidationSeverity::Error, TEXT("AE-M2-025"), TEXT("Synthesis requires target parameter, output unit, and semantic version."), Synthesis.SynthesisId);
		}

		TSet<FGuid> IncludedEvidenceIds;
		TSet<FGuid> IncludedSourceIds;
		for (const FAESynthesisContribution& Contribution : Synthesis.Contributions)
		{
			const FAEEvidenceRecord* const* Evidence = EvidenceById.Find(Contribution.EvidenceId);
			const FAEEvidenceTransformation* const* Transformation = TransformationById.Find(Contribution.TransformationId);
			if (Evidence == nullptr || Transformation == nullptr ||
				(Transformation != nullptr && (*Transformation)->EvidenceId != Contribution.EvidenceId))
			{
				Result.Add(EAEValidationSeverity::Error, TEXT("AE-M2-026"), TEXT("Contribution contains a broken evidence or transformation link."), Contribution.EvidenceId);
				continue;
			}
			if (!Contribution.NormalizedUnit.Equals(Synthesis.OutputUnit, ESearchCase::CaseSensitive) ||
				!Contribution.NormalizedUnit.Equals((*Transformation)->OutputUnit, ESearchCase::CaseSensitive) ||
				!FMath::IsNearlyEqual(Contribution.NormalizedValue, (*Transformation)->NormalizedValue, AEParameterPackagePrivate::NumericTolerance))
			{
				Result.Add(EAEValidationSeverity::Error, TEXT("AE-M2-027"), TEXT("Contribution value or unit differs from its transformation."), Contribution.EvidenceId);
			}
			if (!AEParameterPackagePrivate::IsUnitScore(Contribution.ContextRelevance) ||
				!AEParameterPackagePrivate::IsUnitScore(Contribution.StudyQuality) ||
				!AEParameterPackagePrivate::IsUnitScore(Contribution.ResearcherConfidence))
			{
				Result.Add(EAEValidationSeverity::Error, TEXT("AE-M2-028"), TEXT("Contribution score components must be finite values from zero to one."), Contribution.EvidenceId);
			}
			const double ExpectedWeight =
				static_cast<double>(Contribution.ContextRelevance) *
				static_cast<double>(Contribution.StudyQuality) *
				static_cast<double>(Contribution.ResearcherConfidence);
			if (!FMath::IsNearlyEqual(Contribution.AssignedWeight, ExpectedWeight, AEParameterPackagePrivate::NumericTolerance))
			{
				Result.Add(EAEValidationSeverity::Error, TEXT("AE-M2-029"), TEXT("Contribution weight does not equal relevance times quality times confidence."), Contribution.EvidenceId);
			}
			if (Contribution.bIncluded)
			{
				IncludedEvidenceIds.Add(Contribution.EvidenceId);
				IncludedSourceIds.Add((*Evidence)->SourceId);
				if (Contribution.InclusionReason.IsEmpty())
				{
					Result.Add(EAEValidationSeverity::Error, TEXT("AE-M2-030"), TEXT("Included contribution requires an inclusion reason."), Contribution.EvidenceId);
				}
			}
			else if (Contribution.ExclusionReason.IsEmpty())
			{
				Result.Add(EAEValidationSeverity::Error, TEXT("AE-M2-031"), TEXT("Excluded contribution requires an exclusion reason."), Contribution.EvidenceId);
			}
		}

		const bool bApproved = Synthesis.ReviewStatus.Equals(TEXT("Approved"), ESearchCase::IgnoreCase);
		if (IncludedEvidenceIds.Num() < 2)
		{
			Result.Add(
				bApproved ? EAEValidationSeverity::Error : EAEValidationSeverity::Warning,
				TEXT("AE-M2-032"),
				TEXT("Synthesis has fewer than two included evidence records."),
				Synthesis.SynthesisId);
		}
		if (IncludedSourceIds.Num() < 2)
		{
			Result.Add(EAEValidationSeverity::Warning, TEXT("AE-M2-033"), TEXT("Synthesis does not yet represent two independent literature sources."), Synthesis.SynthesisId);
		}

		double Value = 0.0;
		double Minimum = 0.0;
		double Maximum = 0.0;
		FString Error;
		if (!ComputeSynthesis(Synthesis, Value, Minimum, Maximum, Error))
		{
			Result.Add(EAEValidationSeverity::Error, TEXT("AE-M2-034"), Error, Synthesis.SynthesisId);
		}
		else if (!FMath::IsNearlyEqual(Value, Synthesis.ComputedValue, AEParameterPackagePrivate::NumericTolerance) ||
			!FMath::IsNearlyEqual(Minimum, Synthesis.ComputedMinimum, AEParameterPackagePrivate::NumericTolerance) ||
			!FMath::IsNearlyEqual(Maximum, Synthesis.ComputedMaximum, AEParameterPackagePrivate::NumericTolerance))
		{
			Result.Add(EAEValidationSeverity::Error, TEXT("AE-M2-035"), TEXT("Stored synthesis result differs from deterministic recomputation."), Synthesis.SynthesisId);
		}
	}

	// Validate derived parameter identity, semantic contracts, calibration separation, and ranges.
	for (const FAEDerivedParameter& Parameter : Package.DerivedParameters)
	{
		if (!Parameter.ParameterId.IsValid() || ParameterIds.Contains(Parameter.ParameterId) || Parameter.ParameterName.IsNone() || ParameterNames.Contains(Parameter.ParameterName))
		{
			Result.Add(EAEValidationSeverity::Error, TEXT("AE-M2-040"), TEXT("Derived parameter identity or name is invalid or duplicated."), Parameter.ParameterId);
		}
		ParameterIds.Add(Parameter.ParameterId);
		ParameterNames.Add(Parameter.ParameterName);

		const FAEParameterSynthesis* const* Synthesis = SynthesisById.Find(Parameter.SynthesisId);
		if (Synthesis == nullptr || (Synthesis != nullptr && (*Synthesis)->TargetParameter != Parameter.ParameterName))
		{
			Result.Add(EAEValidationSeverity::Error, TEXT("AE-M2-041"), TEXT("Derived parameter references a missing or differently named synthesis."), Parameter.ParameterId);
			continue;
		}
		if (Parameter.Unit.IsEmpty() || !Parameter.Unit.Equals((*Synthesis)->OutputUnit, ESearchCase::CaseSensitive) || !AEParameterPackagePrivate::IsSemanticVersion(Parameter.ParameterVersion))
		{
			Result.Add(EAEValidationSeverity::Error, TEXT("AE-M2-042"), TEXT("Derived parameter requires matching unit and semantic version."), Parameter.ParameterId);
		}
		if (!FMath::IsFinite(Parameter.EvidenceBasedValue) || !FMath::IsFinite(Parameter.RuntimeValue) ||
			!FMath::IsFinite(Parameter.PlausibleMinimum) || !FMath::IsFinite(Parameter.PlausibleMaximum) ||
			Parameter.PlausibleMinimum > Parameter.PlausibleMaximum ||
			Parameter.RuntimeValue < Parameter.PlausibleMinimum || Parameter.RuntimeValue > Parameter.PlausibleMaximum)
		{
			Result.Add(EAEValidationSeverity::Error, TEXT("AE-M2-043"), TEXT("Derived parameter values or plausible range are invalid."), Parameter.ParameterId);
		}
		if (!FMath::IsNearlyEqual(Parameter.EvidenceBasedValue, (*Synthesis)->ComputedValue, AEParameterPackagePrivate::NumericTolerance))
		{
			Result.Add(EAEValidationSeverity::Error, TEXT("AE-M2-044"), TEXT("Evidence-based value differs from its synthesis result."), Parameter.ParameterId);
		}
		if (Parameter.bHasManualAdjustment)
		{
			if (Parameter.AdjustmentMethod.IsEmpty() || Parameter.AdjustmentReason.IsEmpty())
			{
				Result.Add(EAEValidationSeverity::Error, TEXT("AE-M2-045"), TEXT("Manual adjustment requires method and reason."), Parameter.ParameterId);
			}
		}
		else if (!FMath::IsNearlyEqual(Parameter.RuntimeValue, Parameter.EvidenceBasedValue, AEParameterPackagePrivate::NumericTolerance))
		{
			Result.Add(EAEValidationSeverity::Error, TEXT("AE-M2-046"), TEXT("Unadjusted runtime value must equal the evidence-based value."), Parameter.ParameterId);
		}
	}

	// Verify the package seal after all effective values have been checked.
	FString HashError;
	const FString ExpectedHash = ComputeContentHash(Package, HashError);
	if (ExpectedHash.IsEmpty())
	{
		Result.Add(EAEValidationSeverity::Error, TEXT("AE-M2-047"), HashError);
	}
	else if (!Package.Metadata.ContentHash.Equals(ExpectedHash, ESearchCase::CaseSensitive))
	{
		Result.Add(EAEValidationSeverity::Error, TEXT("AE-M2-048"), TEXT("Stored package content hash does not match canonical effective content."));
	}

	return Result;
}

/* Builds canonical compact JSON with stable field and array ordering for hashing and export. */
bool FAEParameterPackageService::BuildCanonicalPackageJson(
	const UAEParameterSynthesisAsset& Package,
	FString& OutJson,
	FString& OutError)
{
	/* Excludes mutable audit metadata from the hashable package representation. */
	return AEParameterPackagePrivate::BuildPackageJson(Package, false, OutJson, OutError);
}

/* Computes the lowercase SHA-256 digest of canonical effective package content. */
FString FAEParameterPackageService::ComputeContentHash(
	const UAEParameterSynthesisAsset& Package,
	FString& OutError)
{
	FString CanonicalJson;
	if (!BuildCanonicalPackageJson(Package, CanonicalJson, OutError))
	{
		return FString();
	}

	// Hash UTF-8 bytes so locale and TCHAR width cannot affect the digest.
	FTCHARToUTF8 UTF8(*CanonicalJson);
	if (UTF8.Length() < 0 || static_cast<uint64>(UTF8.Length()) > MAX_uint32)
	{
		OutError = TEXT("Canonical JSON is too large for platform SHA-256 hashing.");
		return FString();
	}

	OutError.Reset();
	/* Hashes the exact canonical UTF-8 byte sequence with the portable implementation. */
	return AEParameterPackagePrivate::ComputeSHA256(
		reinterpret_cast<const uint8*>(UTF8.Get()),
		UTF8.Length());
}

/* Writes canonical parameter-package JSON to an explicitly supplied filesystem path. */
bool FAEParameterPackageService::ExportPackageJson(
	const UAEParameterSynthesisAsset& Package,
	const FString& OutputPath,
	FString& OutError)
{
	OutError.Reset();
	if (OutputPath.IsEmpty())
	{
		OutError = TEXT("Parameter-package export path is empty.");
		return false;
	}

	// Include audit metadata and the stored content hash in the exported representation.
	FString Json;
	if (!AEParameterPackagePrivate::BuildPackageJson(Package, true, Json, OutError))
	{
		return false;
	}

	const FString Directory = FPaths::GetPath(OutputPath);
	if (!Directory.IsEmpty() && !IFileManager::Get().MakeDirectory(*Directory, true))
	{
		OutError = TEXT("Failed to create the parameter-package export directory.");
		return false;
	}
	if (!FFileHelper::SaveStringToFile(Json, *OutputPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		OutError = TEXT("Failed to write parameter-package JSON.");
		return false;
	}

	return true;
}

/* Finds one effective runtime value together with its canonical unit and parameter version. */
bool FAEParameterPackageService::TryGetEffectiveParameter(
	const UAEParameterSynthesisAsset& Package,
	const FName ParameterName,
	double& OutRuntimeValue,
	FString& OutUnit,
	FString& OutParameterVersion)
{
	OutRuntimeValue = 0.0;
	OutUnit.Reset();
	OutParameterVersion.Reset();

	// Resolve by stable parameter name and reject invalid runtime values.
	const FAEDerivedParameter* Parameter = Package.DerivedParameters.FindByPredicate([ParameterName](const FAEDerivedParameter& Candidate)
	{
		return Candidate.ParameterName == ParameterName;
	});
	if (Parameter == nullptr || !FMath::IsFinite(Parameter->RuntimeValue) || Parameter->Unit.IsEmpty())
	{
		return false;
	}

	OutRuntimeValue = Parameter->RuntimeValue;
	OutUnit = Parameter->Unit;
	OutParameterVersion = Parameter->ParameterVersion;
	return true;
}
