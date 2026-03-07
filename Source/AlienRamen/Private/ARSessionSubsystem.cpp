#include "ARSessionSubsystem.h"

#include "ARGameInstance.h"
#include "ARLog.h"
#include "ARNetworkUserSettings.h"
#include "ARPlayerStateBase.h"
#include "ARSaveSubsystem.h"
#include "CreateSessionCallbackProxyAdvanced.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "FindSessionsCallbackProxyAdvanced.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Kismet/GameplayStatics.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemNames.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSessionSettings.h"
#include "Modules/ModuleManager.h"

namespace
{
	static const FName GameSessionName = NAME_GameSession;
	static const FName SessionSetting_ARLobbyName(TEXT("ARLobbyName"));
	static constexpr int32 MaxPublicPlayerConnections = 2;

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

	static APlayerController* ResolveLocalPlayerController(UWorld* World)
	{
		return World ? World->GetFirstPlayerController() : nullptr;
	}
}

void UARSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	BindInviteAcceptedDelegate();
}

void UARSessionSubsystem::Deinitialize()
{
	ClearInviteAcceptedDelegate();

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
		if (CancelFindSessionsCompleteHandle.IsValid())
		{
			Session->ClearOnCancelFindSessionsCompleteDelegate_Handle(CancelFindSessionsCompleteHandle);
		}
		if (JoinSessionCompleteHandle.IsValid())
		{
			Session->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteHandle);
		}
		if (FindFriendSessionCompleteHandle.IsValid())
		{
			Session->ClearOnFindFriendSessionCompleteDelegate_Handle(0, FindFriendSessionCompleteHandle);
		}
	}

	CachedNativeSearchResults.Reset();
	LastFindResults.Reset();
	ActiveSessionSearch.Reset();
	ActiveAdvancedCreateProxy = nullptr;
	ActiveAdvancedFindProxy = nullptr;
	bOperationInFlight = false;
	CurrentOperation = ESessionOperation::None;
	LastFindMaxResults = 50;
	bFindRetryWithoutFilters = false;
	bPendingInviteJoinAfterDestroy = false;
	PendingInviteControllerId = 0;
	PendingInviteSearchResult = FOnlineSessionSearchResult();
	bPendingJoinAfterDestroy = false;
	PendingJoinControllerId = 0;
	PendingJoinSearchResult = FOnlineSessionSearchResult();
	InviteDelegateSubsystemName = NAME_None;
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

bool UARSessionSubsystem::CreateSession(const bool bUseLAN, FARSessionResult& OutResult)
{
	return CreateSessionNamed(bUseLAN, FString(), OutResult);
}

bool UARSessionSubsystem::CreateSessionNamed(const bool bUseLAN, const FString& SessionDisplayName, FARSessionResult& OutResult)
{
	if (!SessionUserInviteAcceptedHandle.IsValid())
	{
		BindInviteAcceptedDelegate();
	}

	if (IsStayOfflineEnabled())
	{
		FillResult(OutResult, false, EARSessionResultCode::OfflineBlocked, TEXT("Stay Offline is enabled."));
		OnCreateSessionCompleted.Broadcast(OutResult);
		return false;
	}

	if (bOperationInFlight)
	{
		UE_LOG(ARLog, Warning, TEXT("[Session] CreateSession blocked: another session operation is in flight (%d)."), static_cast<int32>(CurrentOperation));
		FillResult(OutResult, false, EARSessionResultCode::Busy, TEXT("A session operation is already in flight."));
		OnCreateSessionCompleted.Broadcast(OutResult);
		return false;
	}

	FName SubsystemName = NAME_None;
	IOnlineSessionPtr Session = ResolveSessionInterface(bUseLAN, SubsystemName);
	if (!Session.IsValid())
	{
		FillResult(OutResult, false, EARSessionResultCode::NoSessionInterface, TEXT("No online session interface available."));
		OnCreateSessionCompleted.Broadcast(OutResult);
		return false;
	}
	UE_LOG(ARLog, Log, TEXT("[Session] CreateSession starting (LAN=%s, Subsystem=%s)."),
		bUseLAN ? TEXT("true") : TEXT("false"),
		*SubsystemName.ToString());

	FOnlineSessionSettings DesiredSettings;
	if (!BuildDesiredSessionSettings(bUseLAN, SessionDisplayName, DesiredSettings, OutResult))
	{
		OnCreateSessionCompleted.Broadcast(OutResult);
		return false;
	}

	const FNamedOnlineSession* ExistingSession = Session->GetNamedSession(GameSessionName);

	bOperationInFlight = true;
	CurrentOperation = ESessionOperation::Create;
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
			OnCreateSessionCompleted.Broadcast(OutResult);
			return false;
		}
	}
	else
	{
		UWorld* World = GetWorld();
		APlayerController* LocalPlayerController = ResolveLocalPlayerController(World);
		if (LocalPlayerController)
		{
			TArray<FSessionPropertyKeyPair> ExtraSettings;
			const FString ResolvedDisplayName = ResolveSessionDisplayName(SessionDisplayName);
			FSessionPropertyKeyPair LobbyNameSetting;
			LobbyNameSetting.Key = SessionSetting_ARLobbyName;
			LobbyNameSetting.Data.SetValue(ResolvedDisplayName);
			ExtraSettings.Add(LobbyNameSetting);

			FSessionPropertyKeyPair SessionKeySetting;
			SessionKeySetting.Key = SETTING_SESSIONKEY;
			SessionKeySetting.Data.SetValue(ResolvedDisplayName);
			ExtraSettings.Add(SessionKeySetting);

			ActiveAdvancedCreateProxy = UCreateSessionCallbackProxyAdvanced::CreateAdvancedSession(
				this,
				ExtraSettings,
				LocalPlayerController,
				MaxPublicPlayerConnections,
				0,
				bUseLAN,
				true,
				false,
				!bUseLAN,
				true,
				false,
				false,
				false,
				true,
				!bUseLAN,
				true);

			if (ActiveAdvancedCreateProxy)
			{
				ActiveAdvancedCreateProxy->OnSuccess.AddDynamic(this, &UARSessionSubsystem::HandleAdvancedCreateSuccess);
				ActiveAdvancedCreateProxy->OnFailure.AddDynamic(this, &UARSessionSubsystem::HandleAdvancedCreateFailure);
				ActiveAdvancedCreateProxy->Activate();
				FillResult(OutResult, true, EARSessionResultCode::Success);
				return true;
			}
		}

		FOnCreateSessionCompleteDelegate Delegate;
		Delegate.BindUObject(this, &UARSessionSubsystem::HandleCreateSessionComplete);
		CreateSessionCompleteHandle = Session->AddOnCreateSessionCompleteDelegate_Handle(Delegate);
		if (!Session->CreateSession(0, GameSessionName, DesiredSettings))
		{
			Session->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteHandle);
			CreateSessionCompleteHandle.Reset();
			bOperationInFlight = false;
			FillResult(OutResult, false, EARSessionResultCode::CreateFailed, TEXT("CreateSession failed to start."));
			OnCreateSessionCompleted.Broadcast(OutResult);
			return false;
		}
	}

	FillResult(OutResult, true, EARSessionResultCode::Success);
	return true;
}

