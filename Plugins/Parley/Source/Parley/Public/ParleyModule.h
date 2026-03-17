#pragma once

#include "Modules/ModuleInterface.h"

class FParleyModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	class IConsoleObject* CmdParleyDebug = nullptr;
	void HandleConsoleParleyDebug(const TArray<FString>& Args);
};
