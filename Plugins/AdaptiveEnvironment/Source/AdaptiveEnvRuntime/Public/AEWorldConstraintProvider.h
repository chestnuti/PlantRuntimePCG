#pragma once

#include "CoreMinimal.h"

class UAEMoistureSourceComponent;
class UAEMoistureTextureAsset;
class AActor;
class UWorld;

/* Stores one validated ecological ground hit in world space. */
struct ADAPTIVEENVRUNTIME_API FAEGroundSurfaceSample
{
	/* Stores the accepted ground impact location in world centimetres. */
	FVector WorldLocation = FVector::ZeroVector;
	/* Stores the normalized accepted ground surface normal. */
	FVector WorldNormal = FVector::UpVector;
};

/* Stores one validated Game Thread environment observation for an M4 Cell. */
struct ADAPTIVEENVRUNTIME_API FAEWorldConstraintObservation
{
	/* Stores the sampled Cell coordinate. */
	FIntPoint Coordinate = FIntPoint::ZeroValue;
	/* Stores the sampled ground location in world centimetres. */
	FVector WorldCenter = FVector::ZeroVector;
	/* Stores ground slope in degrees. */
	double SlopeDegrees = 0.0;
	/* Stores selected moisture in the zero-to-one range. */
	double MoistureRatio = 0.5;
	/* Indicates that both ground and moisture inputs are finite and usable. */
	bool bValid = false;
};

/* Samples terrain and registered moisture sources without owning ecological state. */
class ADAPTIVEENVRUNTIME_API FAEWorldConstraintProvider
{
public:
	/* Names the Component or Actor tag that authorizes an ecological ground surface. */
	static const FName EnvironmentGroundTag;
	/* Reports whether one hit has a finite ground normal and an approved ground identity. */
	static bool IsValidGroundHit(const FHitResult& Hit);
	/* Traces one world XY point and returns only an approved ecological ground surface. */
	static bool TraceGroundSurface(
		UWorld& World,
		const FVector& TraceCenter,
		float TraceHalfHeightCm,
		const AActor* IgnoredActor,
		FAEGroundSurfaceSample& OutSample);
	/* Samples one Cell on the Game Thread and returns false without partial output on failure. */
	static bool SampleCell(
		UWorld& World,
		const FIntPoint& Coordinate,
		const FVector& XYCenter,
		float TraceHalfHeightCm,
		float DefaultMoistureRatio,
		const UAEMoistureTextureAsset* MoistureTexture,
		const TArray<TWeakObjectPtr<UAEMoistureSourceComponent>>& Sources,
		FAEWorldConstraintObservation& OutObservation);
};
