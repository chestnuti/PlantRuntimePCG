#include "AEVegetationPatchStateService.h"

/* Allocate deterministic reverse-index storage. */
bool FAEVegetationPatchStateService::Initialize(const int32 InCellCount)
{
	if (InCellCount <= 0)
	{
		return false;
	}
	CellCount = InCellCount;
	PatchIdsByCellIndex.SetNum(CellCount);
	Reset();
	return true;
}

/* Clear all World-owned M7 state. */
void FAEVegetationPatchStateService::Reset()
{
	Patches.Reset();
	for (TArray<FGuid>& PatchIds : PatchIdsByCellIndex)
	{
		PatchIds.Reset();
	}
	ActiveTransitionPatchIds.Reset();
	VisualCommands.Reset();
	PatchStateRevision = 0;
	RejectedInputCount = 0;
}

/* Validate and commit one Patch plus its stable reverse-index links. */
bool FAEVegetationPatchStateService::RegisterPatch(
	const FAEVegetationPatchRegistration& Registration)
{
	if (!ValidateRegistration(Registration))
	{
		++RejectedInputCount;
		return false;
	}

	// Remove an earlier generation before rebuilding its reverse-index links.
	UnregisterPatch(Registration.PatchId);
	FPatchRecord& Record = Patches.Add(Registration.PatchId);
	Record.Registration = Registration;
	Record.Registration.WeightedCells.Sort(
		[](const FAEWeightedPatchCell& A, const FAEWeightedPatchCell& B)
		{
			return A.CellIndex < B.CellIndex;
		});
	for (const FAEWeightedPatchCell& WeightedCell : Record.Registration.WeightedCells)
	{
		PatchIdsByCellIndex[WeightedCell.CellIndex].Add(Registration.PatchId);
		PatchIdsByCellIndex[WeightedCell.CellIndex].Sort();
	}
	return true;
}

/* Remove one Patch from state and every Cell reverse index. */
void FAEVegetationPatchStateService::UnregisterPatch(const FGuid& PatchId)
{
	const FPatchRecord* Record = Patches.Find(PatchId);
	if (Record != nullptr)
	{
		for (const FAEWeightedPatchCell& WeightedCell : Record->Registration.WeightedCells)
		{
			if (PatchIdsByCellIndex.IsValidIndex(WeightedCell.CellIndex))
			{
				PatchIdsByCellIndex[WeightedCell.CellIndex].Remove(PatchId);
			}
		}
	}
	Patches.Remove(PatchId);
	ActiveTransitionPatchIds.Remove(PatchId);
}

/* Resolve M5-dirty and active Patch dependencies into stable Cell order. */
void FAEVegetationPatchStateService::BuildRequiredCellIndices(
	const TArray<int32>& M5ChangedIndices,
	TArray<int32>& OutIndices) const
{
	TSet<FGuid> CandidatePatchIds = ActiveTransitionPatchIds;
	for (const int32 CellIndex : M5ChangedIndices)
	{
		if (PatchIdsByCellIndex.IsValidIndex(CellIndex))
		{
			CandidatePatchIds.Append(PatchIdsByCellIndex[CellIndex]);
		}
	}

	TBitArray<> Required(false, CellCount);
	for (const FGuid& PatchId : CandidatePatchIds)
	{
		if (const FPatchRecord* Record = Patches.Find(PatchId))
		{
			for (const FAEWeightedPatchCell& WeightedCell : Record->Registration.WeightedCells)
			{
				Required[WeightedCell.CellIndex] = true;
			}
		}
	}
	OutIndices.Reset();
	for (TConstSetBitIterator<> Iterator(Required); Iterator; ++Iterator)
	{
		OutIndices.Add(Iterator.GetIndex());
	}
}

