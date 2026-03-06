#include "ARSessionSubsystem.h"

#include "ARGameInstance.h"
#include "ARLog.h"
#include "ARNetworkUserSettings.h"
#include "ARPlayerStateBase.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Kismet/GameplayStatics.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemNames.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSessionSettings.h"

namespace
{
	static const FName GameSessionName = NAME_GameSession;

	static bool IsNullLikeSubsystemName(const FName InName)
	{
		if (InName == NAME_None)
		{
			return true;
		}

		const FString Name = InName.ToString();
		return Name.Equals(TEXT("NULL"), ESearchCase::IgnoreCase)
			|| Name.Equals(TEXT("INVALID"), ESearchCase::IgnoreCase)
			|| Name.Equals(TEXT("UNSET"), ESearchCase::IgnoreCase);
	}
}

void UARSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UARSessionSubsystem::Deinitialize()
{
	if (IOnlineSessionPtr Session = IOnlineSubsystem::Get() ? IOnlineSubsystem::Get()->GetSessionInterface() : nullptr)
	{
		if (CreateSessionCompleteHandle.IsValid())
		{
			Session->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteHandle);
		}
		if (UpdateSessionCompleteHandle.IsValid())
		{
			Session->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateSessionCompleteHandle);
		}
		if (DestroySessionCompleteHandle.IsValid())
		{
			Session->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteHandle);
		}
		if (FindSessionsCompleteHandle.IsValid())
		{
			Session->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteHandle);
		}
		if (JoinSessionCompleteHandle.IsValid())
		{
			Session->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteHandle);
		}
	}

	CachedNativeSearchResults.Reset();
	LastFindResults.Reset();
	ActiveSessionSearch.Reset();
	bOperationInFlight = false;
	CurrentOperation = ESessionOperation::None;
	Super::Deinitialize();
}

bool UARSessionSubsystem::IsStayOfflineEnabled() const
{
	if (const UARNetworkUserSettings* Settings = GetDefault<UARNetworkUserSettings>())
	{
		return Settings->bStayOffline;
	}
	return false;
}

void UARSessionSubsystem::SetStayOfflineEnabled(const bool bEnabled, bool& bOutRestartRecommended)
{
	bOutRestartRecommended = false;

	UARNetworkUserSettings* Settings = GetMutableDefault<UARNetworkUserSettings>();
	if (!Settings)
	{
		return;
	}

	const bool bChanged = Settings->bStayOffline != bEnabled;
	Settings->bStayOffline = bEnabled;
	Settings->SaveConfig();

	if (!bChanged)
	{
		return;
	}

	if (bEnabled)
	{
		DestroySessionBestEffort();
		bOutRestartRecommended = Settings->bShowRestartRecommendedHint;
	}
}

