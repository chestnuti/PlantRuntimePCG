#pragma once

#include "CoreMinimal.h"
#include "AEM5ConsumerTypes.h"
#include "AEM7Types.h"

/* Owns deterministic Patch aggregation and the minimal M7 health state. */
class ADAPTIVEENVRUNTIME_API FAEVegetationPatchStateService
{
public:
	/* Allocates the Cell-to-Patch reverse index for one shared Grid. */
	bool Initialize(int32 InCellCount);
	/* Clears registrations, state, active transitions, and diagnostics. */
	void Reset();
	/* Validates and atomically registers or replaces one Patch contract. */
	bool RegisterPatch(const FAEVegetationPatchRegistration& Registration);
	/* Removes one Patch and every reverse-index entry. */
	void UnregisterPatch(const FGuid& PatchId);
	/* Builds the stable union of M5-dirty Patch Cells and active transition dependencies. */
	void BuildRequiredCellIndices(const TArray<int32>& M5ChangedIndices, TArray<int32>& OutIndices) const;
	/* Advances M5-dirty and active Patches from one shared M5 consumer workset. */
	bool Update(
		TConstArrayView<FAEM5ConsumerCellView> Workset,
		const TArray<int32>& M5ChangedIndices,
		double DeltaSimulationHours,
		uint64 SimulationStep);
	/* Reads one committed Patch snapshot. */
	bool GetPatchSnapshot(const FGuid& PatchId, FAEVegetationPatchSnapshot& OutSnapshot) const;
	/* Returns immutable visual commands emitted by the latest update. */
	const TArray<FAEVegetationPatchVisualCommand>& GetVisualCommands() const { return VisualCommands; }
	/* Returns the latest global M7 Patch-state revision. */
	uint64 GetPatchStateRevision() const { return PatchStateRevision; }
	/* Returns rejected registration or input count. */
	uint64 GetRejectedInputCount() const { return RejectedInputCount; }

private:
	/* Stores one registered Patch and its committed state. */
	struct FPatchRecord
	{
		FAEVegetationPatchRegistration Registration;
		FAEVegetationPatchState State;
		bool bInitialized = false;
	};

	/* Validates one complete registration without mutating state. */
	bool ValidateRegistration(const FAEVegetationPatchRegistration& Registration) const;
	/* Calculates M5-derived target health for one Patch. */
	static double CalculateTargetHealth(double DamageRatio, const FAEM7SpeciesParameters& Parameters);
	/* Derives density from current health without storing redundant state. */
	static double CalculateDensity(double HealthRatio, const FAEM7SpeciesParameters& Parameters);

	int32 CellCount = 0;
	TMap<FGuid, FPatchRecord> Patches;
	TArray<TArray<FGuid>> PatchIdsByCellIndex;
	TSet<FGuid> ActiveTransitionPatchIds;
	TArray<FAEVegetationPatchVisualCommand> VisualCommands;
	uint64 PatchStateRevision = 0;
	uint64 RejectedInputCount = 0;
};
