#include "AEPlantSuitabilityLUTAsset.h"

#include "Engine/Texture2D.h"

#if WITH_EDITOR
#include "UObject/UnrealType.h"
#endif

/* Bake the source red channel into platform-independent normalized CPU samples. */
bool UAEPlantSuitabilityLUTAsset::RebuildFromSourceTexture()
{
#if WITH_EDITOR
	if (SourceTexture == nullptr || SourceTexture->SRGB || !SourceTexture->Source.IsValid())
	{
		return false;
	}
	const ETextureSourceFormat Format = SourceTexture->Source.GetFormat();
	if (Format != TSF_G8 && Format != TSF_BGRA8)
	{
		return false;
	}
	TArray64<uint8> SourceData;
	if (!SourceTexture->Source.GetMipData(SourceData, 0))
	{
		return false;
	}
	const int32 Width = SourceTexture->Source.GetSizeX();
	const int32 Height = SourceTexture->Source.GetSizeY();
	const int64 SampleCount = static_cast<int64>(Width) * Height;
	const int32 BytesPerPixel = Format == TSF_G8 ? 1 : 4;
	if (Width <= 0 || Height <= 0 || SampleCount > MAX_int32
		|| SourceData.Num() != SampleCount * BytesPerPixel)
	{
		return false;
	}

	Modify();
	TArray<uint16> Candidate;
	Candidate.SetNumUninitialized(static_cast<int32>(SampleCount));
	for (int32 Index = 0; Index < Candidate.Num(); ++Index)
	{
		const uint8 Red = Format == TSF_G8 ? SourceData[Index] : SourceData[Index * 4 + 2];
		Candidate[Index] = static_cast<uint16>(Red) * 257u;
	}
	BakedDimensions = FIntPoint(Width, Height);
	BakedSamples = MoveTemp(Candidate);
	ContentRevision = ContentRevision == MAX_int32 ? 1 : ContentRevision + 1;
	MarkPackageDirty();
	return true;
#else
	return false;
#endif
}

/* Resolve clamped physical inputs into the authored response surface. */
bool UAEPlantSuitabilityLUTAsset::SampleSuitability(
	const float SlopeDegrees,
	const float MoistureRatio,
	float& OutSuitabilityRatio) const
{
	FString Error;
	if (!IsValidLUT(Error) || !FMath::IsFinite(SlopeDegrees) || !FMath::IsFinite(MoistureRatio))
	{
		return false;
	}
	const double U = FMath::Clamp(
		(static_cast<double>(MoistureRatio) - MoistureMinimumRatio)
		/ (MoistureMaximumRatio - MoistureMinimumRatio),
		0.0,
		1.0);
	const double V = FMath::Clamp(
		(static_cast<double>(SlopeDegrees) - SlopeMinimumDegrees)
		/ (SlopeMaximumDegrees - SlopeMinimumDegrees),
		0.0,
		1.0);
	const double X = U * (BakedDimensions.X - 1);
	const double Y = V * (BakedDimensions.Y - 1);
	const int32 X0 = FMath::FloorToInt(X);
	const int32 Y0 = FMath::FloorToInt(Y);
	const int32 X1 = FMath::Min(X0 + 1, BakedDimensions.X - 1);
	const int32 Y1 = FMath::Min(Y0 + 1, BakedDimensions.Y - 1);
	const auto Read = [this](const int32 SampleX, const int32 SampleY)
	{
		return static_cast<float>(BakedSamples[SampleY * BakedDimensions.X + SampleX]) / 65535.0f;
	};
	const float Lower = FMath::Lerp(Read(X0, Y0), Read(X1, Y0), static_cast<float>(X - X0));
	const float Upper = FMath::Lerp(Read(X0, Y1), Read(X1, Y1), static_cast<float>(X - X0));
	OutSuitabilityRatio = FMath::Clamp(
		FMath::Lerp(Lower, Upper, static_cast<float>(Y - Y0)),
		0.0f,
		1.0f);
	return true;
}

/* Reject empty, inverted, non-finite, or mismatched LUT contracts. */
bool UAEPlantSuitabilityLUTAsset::IsValidLUT(FString& OutError) const
{
	const int64 SampleCount = static_cast<int64>(BakedDimensions.X) * BakedDimensions.Y;
	if (BakedDimensions.X <= 0 || BakedDimensions.Y <= 0 || SampleCount <= 0
		|| SampleCount > MAX_int32 || BakedSamples.Num() != SampleCount)
	{
		OutError = TEXT("Baked dimensions do not match suitability samples.");
		return false;
	}
	if (!FMath::IsFinite(MoistureMinimumRatio) || !FMath::IsFinite(MoistureMaximumRatio)
		|| !FMath::IsFinite(SlopeMinimumDegrees) || !FMath::IsFinite(SlopeMaximumDegrees)
		|| MoistureMinimumRatio < 0.0f || MoistureMaximumRatio > 1.0f
		|| MoistureMinimumRatio >= MoistureMaximumRatio
		|| SlopeMinimumDegrees < 0.0f || SlopeMaximumDegrees > 90.0f
		|| SlopeMinimumDegrees >= SlopeMaximumDegrees)
	{
		OutError = TEXT("Suitability LUT axes must be finite, ordered, and within their documented ranges.");
		return false;
	}
	OutError.Reset();
	return true;
}

#if WITH_EDITOR
void UAEPlantSuitabilityLUTAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	ContentRevision = ContentRevision == MAX_int32 ? 1 : ContentRevision + 1;
}
#endif