bool UARSessionSubsystem::FindSessions(const bool bLANQuery, const int32 MaxResults, FARSessionResult& OutResult)
{
	if (!SessionUserInviteAcceptedHandle.IsValid())
	{
		BindInviteAcceptedDelegate();
	}

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

	UE_LOG(ARLog, Log, TEXT("[Session] FindSessions starting (LAN=%s, MaxResults=%d, Subsystem=%s)."),
		bLANQuery ? TEXT("true") : TEXT("false"),
		MaxResults,
		*SubsystemName.ToString());

	if (APlayerController* LocalPlayerController = ResolveLocalPlayerController(GetWorld()))
	{
		LastFindMaxResults = FMath::Max(1, MaxResults);
		bLastFindWasLANQuery = bLANQuery;
		bFindRetryWithoutFilters = false;
		bOperationInFlight = true;
		CurrentOperation = ESessionOperation::Find;
		ActiveSubsystemName = SubsystemName;

		TArray<FSessionsSearchSetting> Filters;
		ActiveAdvancedFindProxy = UFindSessionsCallbackProxyAdvanced::FindSessionsAdvanced(
			this,
			LocalPlayerController,
			LastFindMaxResults,
			bLANQuery,
			EBPServerPresenceSearchType::AllServers,
			Filters,
			false,
			false,
			false,
			0);

		if (ActiveAdvancedFindProxy)
		{
			ActiveAdvancedFindProxy->OnSuccess.AddDynamic(this, &UARSessionSubsystem::HandleAdvancedFindSuccess);
			ActiveAdvancedFindProxy->OnFailure.AddDynamic(this, &UARSessionSubsystem::HandleAdvancedFindFailure);
			ActiveAdvancedFindProxy->Activate();
			FillResult(OutResult, true, EARSessionResultCode::Success);
			return true;
		}

		bOperationInFlight = false;
		CurrentOperation = ESessionOperation::None;
	}

	ActiveSessionSearch = MakeShared<FOnlineSessionSearch>();
	ActiveSessionSearch->bIsLanQuery = bLANQuery;
	LastFindMaxResults = FMath::Max(1, MaxResults);
	ActiveSessionSearch->MaxSearchResults = LastFindMaxResults;
	bLastFindWasLANQuery = bLANQuery;
	bFindRetryWithoutFilters = false;
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
	UE_LOG(ARLog, Log, TEXT("[Session] JoinSessionByIndex invoked (Index=%d, CachedResults=%d, LastFindWasLAN=%s, InFlight=%s)."),
		ResultIndex,
		CachedNativeSearchResults.Num(),
		bLastFindWasLANQuery ? TEXT("true") : TEXT("false"),
		bOperationInFlight ? TEXT("true") : TEXT("false"));

	if (!SessionUserInviteAcceptedHandle.IsValid())
	{
		BindInviteAcceptedDelegate();
	}

	if (IsStayOfflineEnabled())
	{
		UE_LOG(ARLog, Warning, TEXT("[Session] JoinSessionByIndex blocked: Stay Offline is enabled."));
		FillResult(OutResult, false, EARSessionResultCode::OfflineBlocked, TEXT("Stay Offline is enabled."));
		OnJoinSessionCompleted.Broadcast(OutResult);
		return false;
	}

	if (bOperationInFlight)
	{
		UE_LOG(ARLog, Warning, TEXT("[Session] JoinSessionByIndex blocked: another session operation is in flight."));
		FillResult(OutResult, false, EARSessionResultCode::Busy, TEXT("A session operation is already in flight."));
		OnJoinSessionCompleted.Broadcast(OutResult);
		return false;
	}

	if (!CachedNativeSearchResults.IsValidIndex(ResultIndex))
	{
		UE_LOG(ARLog, Warning, TEXT("[Session] JoinSessionByIndex blocked: invalid index %d for %d cached results."),
			ResultIndex, CachedNativeSearchResults.Num());
		FillResult(OutResult, false, EARSessionResultCode::SessionNotFound, TEXT("Search result index is invalid."));
		OnJoinSessionCompleted.Broadcast(OutResult);
		return false;
	}

	FName SubsystemName = NAME_None;
	IOnlineSessionPtr Session = ResolveSessionInterface(bLastFindWasLANQuery, SubsystemName);
	if (!Session.IsValid())
	{
		UE_LOG(ARLog, Warning, TEXT("[Session] JoinSessionByIndex blocked: no session interface (LAN query=%s)."),
			bLastFindWasLANQuery ? TEXT("true") : TEXT("false"));
		FillResult(OutResult, false, EARSessionResultCode::NoSessionInterface, TEXT("No online session interface available."));
		OnJoinSessionCompleted.Broadcast(OutResult);
		return false;
	}

	const FOnlineSessionSearchResult& Candidate = CachedNativeSearchResults[ResultIndex];
	if (!Candidate.IsValid())
	{
		UE_LOG(ARLog, Warning, TEXT("[Session] JoinSessionByIndex blocked: selected search result is invalid (Index=%d)."), ResultIndex);
		FillResult(OutResult, false, EARSessionResultCode::SessionNotFound, TEXT("Selected search result is invalid."));
		OnJoinSessionCompleted.Broadcast(OutResult);
		return false;
	}

	if (const FNamedOnlineSession* ExistingSession = Session->GetNamedSession(GameSessionName))
	{
		const FString ExistingId = ExistingSession->GetSessionIdStr();
		const FString CandidateId = Candidate.GetSessionIdStr();
		if (!ExistingId.IsEmpty() && ExistingId.Equals(CandidateId, ESearchCase::IgnoreCase))
		{
			UE_LOG(ARLog, Warning, TEXT("[Session] JoinSessionByIndex blocked: attempted to join current session (SessionId=%s)."),
				*CandidateId);
			FillResult(OutResult, false, EARSessionResultCode::JoinFailed, TEXT("Cannot join your own current session."));
			OnJoinSessionCompleted.Broadcast(OutResult);
			return false;
		}

		// If we're in a different active session, tear it down first then retry join.
		PendingJoinSearchResult = Candidate;
		PendingJoinControllerId = 0;
		bPendingJoinAfterDestroy = true;
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
			CurrentOperation = ESessionOperation::None;
			bPendingJoinAfterDestroy = false;
			PendingJoinControllerId = 0;
			PendingJoinSearchResult = FOnlineSessionSearchResult();
			FillResult(OutResult, false, EARSessionResultCode::DestroyFailed, TEXT("Could not destroy existing session before joining."));
			OnJoinSessionCompleted.Broadcast(OutResult);
			return false;
		}

		FillResult(OutResult, true, EARSessionResultCode::Success);
		return true;
	}

	UE_LOG(ARLog, Log, TEXT("[Session] JoinSessionByIndex starting (Index=%d, Subsystem=%s, SessionId=%s)."),
		ResultIndex,
		*SubsystemName.ToString(),
		*Candidate.GetSessionIdStr());

	ActiveSubsystemName = SubsystemName;
	return BeginJoinSession(Session, 0, Candidate, OutResult);
}