bool UARSessionSubsystem::EnsureSessionForCurrentFlow(const bool bPreferLAN, FARSessionResult& OutResult)
{
	if (IsStayOfflineEnabled())
	{
		FillResult(OutResult, false, EARSessionResultCode::OfflineBlocked, TEXT("Stay Offline is enabled."));
		OnEnsureSessionCompleted.Broadcast(OutResult);
		return false;
	}

	if (bOperationInFlight)
	{
		FillResult(OutResult, false, EARSessionResultCode::Busy, TEXT("A session operation is already in flight."));
		return false;
	}

	FName SubsystemName = NAME_None;
	IOnlineSessionPtr Session = ResolveSessionInterface(bPreferLAN, SubsystemName);
	if (!Session.IsValid())
	{
		FillResult(OutResult, false, EARSessionResultCode::NoSessionInterface, TEXT("No online session interface available."));
		OnEnsureSessionCompleted.Broadcast(OutResult);
		return false;
	}

	FOnlineSessionSettings DesiredSettings;
	if (!BuildDesiredSessionSettings(bPreferLAN, DesiredSettings, OutResult))
	{
		OnEnsureSessionCompleted.Broadcast(OutResult);
		return false;
	}

	const FNamedOnlineSession* ExistingSession = Session->GetNamedSession(GameSessionName);

	bOperationInFlight = true;
	CurrentOperation = ESessionOperation::Ensure;
	ActiveSubsystemName = SubsystemName;

	if (ExistingSession)
	{
		FOnUpdateSessionCompleteDelegate Delegate;
		Delegate.BindUObject(this, &UARSessionSubsystem::HandleUpdateSessionComplete);
		UpdateSessionCompleteHandle = Session->AddOnUpdateSessionCompleteDelegate_Handle(Delegate);
		if (!Session->UpdateSession(GameSessionName, DesiredSettings, true))
		{
			Session->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateSessionCompleteHandle);
			UpdateSessionCompleteHandle.Reset();
			bOperationInFlight = false;
			FillResult(OutResult, false, EARSessionResultCode::UpdateFailed, TEXT("UpdateSession failed to start."));
			OnEnsureSessionCompleted.Broadcast(OutResult);
			return false;
		}
	}
	else
	{
		FOnCreateSessionCompleteDelegate Delegate;
		Delegate.BindUObject(this, &UARSessionSubsystem::HandleCreateSessionComplete);
		CreateSessionCompleteHandle = Session->AddOnCreateSessionCompleteDelegate_Handle(Delegate);
		if (!Session->CreateSession(0, GameSessionName, DesiredSettings))
		{
			Session->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteHandle);
			CreateSessionCompleteHandle.Reset();
			bOperationInFlight = false;
			FillResult(OutResult, false, EARSessionResultCode::CreateFailed, TEXT("CreateSession failed to start."));
			OnEnsureSessionCompleted.Broadcast(OutResult);
			return false;
		}
	}

	FillResult(OutResult, true, EARSessionResultCode::Success);
	return true;
}

bool UARSessionSubsystem::FindSessions(const bool bLANQuery, const int32 MaxResults, FARSessionResult& OutResult)
{
	if (IsStayOfflineEnabled())
	{
		FillResult(OutResult, false, EARSessionResultCode::OfflineBlocked, TEXT("Stay Offline is enabled."));
		BroadcastFindCompleted(OutResult);
		return false;
	}

	if (bOperationInFlight)
	{
		FillResult(OutResult, false, EARSessionResultCode::Busy, TEXT("A session operation is already in flight."));
		return false;
	}

	FName SubsystemName = NAME_None;
	IOnlineSessionPtr Session = ResolveSessionInterface(bLANQuery, SubsystemName);
	if (!Session.IsValid())
	{
		FillResult(OutResult, false, EARSessionResultCode::NoSessionInterface, TEXT("No online session interface available."));
		BroadcastFindCompleted(OutResult);
		return false;
	}

	ActiveSessionSearch = MakeShared<FOnlineSessionSearch>();
	ActiveSessionSearch->bIsLanQuery = bLANQuery;
	ActiveSessionSearch->MaxSearchResults = FMath::Max(1, MaxResults);
	if (!bLANQuery)
	{
		ActiveSessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	}

	bOperationInFlight = true;
	CurrentOperation = ESessionOperation::Find;
	ActiveSubsystemName = SubsystemName;
	FOnFindSessionsCompleteDelegate Delegate;
	Delegate.BindUObject(this, &UARSessionSubsystem::HandleFindSessionsComplete);
	FindSessionsCompleteHandle = Session->AddOnFindSessionsCompleteDelegate_Handle(Delegate);

	if (!Session->FindSessions(0, ActiveSessionSearch.ToSharedRef()))
	{
		Session->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteHandle);
		FindSessionsCompleteHandle.Reset();
		bOperationInFlight = false;
		FillResult(OutResult, false, EARSessionResultCode::FindFailed, TEXT("FindSessions failed to start."));
		BroadcastFindCompleted(OutResult);
		return false;
	}

	FillResult(OutResult, true, EARSessionResultCode::Success);
	return true;
}

