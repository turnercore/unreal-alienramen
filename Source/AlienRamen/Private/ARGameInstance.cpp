#include "ARGameInstance.h"

#include "ARLog.h"
#include "ARSaveGame.h"
#include "ARSaveSubsystem.h"
#include "ARSessionSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "Engine/Engine.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "OnlineSessionSettings.h"

void UARGameInstance::Init()
{
	Super::Init();
	RegisterDebugConsoleCommands();
	UE_LOG(ARLog, Log, TEXT("[GameInstance] Initialized: %s"), *GetNameSafe(this));
	BP_OnARGameInstanceInitialized();
}

void UARGameInstance::Shutdown()
{
	UnregisterDebugConsoleCommands();
	UE_LOG(ARLog, Log, TEXT("[GameInstance] Shutdown: %s"), *GetNameSafe(this));
	BP_OnARGameInstanceShutdown();
	Super::Shutdown();
}

UARSaveSubsystem* UARGameInstance::GetARSaveSubsystem() const
{
	return GetSubsystem<UARSaveSubsystem>();
}

UARSessionSubsystem* UARGameInstance::GetARSessionSubsystem() const
{
	return GetSubsystem<UARSessionSubsystem>();
}

// ---- Network compatibility helpers ----

namespace
{
	// Session setting key advertised so clients can pre-filter incompatible sessions.
	static const FName SessionSetting_ARProtocol(TEXT("ARProtocol"));
}

bool UARGameInstance::IsARProtocolCompatible(const int32 OtherProtocol)
{
	return OtherProtocol >= ARMinCompatibleProtocol && OtherProtocol <= ARProtocolVersion;
}

void UARGameInstance::ApplyARProtocolSessionSetting(FOnlineSessionSettings& SessionSettings)
{
	SessionSettings.Set(SessionSetting_ARProtocol, ARProtocolVersion, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
}

bool UARGameInstance::GetARProtocolFromSession(const FOnlineSessionSearchResult& SearchResult, int32& OutProtocol)
{
	OutProtocol = 0;

	int32 ProtocolValue = 0;
	if (SearchResult.Session.SessionSettings.Get(SessionSetting_ARProtocol, ProtocolValue))
	{
		OutProtocol = ProtocolValue;
		return true;
	}

	return false;
}

// ---- Build/version helpers ----

FString UARGameInstance::GetARProjectVersion()
{
	FString ProjectVersion;
	if (GConfig)
	{
		GConfig->GetString(
			TEXT("/Script/EngineSettings.GeneralProjectSettings"),
			TEXT("ProjectVersion"),
			ProjectVersion,
			GGameIni);
	}

	return ProjectVersion;
}

FString UARGameInstance::GetARBuildFingerprint()
{
	const FString ProjectVersion = GetARProjectVersion();
	return FString::Printf(TEXT("%s | NetProto:%d | SaveSchema:%d"),
		*ProjectVersion,
		ARProtocolVersion,
		UARSaveGame::GetCurrentSchemaVersion());
}

void UARGameInstance::BP_OnARGameInstanceInitialized_Implementation()
{
}

void UARGameInstance::BP_OnARGameInstanceShutdown_Implementation()
{
}

void UARGameInstance::RegisterDebugConsoleCommands()
{
	UnregisterDebugConsoleCommands();

	IConsoleManager& ConsoleManager = IConsoleManager::Get();
	CmdArDebug = ConsoleManager.RegisterConsoleCommand(
		TEXT("ar.debug.log"),
		TEXT("Usage: ar.debug.log <veryverbose|verbose|log|warning|error|off|reset>. Applies to ARLog."),
		FConsoleCommandWithArgsDelegate::CreateUObject(this, &UARGameInstance::HandleConsoleArDebug),
		ECVF_Default);
}

void UARGameInstance::UnregisterDebugConsoleCommands()
{
	IConsoleManager& ConsoleManager = IConsoleManager::Get();
	ConsoleManager.UnregisterConsoleObject(TEXT("ar.debug.log"), false);
	CmdArDebug = nullptr;
}

void UARGameInstance::HandleConsoleArDebug(const TArray<FString>& Args)
{
	const FString LevelArg = (Args.Num() > 0 ? Args[0] : FString()).TrimStartAndEnd().ToLower();
	FString TargetVerbosity;
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
		UE_LOG(ARLog, Log, TEXT("[Debug] Usage: ar.debug.log <veryverbose|verbose|log|warning|error|off|reset>"));
		return;
	}

	UWorld* World = GetWorld();
	bool bApplied = false;
	if (GEngine && World)
	{
		const FString ExecCommand = FString::Printf(TEXT("log arlog %s"), *TargetVerbosity);
		bApplied = GEngine->Exec(World, *ExecCommand);
	}

	if (bApplied)
	{
		UE_LOG(ARLog, Log, TEXT("[Debug] ARLog verbosity set to '%s' via ar.debug.log."), *TargetVerbosity);
	}
	else
	{
		UE_LOG(ARLog, Warning, TEXT("[Debug] ar.debug.log could not apply verbosity '%s' (no active world/console context)."), *TargetVerbosity);
	}
}
