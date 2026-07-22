#pragma once

#include "CoreMinimal.h"

class UAEMoistureSourceComponent;
class UWorld;

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
	/* Samples one Cell on the Game Thread and returns false without partial output on failure. */
	static bool SampleCell(
		UWorld& World,
		const FIntPoint& Coordinate,
		const FVector& XYCenter,
		float TraceHalfHeightCm,
		float DefaultMoistureRatio,
		const TArray<TWeakObjectPtr<UAEMoistureSourceComponent>>& Sources,
		FAEWorldConstraintObservation& OutObservation);
};
