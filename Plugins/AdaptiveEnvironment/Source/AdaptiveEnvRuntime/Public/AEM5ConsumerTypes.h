#pragma once

#include "CoreMinimal.h"

/* Freezes the minimal committed M5 state shared by downstream M6 and M7 consumers. */
struct ADAPTIVEENVRUNTIME_API FAEM5ConsumerCellView
{
	/* Stores the row-major Cell index shared by every runtime Grid. */
	int32 CellIndex = INDEX_NONE;
	/* Stores the committed cumulative M5 Damage state in the zero-to-one range. */
	double DamageRatio = 0.0;
	/* Identifies the committed M5 response revision consumed by downstream stages. */
	uint64 ResponseRevision = 0;
	/* Identifies the M5 fixed step that produced the committed response. */
	uint64 ResponseSimulationStep = 0;
};