bool UARSessionSubsystem::FindFriendSession(const FBPUniqueNetId& FriendUniqueNetId, FARSessionResult& OutResult)
{
	if (IsStayOfflineEnabled())
	{
		FillResult(OutResult, false, EARSessionResultCode::OfflineBlocked, TEXT("Stay Offline is enabled."));
		OnFindFriendSessionCompleted.Broadcast(OutResult, LastFindResults);
		return false;
	}

	if (bOperationInFlight)
	{
		FillResult(OutResult, false, EARSessionResultCode::Busy, TEXT("A session operation is already in flight."));
		return false;
	}

	const FUniqueNetId* FriendNetId = FriendUniqueNetId.GetUniqueNetId();
	if (!FriendNetId)
	{
		FillResult(OutResult, false, EARSessionResultCode::SessionNotFound, TEXT("Friend unique net id is invalid."));
		OnFindFriendSessionCompleted.Broadcast(OutResult, LastFindResults);
		return false;
	}

	FName SubsystemName = NAME_None;
	IOnlineSessionPtr Session = ResolveSessionInterface(false, SubsystemName);
	if (!Session.IsValid())
	{
		FillResult(OutResult, false, EARSessionResultCode::NoSessionInterface, TEXT("No online session interface available."));
		OnFindFriendSessionCompleted.Broadcast(OutResult, LastFindResults);
		return false;
	}

	bOperationInFlight = true;
	CurrentOperation = ESessionOperation::Find;
	ActiveSubsystemName = SubsystemName;
	FOnFindFriendSessionCompleteDelegate Delegate;
	Delegate.BindUObject(this, &UARSessionSubsystem::HandleFindFriendSessionComplete);
	FindFriendSessionCompleteHandle = Session->AddOnFindFriendSessionCompleteDelegate_Handle(0, Delegate);

	if (!Session->FindFriendSession(0, *FriendNetId))
	{
		Session->ClearOnFindFriendSessionCompleteDelegate_Handle(0, FindFriendSessionCompleteHandle);
		FindFriendSessionCompleteHandle.Reset();
		bOperationInFlight = false;
		CurrentOperation = ESessionOperation::None;
		FillResult(OutResult, false, EARSessionResultCode::FindFailed, TEXT("FindFriendSession failed to start."));
		OnFindFriendSessionCompleted.Broadcast(OutResult, LastFindResults);
		return false;
	}

	FillResult(OutResult, true, EARSessionResultCode::Success);
	return true;
}