bool UARSessionSubsystem::JoinSessionByIndex(const int32 ResultIndex, FARSessionResult& OutResult)
{
	if (IsStayOfflineEnabled())
	{
		FillResult(OutResult, false, EARSessionResultCode::OfflineBlocked, TEXT("Stay Offline is enabled."));
		OnJoinSessionCompleted.Broadcast(OutResult);
		return false;
	}

	if (bOperationInFlight)
	{
		FillResult(OutResult, false, EARSessionResultCode::Busy, TEXT("A session operation is already in flight."));
		return false;
	}

	if (!CachedNativeSearchResults.IsValidIndex(ResultIndex))
	{
		FillResult(OutResult, false, EARSessionResultCode::SessionNotFound, TEXT("Search result index is invalid."));
		OnJoinSessionCompleted.Broadcast(OutResult);
		return false;
	}

	FName SubsystemName = NAME_None;
	IOnlineSessionPtr Session = ResolveSessionInterface(false, SubsystemName);
	if (!Session.IsValid())
	{
		FillResult(OutResult, false, EARSessionResultCode::NoSessionInterface, TEXT("No online session interface available."));
		OnJoinSessionCompleted.Broadcast(OutResult);
		return false;
	}

	bOperationInFlight = true;
	CurrentOperation = ESessionOperation::Join;
	ActiveSubsystemName = SubsystemName;
	FOnJoinSessionCompleteDelegate Delegate;
	Delegate.BindUObject(this, &UARSessionSubsystem::HandleJoinSessionComplete);
	JoinSessionCompleteHandle = Session->AddOnJoinSessionCompleteDelegate_Handle(Delegate);

	if (!Session->JoinSession(0, GameSessionName, CachedNativeSearchResults[ResultIndex]))
	{
		Session->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteHandle);
		JoinSessionCompleteHandle.Reset();
		bOperationInFlight = false;
		FillResult(OutResult, false, EARSessionResultCode::JoinFailed, TEXT("JoinSession failed to start."));
		OnJoinSessionCompleted.Broadcast(OutResult);
		return false;
	}

	FillResult(OutResult, true, EARSessionResultCode::Success);
	return true;
}

bool UARSessionSubsystem::DestroySession(FARSessionResult& OutResult)
{
	if (bOperationInFlight)
	{
		FillResult(OutResult, false, EARSessionResultCode::Busy, TEXT("A session operation is already in flight."));
		return false;
	}

	FName SubsystemName = NAME_None;
	IOnlineSessionPtr Session = ResolveSessionInterface(false, SubsystemName);
	if (!Session.IsValid())
	{
		FillResult(OutResult, false, EARSessionResultCode::NoSessionInterface, TEXT("No online session interface available."));
		OnDestroySessionCompleted.Broadcast(OutResult);
		return false;
	}

	if (!Session->GetNamedSession(GameSessionName))
	{
		FillResult(OutResult, true, EARSessionResultCode::Success);
		OnDestroySessionCompleted.Broadcast(OutResult);
		return true;
	}

	bOperationInFlight = true;
	CurrentOperation = ESessionOperation::Destroy;
	ActiveSubsystemName = SubsystemName;
	FOnDestroySessionCompleteDelegate Delegate;
	Delegate.BindUObject(this, &UARSessionSubsystem::HandleDestroySessionComplete);
	DestroySessionCompleteHandle = Session->AddOnDestroySessionCompleteDelegate_Handle(Delegate);

	if (!Session->DestroySession(GameSessionName))
	{
		Session->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteHandle);
		DestroySessionCompleteHandle.Reset();
		bOperationInFlight = false;
		FillResult(OutResult, false, EARSessionResultCode::DestroyFailed, TEXT("DestroySession failed to start."));
		OnDestroySessionCompleted.Broadcast(OutResult);
		return false;
	}

	FillResult(OutResult, true, EARSessionResultCode::Success);
	return true;
}

