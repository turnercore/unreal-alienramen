#include "Modules/ModuleManager.h"

class FAlienRamenTestsModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override {}
	virtual void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FAlienRamenTestsModule, AlienRamenTests)

