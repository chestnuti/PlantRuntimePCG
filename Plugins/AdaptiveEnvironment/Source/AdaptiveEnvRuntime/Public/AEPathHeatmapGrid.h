#pragma once

#include "CoreMinimal.h"
#include "AEM6Types.h"
#include "AdaptiveEnvTypes.h"

/* Owns deterministic per-Cell M6 path visual state aligned with M1-M5. */
class ADAPTIVEENVRUNTIME_API FAEPathHeatmapGrid
{
public:
	/* Allocates M6 storage aligned with the shared runtime Grid. */
	bool Initialize(const FAEHeatmapGridConfig& InConfig);
	/* Clears Cell states, active transitions, revisions, and commands. */
	void Reset();
	/* Advances M6 state from committed M5 inputs in row-major order. */
	bool Update(const TArray<FAEM6InputSnapshot>& Inputs, double DeltaSimulationHours, const FAEM6ParameterSet& Parameters);
	/* Builds the stable union of changed M5 Cells and active M6 transitions. */
	void BuildCandidateIndices(const TArray<int32>& M5ChangedIndices, TArray<int32>& OutIndices) const;
	/* Reads one committed M6 Cell by integer coordinate. */
	bool GetCellSnapshot(const FIntPoint& Coordinate, FAEPathHeatmapSnapshot& OutSnapshot) const;
	/* Reads one committed M6 Cell by half-open world XY location. */
	bool GetCellSnapshotAtWorldLocation(const FVector& Location, FAEPathHeatmapSnapshot& OutSnapshot) const;
	/* Builds a full row-major texture rebuild from initialized Cells. */
	void BuildFullVisualCommands(TArray<FAEPathHeatmapVisualCommand>& OutCommands) const;
	/* Returns Cells changed during the latest successful M6 update. */
	const TArray<int32>& GetLastChangedCellIndices() const { return LastChangedCellIndices; }
	/* Returns immutable visual commands produced by the latest update. */
	const TArray<FAEPathHeatmapVisualCommand>& GetVisualCommands() const { return VisualCommands; }
	/* Returns the latest global M6 state revision. */
	uint64 GetPathVisualRevision() const { return PathVisualRevision; }
	/* Returns rejected malformed or regressing input count. */
	uint64 GetRejectedInputCount() const { return RejectedInputCount; }

private:
	/* Stores one complete M6 Cell state and initialization marker. */
	struct FCell
	{
		FAEPathHeatmapCellState State;
		bool bInitialized = false;
	};

	/* Maps a valid coordinate to row-major storage. */
	bool CellToIndex(const FIntPoint& Coordinate, int32& OutIndex) const;
	/* Maps one half-open world XY position to a coordinate. */
	bool WorldToCell(const FVector& Location, FIntPoint& OutCoordinate) const;
	/* Calculates one configured Cell centre in world centimetres. */
	FVector GetCellWorldCenter(const FIntPoint& Coordinate) const;
	/* Validates one complete M6 parameter set. */
	static bool ValidateParameters(const FAEM6ParameterSet& Parameters);
	/* Maps M5 Damage into a bounded M6 target intensity. */
	static double CalculateTargetIntensity(double DamageRatio, const FAEM6ParameterSet& Parameters);

	FAEHeatmapGridConfig Config;
	FVector2D WorldMin = FVector2D::ZeroVector;
	TArray<FCell> Cells;
	TBitArray<> ActiveTransitionFlags;
	TArray<int32> LastChangedCellIndices;
	TArray<FAEPathHeatmapVisualCommand> VisualCommands;
	TArray<uint8> LastEncodedIntensities;
	uint64 PathVisualRevision = 0;
	uint64 RejectedInputCount = 0;
};