bool UARSessionSubsystem::InviteFriendToSession(const FBPUniqueNetId& FriendUniqueNetId, FARSessionResult& OutResult)
{
	if (IsStayOfflineEnabled())
	{
		FillResult(OutResult, false, EARSessionResultCode::OfflineBlocked, TEXT("Stay Offline is enabled."));
		OnInviteFriendCompleted.Broadcast(OutResult);
		return false;
	}

	const FUniqueNetId* FriendNetId = FriendUniqueNetId.GetUniqueNetId();
	if (!FriendNetId)
	{
		FillResult(OutResult, false, EARSessionResultCode::InviteFailed, TEXT("Friend unique net id is invalid."));
		OnInviteFriendCompleted.Broadcast(OutResult);
		return false;
	}

	FName SubsystemName = NAME_None;
	IOnlineSessionPtr Session = ResolveSessionInterface(false, SubsystemName);
	if (!Session.IsValid())
	{
		FillResult(OutResult, false, EARSessionResultCode::NoSessionInterface, TEXT("No online session interface available."));
		OnInviteFriendCompleted.Broadcast(OutResult);
		return false;
	}

	UWorld* World = GetWorld();
	APlayerController* PC = ResolveLocalPlayerController(World);
	const FUniqueNetIdRepl LocalUserIdRepl = PC && PC->PlayerState ? PC->PlayerState->GetUniqueId() : FUniqueNetIdRepl();
	const FUniqueNetId* LocalUserId = LocalUserIdRepl.GetUniqueNetId().Get();
	if (!LocalUserId)
	{
		FillResult(OutResult, false, EARSessionResultCode::InviteFailed, TEXT("Local unique net id is unavailable."));
		OnInviteFriendCompleted.Broadcast(OutResult);
		return false;
	}

	const bool bInvited = Session->SendSessionInviteToFriend(*LocalUserId, GameSessionName, *FriendNetId);
	if (!bInvited)
	{
		FillResult(OutResult, false, EARSessionResultCode::InviteFailed, TEXT("SendSessionInviteToFriend failed."));
		OnInviteFriendCompleted.Broadcast(OutResult);
		return false;
	}

	FillResult(OutResult, true, EARSessionResultCode::Success);
	OnInviteFriendCompleted.Broadcast(OutResult);
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
	const bool bCanJoin = !IsStayOfflineEnabled() && (Existing->SessionSettings.bIsLANMatch || OpenConnections > 0);

	FOnlineSessionSettings NewSettings = Existing->SessionSettings;
	NewSettings.NumPublicConnections = MaxPublicPlayerConnections;
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

bool UARSessionSubsystem::AddLocalPlayer(FARSessionResult& OutResult)
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
		// Keep LAN host/find/join on one backend for the current runtime.
		if (IOnlineSubsystem* DefaultSubsystem = IOnlineSubsystem::Get())
		{
			OutSubsystemName = DefaultSubsystem->GetSubsystemName();
			if (IOnlineSessionPtr DefaultSession = DefaultSubsystem->GetSessionInterface())
			{
				return DefaultSession;
			}
		}

		if (IOnlineSubsystem* NullSubsystem = IOnlineSubsystem::Get(NULL_SUBSYSTEM))
		{
			OutSubsystemName = NullSubsystem->GetSubsystemName();
			return NullSubsystem->GetSessionInterface();
		}

		FModuleManager::Get().LoadModulePtr<IModuleInterface>(TEXT("OnlineSubsystemNull"));
		if (IOnlineSubsystem* LoadedNullSubsystem = IOnlineSubsystem::Get(NULL_SUBSYSTEM))
		{
			OutSubsystemName = LoadedNullSubsystem->GetSubsystemName();
			return LoadedNullSubsystem->GetSessionInterface();
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

bool UARSessionSubsystem::BeginJoinSession(IOnlineSessionPtr Session, const int32 LocalUserNum, const FOnlineSessionSearchResult& SearchResult, FARSessionResult& OutResult)
{
	if (!Session.IsValid())
	{
		FillResult(OutResult, false, EARSessionResultCode::NoSessionInterface, TEXT("No online session interface available."));
		OnJoinSessionCompleted.Broadcast(OutResult);
		return false;
	}

	bOperationInFlight = true;
	CurrentOperation = ESessionOperation::Join;

	FOnJoinSessionCompleteDelegate Delegate;
	Delegate.BindUObject(this, &UARSessionSubsystem::HandleJoinSessionComplete);
	JoinSessionCompleteHandle = Session->AddOnJoinSessionCompleteDelegate_Handle(Delegate);

	if (!Session->JoinSession(LocalUserNum, GameSessionName, SearchResult))
	{
		UE_LOG(ARLog, Warning, TEXT("[Session] BeginJoinSession failed to start (UserNum=%d, SessionId=%s)."),
			LocalUserNum, *SearchResult.GetSessionIdStr());
		Session->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteHandle);
		JoinSessionCompleteHandle.Reset();
		bOperationInFlight = false;
		CurrentOperation = ESessionOperation::None;
		FillResult(OutResult, false, EARSessionResultCode::JoinFailed, TEXT("JoinSession failed to start."));
		OnJoinSessionCompleted.Broadcast(OutResult);
		return false;
	}

	UE_LOG(ARLog, Log, TEXT("[Session] BeginJoinSession started (UserNum=%d, SessionId=%s)."),
		LocalUserNum, *SearchResult.GetSessionIdStr());
	FillResult(OutResult, true, EARSessionResultCode::Success);
	return true;
}

bool UARSessionSubsystem::CancelFindSessions(FARSessionResult& OutResult)
{
	if (!bOperationInFlight || CurrentOperation != ESessionOperation::Find)
	{
		FillResult(OutResult, true, EARSessionResultCode::Success);
		OnCancelFindSessionsCompleted.Broadcast(OutResult);
		return true;
	}

	FName SubsystemName = NAME_None;
	IOnlineSessionPtr Session = ResolveSessionInterface(bLastFindWasLANQuery, SubsystemName);
	if (!Session.IsValid())
	{
		FillResult(OutResult, false, EARSessionResultCode::NoSessionInterface, TEXT("No online session interface available."));
		OnCancelFindSessionsCompleted.Broadcast(OutResult);
		return false;
	}

	ActiveSubsystemName = SubsystemName;
	FOnCancelFindSessionsCompleteDelegate Delegate;
	Delegate.BindUObject(this, &UARSessionSubsystem::HandleCancelFindSessionsComplete);
	CancelFindSessionsCompleteHandle = Session->AddOnCancelFindSessionsCompleteDelegate_Handle(Delegate);

	if (!Session->CancelFindSessions())
	{
		Session->ClearOnCancelFindSessionsCompleteDelegate_Handle(CancelFindSessionsCompleteHandle);
		CancelFindSessionsCompleteHandle.Reset();
		FillResult(OutResult, false, EARSessionResultCode::CancelFailed, TEXT("CancelFindSessions failed to start."));
		OnCancelFindSessionsCompleted.Broadcast(OutResult);
		return false;
	}

	FillResult(OutResult, true, EARSessionResultCode::Success);
	return true;
}

void UARSessionSubsystem::BindInviteAcceptedDelegate()
{
	if (SessionUserInviteAcceptedHandle.IsValid())
	{
		return;
	}

	IOnlineSubsystem* InviteSubsystem = IOnlineSubsystem::Get(STEAM_SUBSYSTEM);
	if (!InviteSubsystem)
	{
		InviteSubsystem = IOnlineSubsystem::Get();
	}

	if (!InviteSubsystem)
	{
		return;
	}

	IOnlineSessionPtr Session = InviteSubsystem->GetSessionInterface();
	if (!Session.IsValid())
	{
		return;
	}

	FOnSessionUserInviteAcceptedDelegate Delegate;
	Delegate.BindUObject(this, &UARSessionSubsystem::HandleSessionUserInviteAccepted);
	SessionUserInviteAcceptedHandle = Session->AddOnSessionUserInviteAcceptedDelegate_Handle(Delegate);
	InviteDelegateSubsystemName = InviteSubsystem->GetSubsystemName();
}

void UARSessionSubsystem::ClearInviteAcceptedDelegate()
{
	if (!SessionUserInviteAcceptedHandle.IsValid())
	{
		return;
	}

	IOnlineSessionPtr Session = nullptr;
	if (!InviteDelegateSubsystemName.IsNone())
	{
		if (IOnlineSubsystem* InviteSubsystem = IOnlineSubsystem::Get(InviteDelegateSubsystemName))
		{
			Session = InviteSubsystem->GetSessionInterface();
		}
	}

	if (!Session.IsValid())
	{
		if (IOnlineSubsystem* DefaultSubsystem = IOnlineSubsystem::Get())
		{
			Session = DefaultSubsystem->GetSessionInterface();
		}
	}

	if (Session.IsValid())
	{
		Session->ClearOnSessionUserInviteAcceptedDelegate_Handle(SessionUserInviteAcceptedHandle);
	}

	SessionUserInviteAcceptedHandle.Reset();
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
	return FMath::Clamp(MaxPublicPlayerConnections - CountCurrentARPlayers(), 0, MaxPublicPlayerConnections);
}

bool UARSessionSubsystem::BuildDesiredSessionSettings(
	const bool bPreferLAN,
	const FString& SessionDisplayName,
	FOnlineSessionSettings& OutSettings,
	FARSessionResult& OutResult)
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		FillResult(OutResult, false, EARSessionResultCode::NoWorld, TEXT("No world available."));
		return false;
	}

	const int32 OpenConnections = ComputeOpenPublicConnections();
	const bool bCanJoin = !IsStayOfflineEnabled() && (bPreferLAN || OpenConnections > 0);

	OutSettings = FOnlineSessionSettings();
	OutSettings.bIsLANMatch = bPreferLAN;
	OutSettings.NumPublicConnections = MaxPublicPlayerConnections;
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
	const FString ResolvedDisplayName = ResolveSessionDisplayName(SessionDisplayName);
	OutSettings.Set(SessionSetting_ARLobbyName, ResolvedDisplayName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	OutSettings.Set(SETTING_SESSIONKEY, ResolvedDisplayName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	UARGameInstance::ApplyARProtocolSessionSetting(OutSettings);

	FillResult(OutResult, true, EARSessionResultCode::Success);
	return true;
}

FString UARSessionSubsystem::ResolveSessionDisplayName(const FString& RequestedSessionDisplayName)
{
	const FString RequestedTrimmed = RequestedSessionDisplayName.TrimStartAndEnd();
	if (!RequestedTrimmed.IsEmpty())
	{
		return RequestedTrimmed;
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UARSaveSubsystem* SaveSubsystem = GI->GetSubsystem<UARSaveSubsystem>())
		{
			if (SaveSubsystem->HasCurrentSave())
			{
				const FString SaveSlotBaseName = SaveSubsystem->GetCurrentSlotBaseName().ToString().TrimStartAndEnd();
				if (!SaveSlotBaseName.IsEmpty())
				{
					return SaveSlotBaseName;
				}
			}

			const FString GeneratedName = SaveSubsystem->GenerateRandomSlotBaseName(false).ToString().TrimStartAndEnd();
			if (!GeneratedName.IsEmpty())
			{
				return GeneratedName;
			}
		}
	}

	return TEXT("AlienRamenLobby");
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
	TSet<FString> SeenSessionIds;

	for (const FOnlineSessionSearchResult& NativeResult : CachedNativeSearchResults)
	{
		FARSessionSearchResultData Row;
		FString SessionDisplayName;
		NativeResult.Session.SessionSettings.Get(SessionSetting_ARLobbyName, SessionDisplayName);
		if (SessionDisplayName.IsEmpty())
		{
			NativeResult.Session.SessionSettings.Get(SETTING_SESSIONKEY, SessionDisplayName);
		}
		if (SessionDisplayName.IsEmpty())
		{
			SessionDisplayName = NativeResult.Session.OwningUserName;
		}
		Row.SessionId = NativeResult.GetSessionIdStr();
		if (!Row.SessionId.IsEmpty())
		{
			if (SeenSessionIds.Contains(Row.SessionId))
			{
				continue;
			}
			SeenSessionIds.Add(Row.SessionId);
		}

		if (SessionDisplayName.IsEmpty())
		{
			if (!Row.SessionId.IsEmpty())
			{
				SessionDisplayName = FString::Printf(TEXT("Session %s"), *Row.SessionId.Left(8));
			}
			else
			{
				SessionDisplayName = FString::Printf(TEXT("Session %d"), LastFindResults.Num() + 1);
			}
		}
		SessionDisplayName = SessionDisplayName.Replace(TEXT("_"), TEXT(" "));
		Row.SessionDisplayName = SessionDisplayName;
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

void UARSessionSubsystem::HandleAdvancedCreateSuccess()
{
	ActiveAdvancedCreateProxy = nullptr;
	bOperationInFlight = false;
	CurrentOperation = ESessionOperation::None;

	FARSessionResult Result;
	FillResult(Result, true, EARSessionResultCode::Success);
	OnCreateSessionCompleted.Broadcast(Result);
}

void UARSessionSubsystem::HandleAdvancedCreateFailure()
{
	ActiveAdvancedCreateProxy = nullptr;
	bOperationInFlight = false;
	CurrentOperation = ESessionOperation::None;

	FARSessionResult Result;
	FillResult(Result, false, EARSessionResultCode::CreateFailed, TEXT("AdvancedSessions CreateAdvancedSession failed."));
	OnCreateSessionCompleted.Broadcast(Result);
}

void UARSessionSubsystem::HandleAdvancedFindSuccess(const TArray<FBlueprintSessionResult>& Results)
{
	ActiveAdvancedFindProxy = nullptr;
	bOperationInFlight = false;
	CurrentOperation = ESessionOperation::None;

	CachedNativeSearchResults.Reset();
	CachedNativeSearchResults.Reserve(Results.Num());
	for (const FBlueprintSessionResult& SessionResult : Results)
	{
		if (SessionResult.OnlineResult.IsValid())
		{
			CachedNativeSearchResults.Add(SessionResult.OnlineResult);
		}
	}

	RebuildLastFindResults();
	FARSessionResult Result;
	const bool bFoundAny = LastFindResults.Num() > 0;
	FillResult(Result, true, bFoundAny ? EARSessionResultCode::Success : EARSessionResultCode::SessionNotFound, bFoundAny ? FString() : TEXT("No sessions found."));
	BroadcastFindCompleted(Result);
}

void UARSessionSubsystem::HandleAdvancedFindFailure(const TArray<FBlueprintSessionResult>& Results)
{
	(void)Results;

	ActiveAdvancedFindProxy = nullptr;
	bOperationInFlight = false;
	CurrentOperation = ESessionOperation::None;
	CachedNativeSearchResults.Reset();
	LastFindResults.Reset();

	FARSessionResult Result;
	FillResult(Result, false, EARSessionResultCode::FindFailed, TEXT("AdvancedSessions FindSessionsAdvanced failed."));
	BroadcastFindCompleted(Result);
}

void UARSessionSubsystem::HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	FARSessionResult Result;
	FillResult(Result, bWasSuccessful, bWasSuccessful ? EARSessionResultCode::Success : EARSessionResultCode::CreateFailed,
		bWasSuccessful ? FString() : FString::Printf(TEXT("CreateSession failed for '%s'."), *SessionName.ToString()));
	UE_LOG(ARLog, Log, TEXT("[Session] CreateSession completed (Success=%s, Subsystem=%s, SessionName=%s)."),
		bWasSuccessful ? TEXT("true") : TEXT("false"),
		*ActiveSubsystemName.ToString(),
		*SessionName.ToString());

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
	OnCreateSessionCompleted.Broadcast(Result);
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

	if (CompletedOperation == ESessionOperation::Create)
	{
		OnCreateSessionCompleted.Broadcast(Result);
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

	if (bPendingInviteJoinAfterDestroy)
	{
		const FOnlineSessionSearchResult InviteJoinResult = PendingInviteSearchResult;
		const int32 InviteControllerId = PendingInviteControllerId;
		bPendingInviteJoinAfterDestroy = false;
		PendingInviteControllerId = 0;
		PendingInviteSearchResult = FOnlineSessionSearchResult();

		if (!bWasSuccessful)
		{
			FARSessionResult JoinFailedResult;
			FillResult(JoinFailedResult, false, EARSessionResultCode::JoinFailed, TEXT("Invite join failed: could not destroy existing session."));
			OnJoinSessionCompleted.Broadcast(JoinFailedResult);
			return;
		}

		FName JoinSubsystemName = NAME_None;
		IOnlineSessionPtr JoinSession = ResolveSessionInterface(false, JoinSubsystemName);
		ActiveSubsystemName = JoinSubsystemName;

		FARSessionResult JoinStartResult;
		BeginJoinSession(JoinSession, InviteControllerId, InviteJoinResult, JoinStartResult);
		return;
	}

	if (bPendingJoinAfterDestroy)
	{
		const FOnlineSessionSearchResult JoinResult = PendingJoinSearchResult;
		const int32 JoinControllerId = PendingJoinControllerId;
		bPendingJoinAfterDestroy = false;
		PendingJoinControllerId = 0;
		PendingJoinSearchResult = FOnlineSessionSearchResult();

		if (!bWasSuccessful)
		{
			FARSessionResult JoinFailedResult;
			FillResult(JoinFailedResult, false, EARSessionResultCode::JoinFailed, TEXT("Join failed: could not destroy existing session."));
			OnJoinSessionCompleted.Broadcast(JoinFailedResult);
			return;
		}

		FName JoinSubsystemName = NAME_None;
		IOnlineSessionPtr JoinSession = ResolveSessionInterface(bLastFindWasLANQuery, JoinSubsystemName);
		ActiveSubsystemName = JoinSubsystemName;

		FARSessionResult JoinStartResult;
		BeginJoinSession(JoinSession, JoinControllerId, JoinResult, JoinStartResult);
		return;
	}

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
	CachedNativeSearchResults.Reset();
	if (ActiveSessionSearch.IsValid())
	{
		for (const FOnlineSessionSearchResult& NativeResult : ActiveSessionSearch->SearchResults)
		{
			if (!NativeResult.IsValid())
			{
				UE_LOG(ARLog, Verbose, TEXT("[Session] Skipping invalid search result entry."));
				continue;
			}

			CachedNativeSearchResults.Add(NativeResult);
		}
	}
	RebuildLastFindResults();

	if (bWasSuccessful && !bLastFindWasLANQuery && LastFindResults.Num() == 0 && !bFindRetryWithoutFilters)
	{
		if (IOnlineSubsystem* ActiveSubsystem = ActiveSubsystemName.IsNone() ? IOnlineSubsystem::Get() : IOnlineSubsystem::Get(ActiveSubsystemName))
		{
			if (IOnlineSessionPtr Session = ActiveSubsystem->GetSessionInterface(); Session.IsValid())
			{
				UE_LOG(ARLog, Log, TEXT("[Session] FindSessions retrying without strict online query filters."));
				bFindRetryWithoutFilters = true;
				ActiveSessionSearch = MakeShared<FOnlineSessionSearch>();
				ActiveSessionSearch->bIsLanQuery = false;
				ActiveSessionSearch->MaxSearchResults = FMath::Max(1, LastFindMaxResults);
				bOperationInFlight = true;
				CurrentOperation = ESessionOperation::Find;

				FOnFindSessionsCompleteDelegate RetryDelegate;
				RetryDelegate.BindUObject(this, &UARSessionSubsystem::HandleFindSessionsComplete);
				FindSessionsCompleteHandle = Session->AddOnFindSessionsCompleteDelegate_Handle(RetryDelegate);
				if (Session->FindSessions(0, ActiveSessionSearch.ToSharedRef()))
				{
					return;
				}

				Session->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteHandle);
				FindSessionsCompleteHandle.Reset();
				bOperationInFlight = false;
				CurrentOperation = ESessionOperation::None;
			}
		}
	}

	const bool bFoundAny = LastFindResults.Num() > 0;
	const EARSessionResultCode ResultCode = !bWasSuccessful
		? EARSessionResultCode::FindFailed
		: (bFoundAny ? EARSessionResultCode::Success : EARSessionResultCode::SessionNotFound);
	const FString Error = !bWasSuccessful
		? TEXT("FindSessions failed.")
		: (bFoundAny ? FString() : TEXT("No sessions found."));
	bFindRetryWithoutFilters = false;
	FillResult(Result, bWasSuccessful, ResultCode, Error);
	UE_LOG(ARLog, Log, TEXT("[Session] FindSessions completed (Success=%s, Results=%d, LAN=%s, Subsystem=%s)."),
		Result.bSuccess ? TEXT("true") : TEXT("false"),
		LastFindResults.Num(),
		bLastFindWasLANQuery ? TEXT("true") : TEXT("false"),
		*ActiveSubsystemName.ToString());
	BroadcastFindCompleted(Result);
}

void UARSessionSubsystem::HandleCancelFindSessionsComplete(bool bWasSuccessful)
{
	FARSessionResult Result;

	if (IOnlineSubsystem* ActiveSubsystem = ActiveSubsystemName.IsNone() ? IOnlineSubsystem::Get() : IOnlineSubsystem::Get(ActiveSubsystemName))
	{
		if (IOnlineSessionPtr Session = ActiveSubsystem->GetSessionInterface(); Session.IsValid() && CancelFindSessionsCompleteHandle.IsValid())
		{
			Session->ClearOnCancelFindSessionsCompleteDelegate_Handle(CancelFindSessionsCompleteHandle);
			CancelFindSessionsCompleteHandle.Reset();
		}

		if (IOnlineSessionPtr Session = ActiveSubsystem->GetSessionInterface(); Session.IsValid() && FindSessionsCompleteHandle.IsValid())
		{
			Session->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteHandle);
			FindSessionsCompleteHandle.Reset();
		}
	}

	if (bOperationInFlight && CurrentOperation == ESessionOperation::Find)
	{
		bOperationInFlight = false;
		CurrentOperation = ESessionOperation::None;
	}
	bFindRetryWithoutFilters = false;

	FillResult(
		Result,
		bWasSuccessful,
		bWasSuccessful ? EARSessionResultCode::Success : EARSessionResultCode::CancelFailed,
		bWasSuccessful ? FString() : TEXT("CancelFindSessions failed."));
	OnCancelFindSessionsCompleted.Broadcast(Result);
}

