#pragma once

#include "CoreMinimal.h"
#include "AEM3Types.h"
#include "AdaptiveEnvTypes.h"

class FAEHeatmapGrid;

/* Owns deterministic per-Cell Exposure and first-version ecological response state. */
class ADAPTIVEENVRUNTIME_API FAEExposureGrid
{
public:
	/* Allocates M3 storage aligned exactly with one validated M1 Grid configuration. */
	bool Initialize(const FAEHeatmapGridConfig& InConfig);
	/* Clears all M3 Cell values, active tracking, and revisions while preserving configuration. */
	void Reset();

	/* Advances dirty raw deltas, Exposure decay, Damage, and Recovery for one fixed simulated step. */
	bool Update(
		const FAEHeatmapGrid& BehaviourGrid,
		const TArray<int32>& DirtyBehaviourCellIndices,
		double SimulationTimeHours,
		double DeltaSimulationHours,
		uint64 BehaviourRevision,
		const FAEM3ParameterSet& Parameters);

	/* Reads one immutable M3 Cell snapshot by integer XY coordinate. */
	bool GetCellSnapshot(const FIntPoint& Coordinate, FAEM3CellSnapshot& OutSnapshot) const;
	/* Reads one immutable M3 Cell snapshot by world position in centimetres. */
	bool GetCellSnapshotAtWorldLocation(const FVector& WorldLocation, FAEM3CellSnapshot& OutSnapshot) const;
	/* Returns the latest global Exposure revision. */
	uint64 GetExposureRevision() const { return ExposureRevision; }
	/* Returns the latest global ecological response revision. */
	uint64 GetResponseRevision() const { return ResponseRevision; }
	/* Returns the number of Cells requiring continued decay or ecological evaluation. */
	int32 GetActiveCellCount() const { return ActiveCellCount; }

private:
	/* Stores mutable M3 state owned by one flat row-major Cell. */
	struct FCellState
	{
		/* Stores the last cumulative M1 totals consumed by this Cell. */
		FAERawBehaviourTotals LastConsumedRawTotals;
		/* Stores the latest normalized Pass contribution. */
		double PassExposure = 0.0;
		/* Stores the latest normalized non-sprint Travel contribution. */
		double TravelExposure = 0.0;
		/* Stores the latest normalized Dwell contribution. */
		double DwellExposure = 0.0;
		/* Stores the latest normalized Sprint contribution. */
		double SprintExposure = 0.0;
		/* Stores the latest normalized weighted collection-event contribution. */
		double CollectExposure = 0.0;
		/* Stores the latest normalized weighted combat-event contribution. */
		double CombatExposure = 0.0;
		/* Stores accumulated and decayed Exposure. */
		double CurrentExposure = 0.0;
		/* Stores continuous ecological Damage from zero to one. */
		double EcologicalDamageRatio = 0.0;
		/* Stores the current Damage rate per simulated hour. */
		double DamageRatePerSimulationHour = 0.0;
		/* Stores the current Recovery rate per simulated hour. */
		double RecoveryRatePerSimulationHour = 0.0;
		/* Stores continuous time below the Recovery threshold in simulated hours. */
		double LowExposureDurationSimulationHours = 0.0;
		/* Stores the latest M1 revision consumed by this Cell. */
		uint64 SourceBehaviourRevision = 0;
		/* Stores the latest global Exposure revision affecting this Cell. */
		uint64 ExposureRevision = 0;
		/* Stores the latest global response revision affecting this Cell. */
		uint64 ResponseRevision = 0;
	};

	/* Maps a valid XY Cell coordinate to a flat row-major index. */
	bool CellToIndex(const FIntPoint& Coordinate, int32& OutIndex) const;
	/* Maps a half-open world XY position to a validated Cell coordinate. */
	bool WorldToCell(const FVector& WorldLocation, FIntPoint& OutCoordinate) const;
	/* Returns the world-space centre of a valid Cell at ground Z. */
	FVector GetCellWorldCenter(const FIntPoint& Coordinate) const;
	/* Converts one M1 snapshot into cumulative raw totals. */
	static FAERawBehaviourTotals MakeRawTotals(const FAEBehaviourCellSnapshot& Snapshot);
	/* Returns true when any cumulative raw total moved backwards and requires a baseline reset. */
	static bool HasRawRegression(const FAERawBehaviourTotals& Current, const FAERawBehaviourTotals& Previous);
	/* Calculates first-version piecewise-linear Damage rate. */
	static double EvaluateDamageRate(double Exposure, const FAEM3ParameterSet& Parameters);
	/* Builds a public immutable snapshot from one internal Cell. */
	FAEM3CellSnapshot MakeSnapshot(const FIntPoint& Coordinate, int32 Index) const;

	/* Stores the validated M1-aligned Grid configuration. */
	FAEHeatmapGridConfig Config;
	/* Stores the lower-left Grid origin in world XY centimetres. */
	FVector2D WorldMin = FVector2D::ZeroVector;
	/* Stores all M3 Cells in deterministic row-major order. */
	TArray<FCellState> Cells;
	/* Marks Cells that must be evaluated during the next fixed step. */
	TBitArray<> ActiveFlags;
	/* Marks Cells included in the current stable processing set. */
	TBitArray<> ProcessingFlags;
	/* Counts currently active Cells without scanning storage. */
	int32 ActiveCellCount = 0;
	/* Increments once for each fixed step that changes any Exposure state. */
	uint64 ExposureRevision = 0;
	/* Increments once for each fixed step that changes any ecological response state. */
	uint64 ResponseRevision = 0;
};
