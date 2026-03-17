#pragma once

#include "Modules/ModuleInterface.h"

class FEmoModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	class IConsoleObject* CmdEmoDebug = nullptr;
	void HandleConsoleEmoDebug(const TArray<FString>& Args);
};