void UARSessionSubsystem::HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type ResultType)
{
	FARSessionResult Result;
	auto JoinResultToString = [](EOnJoinSessionCompleteResult::Type InResultType) -> const TCHAR*
	{
		switch (InResultType)
		{
		case EOnJoinSessionCompleteResult::Success:
			return TEXT("Success");
		case EOnJoinSessionCompleteResult::SessionIsFull:
			return TEXT("SessionIsFull");
		case EOnJoinSessionCompleteResult::SessionDoesNotExist:
			return TEXT("SessionDoesNotExist");
		case EOnJoinSessionCompleteResult::CouldNotRetrieveAddress:
			return TEXT("CouldNotRetrieveAddress");
		case EOnJoinSessionCompleteResult::AlreadyInSession:
			return TEXT("AlreadyInSession");
		case EOnJoinSessionCompleteResult::UnknownError:
		default:
			return TEXT("UnknownError");
		}
	};

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
		UE_LOG(ARLog, Warning, TEXT("[Session] JoinSession completed with failure (Result=%s, Subsystem=%s, SessionName=%s)."),
			JoinResultToString(ResultType),
			*ActiveSubsystemName.ToString(),
			*SessionName.ToString());
		EARSessionResultCode FailureCode = EARSessionResultCode::JoinFailed;
		if (ResultType == EOnJoinSessionCompleteResult::SessionIsFull)
		{
			FailureCode = EARSessionResultCode::SessionFull;
		}
		else if (ResultType == EOnJoinSessionCompleteResult::SessionDoesNotExist)
		{
			FailureCode = EARSessionResultCode::SessionNotFound;
		}
		FillResult(Result, false, FailureCode, FString::Printf(TEXT("JoinSession failed: %s."), JoinResultToString(ResultType)));
		OnJoinSessionCompleted.Broadcast(Result);
		return;
	}

	FString ConnectString;
	if (!Session->GetResolvedConnectString(SessionName, ConnectString))
	{
		UE_LOG(ARLog, Warning, TEXT("[Session] JoinSession succeeded but connect string was unresolved (Subsystem=%s, SessionName=%s)."),
			*ActiveSubsystemName.ToString(),
			*SessionName.ToString());
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
	UE_LOG(ARLog, Log, TEXT("[Session] JoinSession client travel (ConnectString=%s)."), *ConnectString);
	FillResult(Result, true, EARSessionResultCode::Success);
	OnJoinSessionCompleted.Broadcast(Result);
}