bool UARSessionSubsystem::RefreshJoinability(FARSessionResult& OutResult)
{
	if (bOperationInFlight)
	{
		FillResult(OutResult, false, EARSessionResultCode::Busy, TEXT("A session operation is already in flight."));
		return false;
	}

	FName SubsystemName = NAME_None;
	IOnlineSessionPtr Session = ResolveSessionInterface(false, SubsystemName);
	if (!Session.IsValid())
	{
		FillResult(OutResult, false, EARSessionResultCode::NoSessionInterface, TEXT("No online session interface available."));
		OnRefreshJoinabilityCompleted.Broadcast(OutResult);
		return false;
	}

	FNamedOnlineSession* Existing = Session->GetNamedSession(GameSessionName);
	if (!Existing)
	{
		FillResult(OutResult, false, EARSessionResultCode::SessionNotFound, TEXT("No active session exists."));
		OnRefreshJoinabilityCompleted.Broadcast(OutResult);
		return false;
	}

	const int32 OpenConnections = ComputeOpenPublicConnections();
	const bool bCanJoin = !IsStayOfflineEnabled() && OpenConnections > 0;

	FOnlineSessionSettings NewSettings = Existing->SessionSettings;
	NewSettings.NumPublicConnections = FMath::Clamp(OpenConnections, 0, 2);
	NewSettings.bShouldAdvertise = bCanJoin;
	NewSettings.bAllowJoinInProgress = bCanJoin;
	NewSettings.bAllowJoinViaPresence = bCanJoin;
	NewSettings.bAllowInvites = bCanJoin;
	UARGameInstance::ApplyARProtocolSessionSetting(NewSettings);

	bOperationInFlight = true;
	CurrentOperation = ESessionOperation::Refresh;
	ActiveSubsystemName = SubsystemName;
	FOnUpdateSessionCompleteDelegate Delegate;
	Delegate.BindUObject(this, &UARSessionSubsystem::HandleUpdateSessionComplete);
	UpdateSessionCompleteHandle = Session->AddOnUpdateSessionCompleteDelegate_Handle(Delegate);

	if (!Session->UpdateSession(GameSessionName, NewSettings, true))
	{
		Session->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateSessionCompleteHandle);
		UpdateSessionCompleteHandle.Reset();
		bOperationInFlight = false;
		FillResult(OutResult, false, EARSessionResultCode::UpdateFailed, TEXT("UpdateSession failed to start."));
		OnRefreshJoinabilityCompleted.Broadcast(OutResult);
		return false;
	}

	FillResult(OutResult, true, EARSessionResultCode::Success);
	return true;
}

bool UARSessionSubsystem::RequestLocalPlayerJoin(FARSessionResult& OutResult)
{
	if (CountCurrentARPlayers() >= 2)
	{
		FillResult(OutResult, false, EARSessionResultCode::SessionFull, TEXT("Player cap reached (2)."));
		OnLocalJoinCompleted.Broadcast(OutResult);
		return false;
	}

	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		FillResult(OutResult, false, EARSessionResultCode::NoWorld, TEXT("GameInstance is unavailable."));
		OnLocalJoinCompleted.Broadcast(OutResult);
		return false;
	}

	FString Error;
	if (!GI->CreateLocalPlayer(-1, Error, true))
	{
		FillResult(OutResult, false, EARSessionResultCode::LocalJoinFailed, Error.IsEmpty() ? TEXT("CreateLocalPlayer failed.") : Error);
		OnLocalJoinCompleted.Broadcast(OutResult);
		return false;
	}

	FARSessionResult RefreshResult;
	RefreshJoinability(RefreshResult);

	FillResult(OutResult, true, EARSessionResultCode::Success);
	OnLocalJoinCompleted.Broadcast(OutResult);
	return true;
}

