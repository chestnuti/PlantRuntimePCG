#include "AEPlantBiomeMapAsset.h"

/* Samples the normalized biome prior with bilinear interpolation. */
float UAEPlantBiomeMapAsset::SampleWeight(const FVector& WorldLocation) const
{
	FString Error;
	if (!IsValidMap(Error))
	{
		return 0.0f;
	}
	const FVector2D Size = WorldMax - WorldMin;
	const double U = FMath::Clamp((WorldLocation.X - WorldMin.X) / Size.X, 0.0, 1.0);
	const double V = FMath::Clamp((WorldLocation.Y - WorldMin.Y) / Size.Y, 0.0, 1.0);
	const double X = U * static_cast<double>(Dimensions.X - 1);
	const double Y = V * static_cast<double>(Dimensions.Y - 1);
	const int32 X0 = FMath::FloorToInt(X);
	const int32 Y0 = FMath::FloorToInt(Y);
	const int32 X1 = FMath::Min(X0 + 1, Dimensions.X - 1);
	const int32 Y1 = FMath::Min(Y0 + 1, Dimensions.Y - 1);
	const float A = FMath::Lerp(Weights[Y0 * Dimensions.X + X0], Weights[Y0 * Dimensions.X + X1], X - X0);
	const float B = FMath::Lerp(Weights[Y1 * Dimensions.X + X0], Weights[Y1 * Dimensions.X + X1], X - X0);
	const float Sample = FMath::Clamp(FMath::Lerp(A, B, Y - Y0), 0.0f, 1.0f);
	return Sample;
}

/* Validates dimensions, bounds, and normalized weight storage. */
bool UAEPlantBiomeMapAsset::IsValidMap(FString& OutError) const
{
	if (Dimensions.X <= 0 || Dimensions.Y <= 0 || Weights.Num() != Dimensions.X * Dimensions.Y)
	{
		OutError = TEXT("Biome map dimensions do not match its weight array.");
		return false;
	}
	if (WorldMax.X <= WorldMin.X || WorldMax.Y <= WorldMin.Y)
	{
		OutError = TEXT("Biome map world bounds are empty.");
		return false;
	}
	for (const float Weight : Weights)
	{
		if (!FMath::IsFinite(Weight) || Weight < 0.0f || Weight > 1.0f)
		{
			OutError = TEXT("Biome map weights must be finite values in [0,1].");
			return false;
		}
	}
	OutError.Reset();
	return true;
}
