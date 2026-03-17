#include "TagKeyModule.h"

#include "TagKeyLog.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FTagKeyModule, TagKey)

namespace
{
	static bool ApplyModuleLogVerbosity(const TCHAR* CommandName, const TCHAR* CategoryName, const TArray<FString>& Args)
	{
		FString TargetVerbosity = TEXT("veryverbose");
		const FString LevelArg = (Args.Num() > 0 ? Args[0] : FString()).TrimStartAndEnd().ToLower();
		if (!LevelArg.IsEmpty())
		{
			if (LevelArg == TEXT("veryverbose") || LevelArg == TEXT("vv"))
			{
				TargetVerbosity = TEXT("veryverbose");
			}
			else if (LevelArg == TEXT("verbose") || LevelArg == TEXT("v"))
			{
				TargetVerbosity = TEXT("verbose");
			}
			else if (LevelArg == TEXT("log") || LevelArg == TEXT("l") || LevelArg == TEXT("default"))
			{
				TargetVerbosity = TEXT("log");
			}
			else if (LevelArg == TEXT("warning") || LevelArg == TEXT("warn") || LevelArg == TEXT("w"))
			{
				TargetVerbosity = TEXT("warning");
			}
			else if (LevelArg == TEXT("error") || LevelArg == TEXT("e"))
			{
				TargetVerbosity = TEXT("error");
			}
			else if (LevelArg == TEXT("off") || LevelArg == TEXT("none"))
			{
				TargetVerbosity = TEXT("off");
			}
			else if (LevelArg == TEXT("reset"))
			{
				TargetVerbosity = TEXT("log");
			}
			else
			{
				return false;
			}
		}

		if (!GEngine)
		{
			return false;
		}

		UWorld* ExecWorld = nullptr;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.World() && (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game))
			{
				ExecWorld = Context.World();
				break;
			}
		}

		if (!ExecWorld)
		{
			UE_LOG(
				LogTagKey,
				Warning,
				TEXT("[TagKey] %s is only available when a PIE/Game world context exists. Unable to apply verbosity '%s' to %s."),
				CommandName,
				*TargetVerbosity,
				CategoryName);
			return false;
		}

		const FString ExecCommand = FString::Printf(TEXT("log %s %s"), CategoryName, *TargetVerbosity);
		const bool bApplied = GEngine->Exec(ExecWorld, *ExecCommand);
		if (!bApplied)
		{
			UE_LOG(LogTagKey, Warning, TEXT("[TagKey] %s could not apply verbosity '%s' to %s."), CommandName, *TargetVerbosity, CategoryName);
			return false;
		}

		UE_LOG(LogTagKey, Log, TEXT("[TagKey] %s set %s verbosity to '%s'."), CommandName, CategoryName, *TargetVerbosity);
		return true;
	}
}

void FTagKeyModule::StartupModule()
{
	IConsoleManager& ConsoleManager = IConsoleManager::Get();
	CmdTagKeyDebug = ConsoleManager.RegisterConsoleCommand(
		TEXT("tagkey.debug.log"),
		TEXT("Usage: tagkey.debug.log [veryverbose|verbose|log|warning|error|off|reset]. Defaults to veryverbose and applies to TagKeyLog."),
		FConsoleCommandWithArgsDelegate::CreateRaw(this, &FTagKeyModule::HandleConsoleTagKeyDebug),
		ECVF_Default);
}

void FTagKeyModule::ShutdownModule()
{
	IConsoleManager::Get().UnregisterConsoleObject(TEXT("tagkey.debug.log"), false);
	CmdTagKeyDebug = nullptr;
}

void FTagKeyModule::HandleConsoleTagKeyDebug(const TArray<FString>& Args)
{
	if (!ApplyModuleLogVerbosity(TEXT("tagkey.debug.log"), TEXT("logtagkey"), Args))
	{
		UE_LOG(LogTagKey, Log, TEXT("[TagKey] Usage: tagkey.debug.log [veryverbose|verbose|log|warning|error|off|reset]"));
	}
}