/* Aggregate shared M5 views and advance stable Patch health transitions. */
bool FAEVegetationPatchStateService::Update(
	const TConstArrayView<FAEM5ConsumerCellView> Workset,
	const TArray<int32>& M5ChangedIndices,
	const double DeltaSimulationHours,
	const uint64 SimulationStep)
{
	if (!FMath::IsFinite(DeltaSimulationHours) || DeltaSimulationHours < 0.0)
	{
		return false;
	}
	VisualCommands.Reset();

	// Build one validated row-major lookup from the shared M5 workset.
	TMap<int32, const FAEM5ConsumerCellView*> ViewsByIndex;
	TSet<FGuid> CandidatePatchIds = ActiveTransitionPatchIds;
	for (const FAEM5ConsumerCellView& View : Workset)
	{
		if (View.CellIndex < 0 || View.CellIndex >= CellCount
			|| !FMath::IsFinite(View.DamageRatio)
			|| View.DamageRatio < 0.0 || View.DamageRatio > 1.0)
		{
			++RejectedInputCount;
			continue;
		}
		ViewsByIndex.Add(View.CellIndex, &View);
	}
	for (const int32 CellIndex : M5ChangedIndices)
	{
		if (PatchIdsByCellIndex.IsValidIndex(CellIndex))
		{
			CandidatePatchIds.Append(PatchIdsByCellIndex[CellIndex]);
		}
	}

	// Sort Patch identities so hash-container iteration cannot change results.
	TArray<FGuid> OrderedPatchIds = CandidatePatchIds.Array();
	OrderedPatchIds.Sort();
	TArray<FGuid> ChangedPatchIds;
	for (const FGuid& PatchId : OrderedPatchIds)
	{
		FPatchRecord* Record = Patches.Find(PatchId);
		if (Record == nullptr)
		{
			continue;
		}

		// Require a complete view for every weighted Cell before changing the target.
		double WeightedDamage = 0.0;
		double TotalWeight = 0.0;
		uint64 MaximumSourceRevision = 0;
		bool bComplete = true;
		for (const FAEWeightedPatchCell& WeightedCell : Record->Registration.WeightedCells)
		{
			const FAEM5ConsumerCellView* const* View = ViewsByIndex.Find(WeightedCell.CellIndex);
			if (View == nullptr)
			{
				bComplete = false;
				break;
			}
			WeightedDamage += (*View)->DamageRatio * WeightedCell.Weight;
			TotalWeight += WeightedCell.Weight;
			MaximumSourceRevision = FMath::Max(MaximumSourceRevision, (*View)->ResponseRevision);
		}

		const FAEVegetationPatchState Previous = Record->State;
		FAEVegetationPatchState Next = Previous;
		if (bComplete && TotalWeight > 0.0)
		{
			if (Record->bInitialized && MaximumSourceRevision < Previous.SourceResponseRevision)
			{
				++RejectedInputCount;
				continue;
			}
			Next.TargetHealthRatio = CalculateTargetHealth(
				WeightedDamage / TotalWeight,
				Record->Registration.Parameters);
			Next.SourceResponseRevision = MaximumSourceRevision;
		}
		else if (!Record->bInitialized)
		{
			continue;
		}

		// Advance health toward the cached target even when M5 did not change.
		const double Rate = Next.TargetHealthRatio < Previous.HealthRatio
			? Record->Registration.Parameters.DeclineRatePerSimulationHour
			: Record->Registration.Parameters.RecoveryRatePerSimulationHour;
		Next.HealthRatio = FMath::Clamp(
			FMath::FInterpConstantTo(
				Previous.HealthRatio,
				Next.TargetHealthRatio,
				DeltaSimulationHours,
				Rate),
			0.0,
			1.0);
		Next.SimulationStep = SimulationStep;
		const bool bChanged = !Record->bInitialized
			|| !FMath::IsNearlyEqual(
				Previous.TargetHealthRatio,
				Next.TargetHealthRatio,
				Record->Registration.Parameters.HealthDirtyEpsilon)
			|| !FMath::IsNearlyEqual(
				Previous.HealthRatio,
				Next.HealthRatio,
				Record->Registration.Parameters.HealthDirtyEpsilon)
			|| Previous.SourceResponseRevision != Next.SourceResponseRevision;
		Record->State = Next;
		Record->bInitialized = true;
		if (FMath::IsNearlyEqual(
			Next.HealthRatio,
			Next.TargetHealthRatio,
			Record->Registration.Parameters.HealthDirtyEpsilon))
		{
			ActiveTransitionPatchIds.Remove(PatchId);
		}
		else
		{
			ActiveTransitionPatchIds.Add(PatchId);
		}
		if (bChanged)
		{
			ChangedPatchIds.Add(PatchId);
		}
	}

	// Commit one M7 revision and emit immutable Patch-level visual commands.
	if (!ChangedPatchIds.IsEmpty())
	{
		++PatchStateRevision;
		for (const FGuid& PatchId : ChangedPatchIds)
		{
			FPatchRecord& Record = Patches.FindChecked(PatchId);
			Record.State.PatchStateRevision = PatchStateRevision;
			VisualCommands.Add({
				PatchId,
				static_cast<float>(Record.State.HealthRatio),
				static_cast<float>(CalculateDensity(
					Record.State.HealthRatio,
					Record.Registration.Parameters)),
				PatchStateRevision,
				SimulationStep,
				Record.Registration.RegistrationGeneration,
				Record.Registration.Component});
		}
	}
	return true;
}

