#include "AEPlantDistributionService.h"

/* Generates one deterministic maximum-density candidate pool. */
bool FAEPlantDistributionService::GenerateStableCandidatePool(
	const FAEPlantDistributionConfig& Config,
	TArray<FAEM7CandidatePoint>& OutCandidates,
	FString& OutError)
{
	OutCandidates.Reset();
	if (!Config.WorldBounds.bIsValid || Config.GridDimensions.X <= 0 || Config.GridDimensions.Y <= 0
		|| Config.SpeciesId.IsNone() || Config.MinimumSpacingCm <= 0.0f
		|| Config.MaximumInstancesPerSquareMeter <= 0.0f || Config.MaximumCandidateCount <= 0)
	{
		OutError = TEXT("M7 distribution config is incomplete.");
		return false;
	}
	const FVector2D Size = Config.WorldBounds.GetSize();
	const double AreaSquareMeters = Size.X * Size.Y / 10000.0;
	const int32 ExpectedCount = FMath::Clamp(
		FMath::CeilToInt(AreaSquareMeters * Config.MaximumInstancesPerSquareMeter),
		1,
		Config.MaximumCandidateCount);
	const int32 AttemptLimit = FMath::Max(ExpectedCount * Config.AttemptsPerExpectedPoint, ExpectedCount);
	const double MinimumDistanceSquared = FMath::Square(static_cast<double>(Config.MinimumSpacingCm));
	FRandomStream Random(Config.Seed ^ GetTypeHash(Config.SpeciesId));

	for (int32 Attempt = 0; Attempt < AttemptLimit && OutCandidates.Num() < ExpectedCount; ++Attempt)
	{
		FVector Location(
			FMath::RoundToDouble(Random.FRandRange(Config.WorldBounds.Min.X, Config.WorldBounds.Max.X)),
			FMath::RoundToDouble(Random.FRandRange(Config.WorldBounds.Min.Y, Config.WorldBounds.Max.Y)),
			0.0);
		bool bAccepted = true;
		for (const FAEM7CandidatePoint& Existing : OutCandidates)
		{
			if (FVector::DistSquared2D(Location, Existing.Location) < MinimumDistanceSquared)
			{
				bAccepted = false;
				break;
			}
		}
		if (!bAccepted) continue;

		const FVector2D Normalized(
			(Location.X - Config.WorldBounds.Min.X) / Size.X,
			(Location.Y - Config.WorldBounds.Min.Y) / Size.Y);
		FAEM7CandidatePoint& Candidate = OutCandidates.AddDefaulted_GetRef();
		Candidate.Location = Location;
		Candidate.CellCoordinate = FIntPoint(
			FMath::Clamp(FMath::FloorToInt(Normalized.X * Config.GridDimensions.X), 0, Config.GridDimensions.X - 1),
			FMath::Clamp(FMath::FloorToInt(Normalized.Y * Config.GridDimensions.Y), 0, Config.GridDimensions.Y - 1));
		uint64 Id = MixHash(static_cast<uint64>(Config.Seed) ^ static_cast<uint64>(GetTypeHash(Config.SpeciesId)));
		Id = MixHash(Id ^ static_cast<uint64>(OutCandidates.Num() - 1));
		Id = MixHash(Id ^ static_cast<uint64>(FMath::RoundToInt64(Location.X)));
		Id = MixHash(Id ^ static_cast<uint64>(FMath::RoundToInt64(Location.Y)));
		Candidate.StablePointId = Id & 0x7fffffffffffffffull;
		Candidate.SelectionKey = HashToUnitFloat(MixHash(Candidate.StablePointId ^ 0xA24BAED4963EE407ull));
		Candidate.HealthVariation =
			(HashToUnitFloat(MixHash(Candidate.StablePointId ^ 0x9FB21C651E98DF25ull)) * 2.0f - 1.0f)
			* Config.HealthVariationAmplitude;
	}
	OutError.Reset();
	return OutCandidates.Num() > 0;
}

/* Collects deterministic candidate coverage without expanding to unused Grid Cells. */
void FAEPlantDistributionService::CollectOccupiedCellIndices(
	const TArray<FAEM7CandidatePoint>& Candidates,
	const FIntPoint& GridDimensions,
	TArray<int32>& OutCellIndices)
{
	TSet<int32> UniqueIndices;
	if (GridDimensions.X > 0 && GridDimensions.Y > 0)
	{
		for (const FAEM7CandidatePoint& Candidate : Candidates)
		{
			if (Candidate.CellCoordinate.X >= 0
				&& Candidate.CellCoordinate.Y >= 0
				&& Candidate.CellCoordinate.X < GridDimensions.X
				&& Candidate.CellCoordinate.Y < GridDimensions.Y)
			{
				UniqueIndices.Add(
					Candidate.CellCoordinate.Y * GridDimensions.X
					+ Candidate.CellCoordinate.X);
			}
		}
	}
	OutCellIndices = UniqueIndices.Array();
	OutCellIndices.Sort();
}

/* Applies a stable 64-bit avalanche mix. */
uint64 FAEPlantDistributionService::MixHash(uint64 Value)
{
	Value ^= Value >> 30;
	Value *= 0xbf58476d1ce4e5b9ull;
	Value ^= Value >> 27;
	Value *= 0x94d049bb133111ebull;
	return Value ^ (Value >> 31);
}

/* Converts the high hash bits into a deterministic unit interval value. */
float FAEPlantDistributionService::HashToUnitFloat(const uint64 Value)
{
	return static_cast<float>((Value >> 40) & 0xffffffull) / 16777216.0f;
}
