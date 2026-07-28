#include "AEWorldConstraintProvider.h"

#include "AEMoistureSourceComponent.h"
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

/* Trace ground and choose one deterministic overlapping moisture source. */
bool FAEWorldConstraintProvider::SampleCell(
	UWorld& World,
	const FIntPoint& Coordinate,
	const FVector& XYCenter,
	const float TraceHalfHeightCm,
	const float DefaultMoistureRatio,
	const TArray<TWeakObjectPtr<UAEMoistureSourceComponent>>& Sources,
	FAEWorldConstraintObservation& OutObservation)
{
	check(IsInGameThread());
	FAEWorldConstraintObservation Candidate;
	Candidate.Coordinate = Coordinate;

	// Query every collision object type, then reject hits by ecological identity.
	FHitResult Hit;
	const float SafeHalfHeight = FMath::Max(TraceHalfHeightCm, 1.0f);
	const FVector Start(XYCenter.X, XYCenter.Y, XYCenter.Z + SafeHalfHeight);
	const FVector End(XYCenter.X, XYCenter.Y, XYCenter.Z - SafeHalfHeight);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AdaptiveEnvM4Ground), false);
	const FCollisionObjectQueryParams ObjectQueryParams(
		FCollisionObjectQueryParams::InitType::AllObjects);
	bool bFoundValidGround = false;
	for (int32 Attempt = 0; Attempt < AEWorldConstraintProviderPrivate::MaximumRejectedGroundHits; ++Attempt)
	{
		Hit = FHitResult();
		if (!World.LineTraceSingleByObjectType(Hit, Start, End, ObjectQueryParams, QueryParams))
		{
			break;
		}
		if (IsValidGroundHit(Hit))
		{
			bFoundValidGround = true;
			break;
		}
		if (UPrimitiveComponent* RejectedComponent = Hit.GetComponent())
		{
			QueryParams.AddIgnoredComponent(RejectedComponent);
			continue;
		}
		break;
	}
	if (!bFoundValidGround)
	{
		return false;
	}

	// Derive the environmental observation only from the accepted ground surface.
	Candidate.WorldCenter = Hit.ImpactPoint;
	Candidate.SlopeDegrees = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(static_cast<double>(Hit.ImpactNormal.Z), -1.0, 1.0)));

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
	Candidate.MoistureRatio = FMath::Clamp(Selected ? Selected->MoistureRatio : DefaultMoistureRatio, 0.0f, 1.0f);
	Candidate.bValid = FMath::IsFinite(Candidate.SlopeDegrees) && FMath::IsFinite(Candidate.MoistureRatio);
	if (!Candidate.bValid)
	{
		return false;
	}
	OutObservation = Candidate;
	return true;
}
