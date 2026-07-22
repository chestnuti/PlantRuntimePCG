#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "AEMoistureSourceComponent.generated.h"

class UAEAdaptiveEnvWorldSubsystem;

/* Publishes one deterministic box-shaped moisture source to the World constraint sampler. */
UCLASS(ClassGroup = (AdaptiveEnvironment), meta = (BlueprintSpawnableComponent))
class ADAPTIVEENVRUNTIME_API UAEMoistureSourceComponent final : public USceneComponent
{
	GENERATED_BODY()

public:
	/* Creates a non-ticking moisture source. */
	UAEMoistureSourceComponent();
	/* Registers the source at the next safe World pipeline boundary. */
	virtual void BeginPlay() override;
	/* Unregisters the source before its owner leaves play. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/* Returns whether a world point lies inside this source's axis-aligned box. */
	bool ContainsWorldPoint(const FVector& WorldPoint) const;

	/* Stores the normalized moisture supplied inside the source volume. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Adaptive Environment|M4", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MoistureRatio = 0.5f;
	/* Selects the winning source when multiple volumes overlap; larger values win. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Adaptive Environment|M4")
	int32 Priority = 0;
	/* Provides a stable tie-break identity for overlapping sources. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Adaptive Environment|M4")
	FGuid SourceId;
	/* Defines axis-aligned source half extents in world centimetres. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Adaptive Environment|M4", meta = (ClampMin = "0.0"))
	FVector BoxExtentCm = FVector(500.0, 500.0, 500.0);
};
