#include "Modules/ModuleManager.h"

/* Registers the editor-only parameter bundle import module. */
class FAdaptiveEnvEditorModule final : public IModuleInterface
{
public:
	/* Starts the stateless editor extension. */
	virtual void StartupModule() override {}
	/* Stops the stateless editor extension. */
	virtual void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FAdaptiveEnvEditorModule, AdaptiveEnvEditor)