/* Copy one committed Patch into the reflected read-only contract. */
bool FAEVegetationPatchStateService::GetPatchSnapshot(
	const FGuid& PatchId,
	FAEVegetationPatchSnapshot& OutSnapshot) const
{
	const FPatchRecord* Record = Patches.Find(PatchId);
	if (Record == nullptr || !Record->bInitialized)
	{
		return false;
	}
	OutSnapshot.PatchId = PatchId;
	OutSnapshot.SpeciesId = Record->Registration.SpeciesId;
	OutSnapshot.TargetHealthRatio = static_cast<float>(Record->State.TargetHealthRatio);
	OutSnapshot.HealthRatio = static_cast<float>(Record->State.HealthRatio);
	OutSnapshot.DensityRatio = static_cast<float>(CalculateDensity(
		Record->State.HealthRatio,
		Record->Registration.Parameters));
	OutSnapshot.SourceResponseRevision = static_cast<int64>(Record->State.SourceResponseRevision);
	OutSnapshot.PatchStateRevision = static_cast<int64>(Record->State.PatchStateRevision);
	OutSnapshot.SimulationStep = static_cast<int64>(Record->State.SimulationStep);
	return true;
}

/* Validate identity, weights, component ownership, and numerical parameters. */
bool FAEVegetationPatchStateService::ValidateRegistration(
	const FAEVegetationPatchRegistration& Registration) const
{
	const FAEM7SpeciesParameters& Parameters = Registration.Parameters;
	if (!Registration.PatchId.IsValid() || !Registration.SpeciesId.IsValid()
		|| Registration.RegistrationGeneration == 0
		|| !Registration.Component.IsValid()
		|| Registration.WeightedCells.IsEmpty()
		|| !FMath::IsFinite(Parameters.DamageToleranceRatio)
		|| !FMath::IsFinite(Parameters.DamageResponseExponent)
		|| !FMath::IsFinite(Parameters.MinimumDensityRatio)
		|| !FMath::IsFinite(Parameters.DeclineRatePerSimulationHour)
		|| !FMath::IsFinite(Parameters.RecoveryRatePerSimulationHour)
		|| !FMath::IsFinite(Parameters.HealthDirtyEpsilon)
		|| Parameters.DamageToleranceRatio < 0.0 || Parameters.DamageToleranceRatio >= 1.0
		|| Parameters.DamageResponseExponent <= 0.0
		|| Parameters.MinimumDensityRatio < 0.0 || Parameters.MinimumDensityRatio > 1.0
		|| Parameters.DeclineRatePerSimulationHour < 0.0
		|| Parameters.RecoveryRatePerSimulationHour < 0.0
		|| Parameters.HealthDirtyEpsilon <= 0.0 || Parameters.HealthDirtyEpsilon > 1.0)
	{
		return false;
	}
	double TotalWeight = 0.0;
	TSet<int32> UniqueIndices;
	for (const FAEWeightedPatchCell& WeightedCell : Registration.WeightedCells)
	{
		if (WeightedCell.CellIndex < 0 || WeightedCell.CellIndex >= CellCount
			|| !FMath::IsFinite(WeightedCell.Weight) || WeightedCell.Weight <= 0.0
			|| UniqueIndices.Contains(WeightedCell.CellIndex))
		{
			return false;
		}
		UniqueIndices.Add(WeightedCell.CellIndex);
		TotalWeight += WeightedCell.Weight;
	}
	// Accept only a finite positive total aggregation weight.
	return FMath::IsFinite(TotalWeight) && TotalWeight > 0.0;
}

/* Map bounded M5 Damage through one species tolerance curve. */
double FAEVegetationPatchStateService::CalculateTargetHealth(
	const double DamageRatio,
	const FAEM7SpeciesParameters& Parameters)
{
	const double NormalizedDamage = FMath::Clamp(
		(DamageRatio - Parameters.DamageToleranceRatio)
		/ FMath::Max(1.0 - Parameters.DamageToleranceRatio, UE_DOUBLE_SMALL_NUMBER),
		0.0,
		1.0);
	return 1.0 - FMath::Pow(NormalizedDamage, Parameters.DamageResponseExponent);
}

/* Derive density linearly from health and the species survival floor. */
double FAEVegetationPatchStateService::CalculateDensity(
	const double HealthRatio,
	const FAEM7SpeciesParameters& Parameters)
{
	// Interpolate between the species survival floor and full density.
	return FMath::Lerp(
		Parameters.MinimumDensityRatio,
		1.0,
		FMath::Clamp(HealthRatio, 0.0, 1.0));
}