IOnlineSessionPtr UARSessionSubsystem::ResolveSessionInterface(const bool bPreferLAN, FName& OutSubsystemName) const
{
	OutSubsystemName = NAME_None;

	if (bPreferLAN)
	{
		if (IOnlineSubsystem* NullSubsystem = IOnlineSubsystem::Get(NULL_SUBSYSTEM))
		{
			OutSubsystemName = NullSubsystem->GetSubsystemName();
			return NullSubsystem->GetSessionInterface();
		}

		if (IOnlineSubsystem* DefaultSubsystem = IOnlineSubsystem::Get())
		{
			OutSubsystemName = DefaultSubsystem->GetSubsystemName();
			return DefaultSubsystem->GetSessionInterface();
		}
		return nullptr;
	}

	if (IOnlineSubsystem* DefaultSubsystem = IOnlineSubsystem::Get())
	{
		OutSubsystemName = DefaultSubsystem->GetSubsystemName();
		const bool bDefaultIsNullLike = IsNullLikeSubsystemName(OutSubsystemName);

		if (IsStayOfflineEnabled())
		{
			if (!bDefaultIsNullLike)
			{
				return nullptr;
			}
			return DefaultSubsystem->GetSessionInterface();
		}

		if (!bDefaultIsNullLike)
		{
			if (IOnlineSessionPtr DefaultSession = DefaultSubsystem->GetSessionInterface())
			{
				return DefaultSession;
			}
		}
	}

	// Fallback for environments where default service is Null but Steam is available.
	if (!IsStayOfflineEnabled())
	{
		if (IOnlineSubsystem* SteamSubsystem = IOnlineSubsystem::Get(STEAM_SUBSYSTEM))
		{
			OutSubsystemName = SteamSubsystem->GetSubsystemName();
			if (IOnlineSessionPtr SteamSession = SteamSubsystem->GetSessionInterface())
			{
				return SteamSession;
			}
		}
	}

	if (IOnlineSubsystem* DefaultSubsystem = IOnlineSubsystem::Get())
	{
		OutSubsystemName = DefaultSubsystem->GetSubsystemName();
		return DefaultSubsystem->GetSessionInterface();
	}

	return nullptr;
}

int32 UARSessionSubsystem::CountCurrentARPlayers() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0;
	}

	const AGameStateBase* GS = World->GetGameState();
	if (!GS)
	{
		return 0;
	}

	int32 Count = 0;
	for (APlayerState* PS : GS->PlayerArray)
	{
		if (Cast<AARPlayerStateBase>(PS))
		{
			++Count;
		}
	}
	return Count;
}

int32 UARSessionSubsystem::ComputeOpenPublicConnections() const
{
	return FMath::Clamp(2 - CountCurrentARPlayers(), 0, 2);
}

bool UARSessionSubsystem::BuildDesiredSessionSettings(const bool bPreferLAN, FOnlineSessionSettings& OutSettings, FARSessionResult& OutResult) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		FillResult(OutResult, false, EARSessionResultCode::NoWorld, TEXT("No world available."));
		return false;
	}

	const int32 OpenConnections = ComputeOpenPublicConnections();
	const bool bCanJoin = !IsStayOfflineEnabled() && OpenConnections > 0;

	OutSettings = FOnlineSessionSettings();
	OutSettings.bIsLANMatch = bPreferLAN;
	OutSettings.NumPublicConnections = FMath::Clamp(OpenConnections, 0, 2);
	OutSettings.NumPrivateConnections = 0;
	OutSettings.bShouldAdvertise = bCanJoin;
	OutSettings.bAllowJoinInProgress = bCanJoin;
	OutSettings.bAllowInvites = bCanJoin;
	OutSettings.bUsesPresence = !bPreferLAN;
	OutSettings.bUseLobbiesIfAvailable = !bPreferLAN;
	OutSettings.bAllowJoinViaPresence = bCanJoin;
	OutSettings.bAllowJoinViaPresenceFriendsOnly = false;
	OutSettings.bUseLobbiesVoiceChatIfAvailable = !bPreferLAN;
	OutSettings.Set(SETTING_MAPNAME, World->GetMapName(), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	UARGameInstance::ApplyARProtocolSessionSetting(OutSettings);

	FillResult(OutResult, true, EARSessionResultCode::Success);
	return true;
}

void UARSessionSubsystem::FillResult(FARSessionResult& OutResult, const bool bSuccess, const EARSessionResultCode Code, const FString& Error) const
{
	OutResult.bSuccess = bSuccess;
	OutResult.ResultCode = Code;
	OutResult.Error = Error;
}

void UARSessionSubsystem::BroadcastFindCompleted(const FARSessionResult& Result)
{
	OnFindSessionsCompleted.Broadcast(Result, LastFindResults);
}