void UARSessionSubsystem::HandleFindFriendSessionComplete(int32 LocalUserNum, bool bWasSuccessful, const TArray<FOnlineSessionSearchResult>& SessionInfo)
{
	(void)LocalUserNum;

	if (IOnlineSubsystem* ActiveSubsystem = ActiveSubsystemName.IsNone() ? IOnlineSubsystem::Get() : IOnlineSubsystem::Get(ActiveSubsystemName))
	{
		if (IOnlineSessionPtr Session = ActiveSubsystem->GetSessionInterface(); Session.IsValid() && FindFriendSessionCompleteHandle.IsValid())
		{
			Session->ClearOnFindFriendSessionCompleteDelegate_Handle(0, FindFriendSessionCompleteHandle);
			FindFriendSessionCompleteHandle.Reset();
		}
	}

	bOperationInFlight = false;
	CurrentOperation = ESessionOperation::None;
	CachedNativeSearchResults.Reset();
	for (const FOnlineSessionSearchResult& NativeResult : SessionInfo)
	{
		if (NativeResult.IsValid())
		{
			CachedNativeSearchResults.Add(NativeResult);
		}
	}
	RebuildLastFindResults();

	FARSessionResult Result;
	const bool bFoundAny = LastFindResults.Num() > 0;
	FillResult(Result, bWasSuccessful, bFoundAny ? EARSessionResultCode::Success : EARSessionResultCode::SessionNotFound, bFoundAny ? FString() : TEXT("Friend is not in a joinable session."));
	OnFindFriendSessionCompleted.Broadcast(Result, LastFindResults);
}

