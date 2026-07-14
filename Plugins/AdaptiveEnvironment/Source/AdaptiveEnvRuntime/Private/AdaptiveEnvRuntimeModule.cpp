#include "AdaptiveEnvRuntimeModule.h"

#include "AdaptiveEnvLog.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogAdaptiveEnv);

// Announce that the runtime module is ready for World initialization.
void FAdaptiveEnvRuntimeModule::StartupModule()
{
	UE_LOG(LogAdaptiveEnv, Log, TEXT("AdaptiveEnvRuntime module started."));
}

// Announce module shutdown after all Worlds have released their subsystems.
void FAdaptiveEnvRuntimeModule::ShutdownModule()
{
	UE_LOG(LogAdaptiveEnv, Log, TEXT("AdaptiveEnvRuntime module stopped."));
}

// Register the plugin runtime module with Unreal Engine.
IMPLEMENT_MODULE(FAdaptiveEnvRuntimeModule, AdaptiveEnvRuntime)
