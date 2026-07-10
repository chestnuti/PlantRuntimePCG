#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AdaptiveEnvWorldSubsystem.generated.h"

UCLASS()
class ADAPTIVEENVRUNTIME_API UAEAdaptiveEnvWorldSubsystem final : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;

	UFUNCTION(BlueprintPure, Category = "Adaptive Environment")
	FGuid GetInstanceId() const { return InstanceId; }

	UFUNCTION(BlueprintPure, Category = "Adaptive Environment")
	int64 GetTickCount() const { return TickCount; }

protected:
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

private:
	FGuid InstanceId;
	int64 TickCount = 0;
	bool bRuntimeEnabled = false;
};