void UARSessionSubsystem::HandleSessionUserInviteAccepted(
	const bool bWasSuccessful,
	const int32 ControllerId,
	FUniqueNetIdPtr UserId,
	const FOnlineSessionSearchResult& InviteResult)
{
	(void)UserId;

	if (IsStayOfflineEnabled())
	{
		FARSessionResult Result;
		FillResult(Result, false, EARSessionResultCode::OfflineBlocked, TEXT("Stay Offline is enabled."));
		OnJoinSessionCompleted.Broadcast(Result);
		return;
	}

	if (!bWasSuccessful || !InviteResult.IsValid())
	{
		FARSessionResult Result;
		FillResult(Result, false, EARSessionResultCode::JoinFailed, TEXT("Invite accepted but no valid session was provided."));
		OnJoinSessionCompleted.Broadcast(Result);
		return;
	}

	if (bOperationInFlight)
	{
		FARSessionResult Result;
		FillResult(Result, false, EARSessionResultCode::Busy, TEXT("A session operation is already in flight."));
		OnJoinSessionCompleted.Broadcast(Result);
		return;
	}

	FName SubsystemName = InviteDelegateSubsystemName;
	IOnlineSessionPtr Session = nullptr;
	if (!SubsystemName.IsNone())
	{
		if (IOnlineSubsystem* InviteSubsystem = IOnlineSubsystem::Get(SubsystemName))
		{
			Session = InviteSubsystem->GetSessionInterface();
		}
	}

	if (!Session.IsValid())
	{
		Session = ResolveSessionInterface(false, SubsystemName);
	}

	if (!Session.IsValid())
	{
		FARSessionResult Result;
		FillResult(Result, false, EARSessionResultCode::NoSessionInterface, TEXT("No online session interface available for invite join."));
		OnJoinSessionCompleted.Broadcast(Result);
		return;
	}

	if (Session->GetNamedSession(GameSessionName))
	{
		PendingInviteSearchResult = InviteResult;
		PendingInviteControllerId = FMath::Max(0, ControllerId);
		bPendingInviteJoinAfterDestroy = true;

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
			CurrentOperation = ESessionOperation::None;
			bPendingInviteJoinAfterDestroy = false;
			PendingInviteControllerId = 0;
			PendingInviteSearchResult = FOnlineSessionSearchResult();

			FARSessionResult Result;
			FillResult(Result, false, EARSessionResultCode::JoinFailed, TEXT("Invite join failed: could not destroy existing session."));
			OnJoinSessionCompleted.Broadcast(Result);
		}
		return;
	}

	ActiveSubsystemName = SubsystemName;
	FARSessionResult JoinStartResult;
	BeginJoinSession(Session, FMath::Max(0, ControllerId), InviteResult, JoinStartResult);
}

