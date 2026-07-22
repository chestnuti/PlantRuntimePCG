#include "AEWorldConstraintProvider.h"

#include "AEMoistureSourceComponent.h"
#include "Engine/World.h"

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

	// Trace vertically through the Cell centre and derive slope from the impact normal.
	FHitResult Hit;
	const float SafeHalfHeight = FMath::Max(TraceHalfHeightCm, 1.0f);
	const FVector Start(XYCenter.X, XYCenter.Y, XYCenter.Z + SafeHalfHeight);
	const FVector End(XYCenter.X, XYCenter.Y, XYCenter.Z - SafeHalfHeight);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AdaptiveEnvM4Ground), false);
	if (!World.LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, QueryParams) || !Hit.ImpactNormal.IsNormalized())
	{
		return false;
	}
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
