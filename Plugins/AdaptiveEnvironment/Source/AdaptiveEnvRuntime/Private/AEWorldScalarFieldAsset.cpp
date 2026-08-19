#include "AEWorldScalarFieldAsset.h"

#include "Engine/Texture2D.h"

/* Rebuild runtime CPU data from the texture's editor source mip. */
bool UAEWorldScalarFieldAsset::RebuildFromSourceTexture()
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
	const int64 PixelCount = static_cast<int64>(Width) * Height;
	const int32 BytesPerPixel = Format == TSF_G8 ? 1 : 4;
	if (Width <= 0 || Height <= 0 || SourceData.Num() != PixelCount * BytesPerPixel || PixelCount > MAX_int32)
	{
		return false;
	}
	Modify();
	TArray<uint16> Candidate;
	Candidate.SetNumUninitialized(static_cast<int32>(PixelCount));
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

/* Sample the baked field in world XY with a stable outside policy. */
bool UAEWorldScalarFieldAsset::SampleValue(const FVector& WorldLocation, const float FallbackValue, float& OutValue) const
{
	FString Error;
	if (!IsValidField(Error) || WorldLocation.ContainsNaN() || !FMath::IsFinite(FallbackValue))
	{
		return false;
	}
	const FVector2D Size = WorldMax - WorldMin;
	double U = (WorldLocation.X - WorldMin.X) / Size.X;
	double V = (WorldLocation.Y - WorldMin.Y) / Size.Y;
	const bool bOutside = U < 0.0 || U > 1.0 || V < 0.0 || V > 1.0;
	if (bOutside && OutsidePolicy == EAETextureOutsidePolicy::UseFallback)
	{
		OutValue = FMath::Clamp(FallbackValue, 0.0f, 1.0f);
		return true;
	}
	if (bOutside && OutsidePolicy == EAETextureOutsidePolicy::Zero)
	{
		OutValue = 0.0f;
		return true;
	}
	U = FMath::Clamp(U, 0.0, 1.0);
	V = FMath::Clamp(V, 0.0, 1.0);
	if (bFlipVerticalAxis)
	{
		V = 1.0 - V;
	}
	const double X = U * (BakedDimensions.X - 1);
	const double Y = V * (BakedDimensions.Y - 1);
	const int32 X0 = FMath::FloorToInt(X);
	const int32 Y0 = FMath::FloorToInt(Y);
	const int32 X1 = FMath::Min(X0 + 1, BakedDimensions.X - 1);
	const int32 Y1 = FMath::Min(Y0 + 1, BakedDimensions.Y - 1);
	const auto Read = [this](const int32 SX, const int32 SY)
	{
		return static_cast<float>(BakedSamples[SY * BakedDimensions.X + SX]) / 65535.0f;
	};
	const float A = FMath::Lerp(Read(X0, Y0), Read(X1, Y0), static_cast<float>(X - X0));
	const float B = FMath::Lerp(Read(X0, Y1), Read(X1, Y1), static_cast<float>(X - X0));
	OutValue = FMath::Clamp(FMath::Lerp(A, B, static_cast<float>(Y - Y0)), 0.0f, 1.0f);
	return true;
}

/* Validate the complete baked field contract without accessing editor-only source pixels. */
bool UAEWorldScalarFieldAsset::IsValidField(FString& OutError) const
{
	const int64 PixelCount = static_cast<int64>(BakedDimensions.X) * BakedDimensions.Y;
	if (BakedDimensions.X <= 0 || BakedDimensions.Y <= 0 || PixelCount <= 0
		|| PixelCount > MAX_int32 || BakedSamples.Num() != PixelCount)
	{
		OutError = TEXT("Baked dimensions do not match scalar samples.");
		return false;
	}
	if (WorldMax.X <= WorldMin.X || WorldMax.Y <= WorldMin.Y)
	{
		OutError = TEXT("Scalar field world bounds are empty.");
		return false;
	}
	OutError.Reset();
	return true;
}
