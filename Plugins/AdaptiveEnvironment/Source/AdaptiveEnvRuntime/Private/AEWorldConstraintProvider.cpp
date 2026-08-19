#include "AEWorldConstraintProvider.h"

#include "AEMoistureSourceComponent.h"
#include "AEWorldScalarFieldAsset.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "LandscapeProxy.h"

namespace AEWorldConstraintProviderPrivate
{
	constexpr int32 MaximumRejectedGroundHits = 32;
}

const FName FAEWorldConstraintProvider::EnvironmentGroundTag = TEXT("AE.EnvironmentGround");

/* Reports whether one hit has a finite ground normal and an approved ground identity. */
bool FAEWorldConstraintProvider::IsValidGroundHit(const FHitResult& Hit)
{
	const UPrimitiveComponent* Component = Hit.GetComponent();
	const AActor* Actor = Hit.GetActor();
	if (Component == nullptr || !Hit.ImpactNormal.IsNormalized())
	{
		return false;
	}

	const bool bTaggedGround = Component->ComponentHasTag(EnvironmentGroundTag)
		|| (Actor != nullptr && Actor->ActorHasTag(EnvironmentGroundTag));
	const bool bLandscapeGround = Actor != nullptr && Actor->IsA<ALandscapeProxy>();
	return bTaggedGround || bLandscapeGround;
}

/* Trace one point and skip blocking components that are not authorized ecological ground. */
bool FAEWorldConstraintProvider::TraceGroundSurface(
	UWorld& World,
	const FVector& TraceCenter,
	const float TraceHalfHeightCm,
	const AActor* IgnoredActor,
	FAEGroundSurfaceSample& OutSample)
{
	check(IsInGameThread());
	if (TraceCenter.ContainsNaN() || !FMath::IsFinite(TraceHalfHeightCm) || TraceHalfHeightCm <= 0.0f)
	{
		return false;
	}

	// Query every collision object type and exclude the caller-owned Actor when requested.
	const float SafeHalfHeight = FMath::Max(TraceHalfHeightCm, 1.0f);
	const FVector Start(TraceCenter.X, TraceCenter.Y, TraceCenter.Z + SafeHalfHeight);
	const FVector End(TraceCenter.X, TraceCenter.Y, TraceCenter.Z - SafeHalfHeight);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AdaptiveEnvGround), false);
	if (IgnoredActor != nullptr)
	{
		QueryParams.AddIgnoredActor(IgnoredActor);
	}
	const FCollisionObjectQueryParams ObjectQueryParams(
		FCollisionObjectQueryParams::InitType::AllObjects);

	// Reject unauthorized blockers deterministically until valid ground or empty space is reached.
	FHitResult Hit;
	for (int32 Attempt = 0; Attempt < AEWorldConstraintProviderPrivate::MaximumRejectedGroundHits; ++Attempt)
	{
		Hit = FHitResult();
		if (!World.LineTraceSingleByObjectType(Hit, Start, End, ObjectQueryParams, QueryParams))
		{
			return false;
		}
		if (IsValidGroundHit(Hit))
		{
			if (Hit.ImpactPoint.ContainsNaN() || Hit.ImpactNormal.ContainsNaN())
			{
				return false;
			}
			FAEGroundSurfaceSample Candidate;
			Candidate.WorldLocation = Hit.ImpactPoint;
			Candidate.WorldNormal = Hit.ImpactNormal;
			OutSample = Candidate;
			return true;
		}
		if (UPrimitiveComponent* RejectedComponent = Hit.GetComponent())
		{
			QueryParams.AddIgnoredComponent(RejectedComponent);
			continue;
		}
		return false;
	}
	return false;
}

/* Trace ground and choose one deterministic overlapping moisture source. */
bool FAEWorldConstraintProvider::SampleCell(
	UWorld& World,
	const FIntPoint& Coordinate,
	const FVector& XYCenter,
	const float TraceHalfHeightCm,
	const float DefaultMoistureRatio,
	const UAEMoistureTextureAsset* MoistureTexture,
	const TArray<TWeakObjectPtr<UAEMoistureSourceComponent>>& Sources,
	FAEWorldConstraintObservation& OutObservation)
{
	check(IsInGameThread());
	FAEWorldConstraintObservation Candidate;
	Candidate.Coordinate = Coordinate;

	// Reuse the shared ecological ground contract before deriving M4-specific values.
	FAEGroundSurfaceSample Ground;
	if (!TraceGroundSurface(World, XYCenter, TraceHalfHeightCm, nullptr, Ground))
	{
		return false;
	}

	// Derive the environmental observation only from the accepted ground surface.
	Candidate.WorldCenter = Ground.WorldLocation;
	Candidate.SlopeDegrees = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(static_cast<double>(Ground.WorldNormal.Z), -1.0, 1.0)));

	// Resolve overlaps by priority and then stable SourceId.
	const UAEMoistureSourceComponent* Selected = nullptr;
	for (const TWeakObjectPtr<UAEMoistureSourceComponent>& WeakSource : Sources)
	{
		const UAEMoistureSourceComponent* Source = WeakSource.Get();
		if (Source == nullptr || !Source->ContainsWorldPoint(Candidate.WorldCenter) || !FMath::IsFinite(Source->MoistureRatio))
		{
			continue;
		}
		if (Selected == nullptr || Source->Priority > Selected->Priority
			|| (Source->Priority == Selected->Priority && Source->SourceId < Selected->SourceId))
		{
			Selected = Source;
		}
	}
	float MoistureRatio = DefaultMoistureRatio;
	if (Selected != nullptr)
	{
		MoistureRatio = Selected->MoistureRatio;
	}
	else if (MoistureTexture != nullptr)
	{
		MoistureTexture->SampleValue(Candidate.WorldCenter, DefaultMoistureRatio, MoistureRatio);
	}
	Candidate.MoistureRatio = FMath::Clamp(MoistureRatio, 0.0f, 1.0f);
	Candidate.bValid = FMath::IsFinite(Candidate.SlopeDegrees) && FMath::IsFinite(Candidate.MoistureRatio);
	if (!Candidate.bValid)
	{
		return false;
	}
	OutObservation = Candidate;
	return true;
}
