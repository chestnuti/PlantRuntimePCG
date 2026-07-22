#include "AEMoistureSourceComponent.h"

#include "AdaptiveEnvWorldSubsystem.h"
#include "Engine/World.h"

/* Disable component Tick because the World Subsystem owns sampling order. */
UAEMoistureSourceComponent::UAEMoistureSourceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

/* Register this source with the current playable World. */
void UAEMoistureSourceComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!SourceId.IsValid())
	{
		SourceId = FGuid::NewGuid();
	}
	if (UWorld* World = GetWorld())
	{
		if (UAEAdaptiveEnvWorldSubsystem* Subsystem = World->GetSubsystem<UAEAdaptiveEnvWorldSubsystem>())
		{
			Subsystem->RegisterMoistureSource(this);
		}
	}
}

/* Remove this source before component teardown. */
void UAEMoistureSourceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UAEAdaptiveEnvWorldSubsystem* Subsystem = World->GetSubsystem<UAEAdaptiveEnvWorldSubsystem>())
		{
			Subsystem->UnregisterMoistureSource(this);
		}
	}
	Super::EndPlay(EndPlayReason);
}

/* Test one world point against the validated absolute box extents. */
bool UAEMoistureSourceComponent::ContainsWorldPoint(const FVector& WorldPoint) const
{
	// Use an axis-aligned world box so sampling remains deterministic under source rotation.
	return FBox::BuildAABB(GetComponentLocation(), BoxExtentCm.GetAbs()).IsInsideOrOn(WorldPoint);
}
