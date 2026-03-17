#include "ARGameInstance.h"

#include "ARFactionVotingSubsystem.h"
#include "ARLog.h"
#include "ARParleySaveBridge.h"
#include "ARSaveGame.h"
#include "ARSaveSubsystem.h"
#include "ARSessionSubsystem.h"
#include "ARTravelSubsystem.h"
#include "ParleyDialogueSubsystem.h"
#include "ParleyFactionSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "Engine/Engine.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "OnlineSessionSettings.h"

void UARGameInstance::Init()
{
	Super::Init();
	RegisterDebugConsoleCommands();

	if (!ParleySaveBridge)
	{
		ParleySaveBridge = NewObject<UARParleySaveBridge>(this);
	}

	if (ParleySaveBridge)
	{
		ParleySaveBridge->Initialize(
			GetSubsystem<UARSaveSubsystem>(),
			GetSubsystem<UParleyDialogueSubsystem>(),
			GetSubsystem<UParleyFactionSubsystem>());
	}

	UE_LOG(ARLog, Log, TEXT("[GameInstance] Initialized: %s"), *GetNameSafe(this));
	BP_OnARGameInstanceInitialized();
}

void UARGameInstance::Shutdown()
{
	if (ParleySaveBridge)
	{
		ParleySaveBridge->Shutdown();
	}
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

UARTravelSubsystem* UARGameInstance::GetARTravelSubsystem() const
{
	return GetSubsystem<UARTravelSubsystem>();
}

UARFactionVotingSubsystem* UARGameInstance::GetARFactionVotingSubsystem() const
{
	return GetSubsystem<UARFactionVotingSubsystem>();
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
	CmdArDebugAll = ConsoleManager.RegisterConsoleCommand(
		TEXT("ar.debug.log.all"),
		TEXT("Usage: ar.debug.log.all <veryverbose|verbose|log|warning|error|off|reset>. Applies to ARLog, EmoLog, ParleyLog, and TagKeyLog."),
		FConsoleCommandWithArgsDelegate::CreateUObject(this, &UARGameInstance::HandleConsoleArDebugAll),
		ECVF_Default);
}

void UARGameInstance::UnregisterDebugConsoleCommands()
{
	IConsoleManager& ConsoleManager = IConsoleManager::Get();
	ConsoleManager.UnregisterConsoleObject(TEXT("ar.debug.log"), false);
	ConsoleManager.UnregisterConsoleObject(TEXT("ar.debug.log.all"), false);
	CmdArDebug = nullptr;
	CmdArDebugAll = nullptr;
}

bool UARGameInstance::TryResolveDebugVerbosityArg(const TArray<FString>& Args, FString& OutTargetVerbosity) const
{
	const FString LevelArg = (Args.Num() > 0 ? Args[0] : FString()).TrimStartAndEnd().ToLower();
	if (LevelArg == TEXT("veryverbose") || LevelArg == TEXT("vv"))
	{
		OutTargetVerbosity = TEXT("veryverbose");
	}
	else if (LevelArg == TEXT("verbose") || LevelArg == TEXT("v"))
	{
		OutTargetVerbosity = TEXT("verbose");
	}
	else if (LevelArg == TEXT("log") || LevelArg == TEXT("l") || LevelArg == TEXT("default"))
	{
		OutTargetVerbosity = TEXT("log");
	}
	else if (LevelArg == TEXT("warning") || LevelArg == TEXT("warn") || LevelArg == TEXT("w"))
	{
		OutTargetVerbosity = TEXT("warning");
	}
	else if (LevelArg == TEXT("error") || LevelArg == TEXT("e"))
	{
		OutTargetVerbosity = TEXT("error");
	}
	else if (LevelArg == TEXT("off") || LevelArg == TEXT("none"))
	{
		OutTargetVerbosity = TEXT("off");
	}
	else if (LevelArg == TEXT("reset"))
	{
		OutTargetVerbosity = TEXT("log");
	}
	else
	{
		return false;
	}

	return true;
}

void UARGameInstance::HandleConsoleArDebug(const TArray<FString>& Args)
{
	FString TargetVerbosity;
	if (!TryResolveDebugVerbosityArg(Args, TargetVerbosity))
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

void UARGameInstance::HandleConsoleArDebugAll(const TArray<FString>& Args)
{
	FString TargetVerbosity;
	if (!TryResolveDebugVerbosityArg(Args, TargetVerbosity))
	{
		UE_LOG(ARLog, Log, TEXT("[Debug] Usage: ar.debug.log.all <veryverbose|verbose|log|warning|error|off|reset>"));
		return;
	}

	UWorld* World = GetWorld();
	if (!GEngine || !World)
	{
		UE_LOG(ARLog, Warning, TEXT("[Debug] ar.debug.log.all could not apply verbosity '%s' (no active world/console context)."), *TargetVerbosity);
		return;
	}

	const TCHAR* Categories[] =
	{
		TEXT("arlog"),
		TEXT("emolog"),
		TEXT("parleylog"),
		TEXT("logtagkey")
	};

	bool bAppliedAll = true;
	for (const TCHAR* CategoryName : Categories)
	{
		const FString ExecCommand = FString::Printf(TEXT("log %s %s"), CategoryName, *TargetVerbosity);
		bAppliedAll &= GEngine->Exec(World, *ExecCommand);
	}

	if (bAppliedAll)
	{
		UE_LOG(ARLog, Log, TEXT("[Debug] ar.debug.log.all set ARLog, EmoLog, ParleyLog, and TagKeyLog to '%s'."), *TargetVerbosity);
	}
	else
	{
		UE_LOG(ARLog, Warning, TEXT("[Debug] ar.debug.log.all only partially applied verbosity '%s'."), *TargetVerbosity);
	}
}