void UARSessionSubsystem::RebuildLastFindResults()
{
	LastFindResults.Reset();
	LastFindResults.Reserve(CachedNativeSearchResults.Num());

	for (const FOnlineSessionSearchResult& NativeResult : CachedNativeSearchResults)
	{
		FARSessionSearchResultData Row;
		Row.SessionId = NativeResult.GetSessionIdStr();
		Row.OwningUserName = NativeResult.Session.OwningUserName;
		Row.MaxPlayers = NativeResult.Session.SessionSettings.NumPublicConnections;
		const int32 OpenConnections = NativeResult.Session.NumOpenPublicConnections;
		Row.CurrentPlayers = FMath::Clamp(Row.MaxPlayers - OpenConnections, 0, Row.MaxPlayers);
		Row.bIsLAN = NativeResult.Session.SessionSettings.bIsLANMatch;
		Row.PingInMs = NativeResult.PingInMs;

		int32 ProtocolValue = 0;
		if (UARGameInstance::GetARProtocolFromSession(NativeResult, ProtocolValue))
		{
			Row.bProtocolPresent = true;
			Row.ProtocolVersion = ProtocolValue;
			Row.bProtocolCompatible = UARGameInstance::IsARProtocolCompatible(ProtocolValue);
		}

		LastFindResults.Add(MoveTemp(Row));
	}
}

void UARSessionSubsystem::DestroySessionBestEffort()
{
	if (bOperationInFlight)
	{
		return;
	}

	FARSessionResult Result;
	DestroySession(Result);
}

void UARSessionSubsystem::HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	FARSessionResult Result;
	FillResult(Result, bWasSuccessful, bWasSuccessful ? EARSessionResultCode::Success : EARSessionResultCode::CreateFailed,
		bWasSuccessful ? FString() : FString::Printf(TEXT("CreateSession failed for '%s'."), *SessionName.ToString()));

	if (IOnlineSubsystem* ActiveSubsystem = ActiveSubsystemName.IsNone() ? IOnlineSubsystem::Get() : IOnlineSubsystem::Get(ActiveSubsystemName))
	{
		if (IOnlineSessionPtr Session = ActiveSubsystem->GetSessionInterface(); Session.IsValid() && CreateSessionCompleteHandle.IsValid())
		{
			Session->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteHandle);
			CreateSessionCompleteHandle.Reset();
		}
	}

	bOperationInFlight = false;
	CurrentOperation = ESessionOperation::None;
	OnEnsureSessionCompleted.Broadcast(Result);
}

void UARSessionSubsystem::HandleUpdateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	FARSessionResult Result;
	FillResult(Result, bWasSuccessful, bWasSuccessful ? EARSessionResultCode::Success : EARSessionResultCode::UpdateFailed,
		bWasSuccessful ? FString() : FString::Printf(TEXT("UpdateSession failed for '%s'."), *SessionName.ToString()));

	if (IOnlineSubsystem* ActiveSubsystem = ActiveSubsystemName.IsNone() ? IOnlineSubsystem::Get() : IOnlineSubsystem::Get(ActiveSubsystemName))
	{
		if (IOnlineSessionPtr Session = ActiveSubsystem->GetSessionInterface(); Session.IsValid() && UpdateSessionCompleteHandle.IsValid())
		{
			Session->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateSessionCompleteHandle);
			UpdateSessionCompleteHandle.Reset();
		}
	}

	bOperationInFlight = false;
	const ESessionOperation CompletedOperation = CurrentOperation;
	CurrentOperation = ESessionOperation::None;

	if (CompletedOperation == ESessionOperation::Ensure)
	{
		OnEnsureSessionCompleted.Broadcast(Result);
		return;
	}

	if (CompletedOperation == ESessionOperation::Refresh)
	{
		OnRefreshJoinabilityCompleted.Broadcast(Result);
	}
}

