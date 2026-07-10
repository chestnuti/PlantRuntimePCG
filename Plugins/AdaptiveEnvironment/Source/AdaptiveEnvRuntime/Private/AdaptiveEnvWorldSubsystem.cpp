#include "AdaptiveEnvWorldSubsystem.h"

#include "AdaptiveEnvLog.h"
#include "AdaptiveEnvSettings.h"

void UAEAdaptiveEnvWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	InstanceId = FGuid::NewGuid();
	TickCount = 0;
	bRuntimeEnabled = GetDefault<UAdaptiveEnvSettings>()->bEnableRuntime;

	UE_LOG(
		LogAdaptiveEnv,
		Log,
		TEXT("World subsystem initialized. World=%s Instance=%s"),
		*GetNameSafe(GetWorld()),
		*InstanceId.ToString(EGuidFormats::DigitsWithHyphens));
}

void UAEAdaptiveEnvWorldSubsystem::Deinitialize()
{
	UE_LOG(
		LogAdaptiveEnv,
		Log,
		TEXT("World subsystem deinitialized. World=%s Instance=%s Ticks=%lld"),
		*GetNameSafe(GetWorld()),
		*InstanceId.ToString(EGuidFormats::DigitsWithHyphens),
		TickCount);

	bRuntimeEnabled = false;
	Super::Deinitialize();
}

void UAEAdaptiveEnvWorldSubsystem::Tick(float DeltaTime)
{
	++TickCount;

	// M0 keeps the ordered runtime pipeline empty.
	(void)DeltaTime;
}

bool UAEAdaptiveEnvWorldSubsystem::IsTickable() const
{
	return bRuntimeEnabled && IsInitialized();
}

TStatId UAEAdaptiveEnvWorldSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UAEAdaptiveEnvWorldSubsystem, STATGROUP_Tickables);
}

bool UAEAdaptiveEnvWorldSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game
		|| WorldType == EWorldType::PIE
		|| WorldType == EWorldType::GamePreview;
}
