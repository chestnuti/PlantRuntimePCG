#pragma once

#include "Modules/ModuleManager.h"

/* Manages startup and shutdown for the AdaptiveEnvRuntime module. */
class FAdaptiveEnvRuntimeModule final : public IModuleInterface
{
public:
	/* Initializes module-level runtime services. */
	virtual void StartupModule() override;

	/* Releases module-level runtime services. */
	virtual void ShutdownModule() override;
};
