#include "EmoModule.h"
#include "EmoLog.h"
#include "Engine/Engine.h"
#include "GameplayTagsManager.h"
#include "Interfaces/IPluginManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FEmoModule, Emo)

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
				EmoLog,
				Warning,
				TEXT("[Emo] %s is only available when a PIE/Game world context exists. Unable to apply verbosity '%s' to %s."),
				CommandName,
				*TargetVerbosity,
				CategoryName);
			return false;
		}

		const FString ExecCommand = FString::Printf(TEXT("log %s %s"), CategoryName, *TargetVerbosity);
		const bool bApplied = GEngine->Exec(ExecWorld, *ExecCommand);
		if (!bApplied)
		{
			UE_LOG(EmoLog, Warning, TEXT("[Emo] %s could not apply verbosity '%s' to %s."), CommandName, *TargetVerbosity, CategoryName);
			return false;
		}

		UE_LOG(EmoLog, Log, TEXT("[Emo] %s set %s verbosity to '%s'."), CommandName, CategoryName, *TargetVerbosity);
		return true;
	}
}

void FEmoModule::StartupModule()
{
	IConsoleManager& ConsoleManager = IConsoleManager::Get();
	CmdEmoDebug = ConsoleManager.RegisterConsoleCommand(
		TEXT("emo.debug.log"),
		TEXT("Usage: emo.debug.log [veryverbose|verbose|log|warning|error|off|reset]. Defaults to veryverbose and applies to EmoLog."),
		FConsoleCommandWithArgsDelegate::CreateRaw(this, &FEmoModule::HandleConsoleEmoDebug),
		ECVF_Default);

	if (TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("Emo")))
	{
		const FString TagsSearchPath = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Config"), TEXT("Tags"));
		UGameplayTagsManager::Get().AddTagIniSearchPath(TagsSearchPath);
		UE_LOG(EmoLog, Verbose, TEXT("[Emo] Registered gameplay-tag config path: %s"), *TagsSearchPath);
	}

	static const TCHAR* RequiredTags[] =
	{
		TEXT("Parley.Emotion"),
		TEXT("Parley.Emotion.Busy"),
		TEXT("Parley.Emotion.WantsToTalk"),
		TEXT("Parley.Emotion.Preview")
	};

	for (const TCHAR* RequiredTag : RequiredTags)
	{
		if (!UGameplayTagsManager::Get().RequestGameplayTag(FName(RequiredTag), false).IsValid())
		{
			UE_LOG(
				EmoLog,
				Warning,
				TEXT("[Emo] Required gameplay tag '%s' is missing. Add it to project/plugin gameplay-tag config."),
				RequiredTag);
		}
	}
}

void FEmoModule::ShutdownModule()
{
	IConsoleManager::Get().UnregisterConsoleObject(TEXT("emo.debug.log"), false);
	CmdEmoDebug = nullptr;
}

void FEmoModule::HandleConsoleEmoDebug(const TArray<FString>& Args)
{
	if (!ApplyModuleLogVerbosity(TEXT("emo.debug.log"), TEXT("emolog"), Args))
	{
		UE_LOG(EmoLog, Log, TEXT("[Emo] Usage: emo.debug.log [veryverbose|verbose|log|warning|error|off|reset]"));
	}
}
