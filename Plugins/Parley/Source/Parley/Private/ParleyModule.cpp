#include "ParleyModule.h"
#include "ParleyLog.h"
#include "Engine/Engine.h"
#include "GameplayTagsManager.h"
#include "HAL/IConsoleManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FParleyModule, Parley)

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

		const FString ExecCommand = FString::Printf(TEXT("log %s %s"), CategoryName, *TargetVerbosity);
		const bool bApplied = GEngine->Exec(ExecWorld, *ExecCommand);
		if (!bApplied)
		{
			UE_LOG(ParleyLog, Warning, TEXT("[Parley] %s could not apply verbosity '%s' to %s."), CommandName, *TargetVerbosity, CategoryName);
			return false;
		}

		UE_LOG(ParleyLog, Log, TEXT("[Parley] %s set %s verbosity to '%s'."), CommandName, CategoryName, *TargetVerbosity);
		return true;
	}
}

void FParleyModule::StartupModule()
{
	IConsoleManager& ConsoleManager = IConsoleManager::Get();
	CmdParleyDebug = ConsoleManager.RegisterConsoleCommand(
		TEXT("parley.debug.log"),
		TEXT("Usage: parley.debug.log [veryverbose|verbose|log|warning|error|off|reset]. Defaults to veryverbose and applies to ParleyLog."),
		FConsoleCommandWithArgsDelegate::CreateRaw(this, &FParleyModule::HandleConsoleParleyDebug),
		ECVF_Default);

	if (TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("Parley")))
	{
		const FString TagsSearchPath = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Config"), TEXT("Tags"));
		UGameplayTagsManager::Get().AddTagIniSearchPath(TagsSearchPath);
		UE_LOG(ParleyLog, Verbose, TEXT("[Parley] Registered gameplay-tag config path: %s"), *TagsSearchPath);
	}

	static const TCHAR* RequiredTags[] =
	{
		TEXT("Parley.Speaker"),
		TEXT("Parley.Speaker.Requester"),
		TEXT("Parley.Speaker.Owner"),
		TEXT("Parley.Speaker.Brother"),
		TEXT("Parley.Speaker.Sister"),
		TEXT("Parley.Conversations"),
		TEXT("Parley.Emotion"),
		TEXT("Parley.Emotion.Busy"),
		TEXT("Parley.Emotion.Default"),
		TEXT("Parley.AudioCue"),
		TEXT("Parley.Factions"),
		TEXT("Parley.Factions.Effect")
	};

	for (const TCHAR* RequiredTag : RequiredTags)
	{
		if (!UGameplayTagsManager::Get().RequestGameplayTag(FName(RequiredTag), false).IsValid())
		{
			UE_LOG(
				ParleyLog,
				Warning,
				TEXT("[Parley] Required gameplay tag '%s' is missing. Add it to project/plugin gameplay-tag config."),
				RequiredTag);
		}
	}
}

void FParleyModule::ShutdownModule()
{
	IConsoleManager::Get().UnregisterConsoleObject(TEXT("parley.debug.log"), false);
	CmdParleyDebug = nullptr;
}

void FParleyModule::HandleConsoleParleyDebug(const TArray<FString>& Args)
{
	if (!ApplyModuleLogVerbosity(TEXT("parley.debug.log"), TEXT("parleylog"), Args))
	{
		UE_LOG(ParleyLog, Log, TEXT("[Parley] Usage: parley.debug.log [veryverbose|verbose|log|warning|error|off|reset]"));
	}
}
