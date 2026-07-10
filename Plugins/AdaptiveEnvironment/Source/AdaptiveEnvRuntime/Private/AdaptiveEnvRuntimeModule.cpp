#include "AdaptiveEnvRuntimeModule.h"

#include "AdaptiveEnvLog.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogAdaptiveEnv);

void FAdaptiveEnvRuntimeModule::StartupModule()
{
	UE_LOG(LogAdaptiveEnv, Log, TEXT("AdaptiveEnvRuntime module started."));
}

void FAdaptiveEnvRuntimeModule::ShutdownModule()
{
	UE_LOG(LogAdaptiveEnv, Log, TEXT("AdaptiveEnvRuntime module stopped."));
}

IMPLEMENT_MODULE(FAdaptiveEnvRuntimeModule, AdaptiveEnvRuntime)