void UARSessionSubsystem::HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	FARSessionResult Result;
	FillResult(Result, bWasSuccessful, bWasSuccessful ? EARSessionResultCode::Success : EARSessionResultCode::DestroyFailed,
		bWasSuccessful ? FString() : FString::Printf(TEXT("DestroySession failed for '%s'."), *SessionName.ToString()));

	if (IOnlineSubsystem* ActiveSubsystem = ActiveSubsystemName.IsNone() ? IOnlineSubsystem::Get() : IOnlineSubsystem::Get(ActiveSubsystemName))
	{
		if (IOnlineSessionPtr Session = ActiveSubsystem->GetSessionInterface(); Session.IsValid() && DestroySessionCompleteHandle.IsValid())
		{
			Session->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteHandle);
			DestroySessionCompleteHandle.Reset();
		}
	}

	bOperationInFlight = false;
	CurrentOperation = ESessionOperation::None;
	OnDestroySessionCompleted.Broadcast(Result);
}

void UARSessionSubsystem::HandleFindSessionsComplete(bool bWasSuccessful)
{
	FARSessionResult Result;

	if (IOnlineSubsystem* ActiveSubsystem = ActiveSubsystemName.IsNone() ? IOnlineSubsystem::Get() : IOnlineSubsystem::Get(ActiveSubsystemName))
	{
		if (IOnlineSessionPtr Session = ActiveSubsystem->GetSessionInterface(); Session.IsValid() && FindSessionsCompleteHandle.IsValid())
		{
			Session->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteHandle);
			FindSessionsCompleteHandle.Reset();
		}
	}

	bOperationInFlight = false;
	CurrentOperation = ESessionOperation::None;
	CachedNativeSearchResults = (ActiveSessionSearch.IsValid() ? ActiveSessionSearch->SearchResults : TArray<FOnlineSessionSearchResult>());
	RebuildLastFindResults();

	FillResult(Result, bWasSuccessful, bWasSuccessful ? EARSessionResultCode::Success : EARSessionResultCode::FindFailed,
		bWasSuccessful ? FString() : TEXT("FindSessions failed."));
	BroadcastFindCompleted(Result);
}

void UARSessionSubsystem::HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type ResultType)
{
	FARSessionResult Result;

	IOnlineSessionPtr Session = nullptr;
	if (IOnlineSubsystem* ActiveSubsystem = ActiveSubsystemName.IsNone() ? IOnlineSubsystem::Get() : IOnlineSubsystem::Get(ActiveSubsystemName))
	{
		Session = ActiveSubsystem->GetSessionInterface();
	}
	if (Session.IsValid() && JoinSessionCompleteHandle.IsValid())
	{
		Session->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteHandle);
		JoinSessionCompleteHandle.Reset();
	}

	bOperationInFlight = false;
	CurrentOperation = ESessionOperation::None;

	if (!Session.IsValid())
	{
		FillResult(Result, false, EARSessionResultCode::NoSessionInterface, TEXT("No online session interface available after join."));
		OnJoinSessionCompleted.Broadcast(Result);
		return;
	}

	if (ResultType != EOnJoinSessionCompleteResult::Success)
	{
		FillResult(Result, false, EARSessionResultCode::JoinFailed, TEXT("JoinSession failed."));
		OnJoinSessionCompleted.Broadcast(Result);
		return;
	}

	FString ConnectString;
	if (!Session->GetResolvedConnectString(SessionName, ConnectString))
	{
		FillResult(Result, false, EARSessionResultCode::JoinFailed, TEXT("Could not resolve connect string."));
		OnJoinSessionCompleted.Broadcast(Result);
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		FillResult(Result, false, EARSessionResultCode::NoWorld, TEXT("No world available for client travel."));
		OnJoinSessionCompleted.Broadcast(Result);
		return;
	}

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC)
	{
		FillResult(Result, false, EARSessionResultCode::JoinFailed, TEXT("No local player controller available for join travel."));
		OnJoinSessionCompleted.Broadcast(Result);
		return;
	}

	PC->ClientTravel(ConnectString, TRAVEL_Absolute);
	FillResult(Result, true, EARSessionResultCode::Success);
	OnJoinSessionCompleted.Broadcast(Result);
}
