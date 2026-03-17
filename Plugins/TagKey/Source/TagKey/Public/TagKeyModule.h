#pragma once

#include "Modules/ModuleInterface.h"

class FTagKeyModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	class IConsoleObject* CmdTagKeyDebug = nullptr;
	void HandleConsoleTagKeyDebug(const TArray<FString>& Args);
};
