/**
 * @file ARSessionSubsystem.h
 * @brief Session subsystem for platform/LAN/local multiplayer flow.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARSaveTypes.h"
#include "BlueprintDataDefinitions.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ARSessionSubsystem.generated.h"

class FOnlineSessionSearch;
class FOnlineSessionSettings;
class UARNetworkRoutingSettings;

UENUM(BlueprintType)
enum class EARSessionResultCode : uint8
{
	Success = 0,
	OfflineBlocked,
	NoWorld,
	NoOnlineSubsystem,
	NoSessionInterface,
	Busy,
	SessionNotFound,
	SessionFull,
	CreateFailed,
	FindFailed,
	JoinFailed,
	DestroyFailed,
	UpdateFailed,
	InviteFailed,
	CancelFailed,
	LocalJoinFailed,
	Unknown
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARSessionResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|Session")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|Session")
	EARSessionResultCode ResultCode = EARSessionResultCode::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|Session")
	FString Error;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARSessionSearchResultData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|Session")
	FString SessionId;

	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|Session")
	FString SessionDisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|Session")
	FString OwningUserName;

	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|Session")
	int32 CurrentPlayers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|Session")
	int32 MaxPlayers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|Session")
	bool bIsLAN = false;

	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|Session")
	int32 PingInMs = -1;

	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|Session")
	bool bProtocolPresent = false;

	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|Session")
	int32 ProtocolVersion = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|Session")
	bool bProtocolCompatible = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAROnSessionActionCompleted, const FARSessionResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAROnSessionFindCompleted, const FARSessionResult&, Result, const TArray<FARSessionSearchResultData>&, Results);

/** Server/client session orchestration surface for platform/LAN/local multiplayer. */
UCLASS()
class ALIENRAMEN_API UARSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Session")
	bool IsStayOfflineEnabled() const;

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Session")
	void SetStayOfflineEnabled(bool bEnabled, bool& bOutRestartRecommended);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Session")
	bool CreateSession(bool bUseLAN, FARSessionResult& OutResult);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Session")
	bool CreateSessionNamed(bool bUseLAN, const FString& SessionDisplayName, FARSessionResult& OutResult);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Session")
	bool FindSessions(bool bLANQuery, int32 MaxResults, FARSessionResult& OutResult);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Session")
	bool CancelFindSessions(FARSessionResult& OutResult);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Session")
	bool JoinSessionByIndex(int32 ResultIndex, FARSessionResult& OutResult);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Session")
	bool FindFriendSession(const FBPUniqueNetId& FriendUniqueNetId, FARSessionResult& OutResult);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Session")
	bool InviteFriendToSession(const FBPUniqueNetId& FriendUniqueNetId, FARSessionResult& OutResult);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Session")
	bool DestroySession(FARSessionResult& OutResult);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Session")
	bool RefreshJoinability(FARSessionResult& OutResult);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Session")
	bool AddLocalPlayer(FARSessionResult& OutResult);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Session")
	const TArray<FARSessionSearchResultData>& GetLastFindResults() const { return LastFindResults; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Session|Routing")
	FName GetConfiguredInternetSubsystemName() const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Session|Routing")
	FName GetConfiguredLanSubsystemName() const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Session|Routing")
	TArray<FName> GetConfiguredInternetSubsystemFallbackOrder() const;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Session")
	FAROnSessionActionCompleted OnCreateSessionCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Session")
	FAROnSessionFindCompleted OnFindSessionsCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Session")
	FAROnSessionActionCompleted OnCancelFindSessionsCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Session")
	FAROnSessionActionCompleted OnJoinSessionCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Session")
	FAROnSessionFindCompleted OnFindFriendSessionCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Session")
	FAROnSessionActionCompleted OnInviteFriendCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Session")
	FAROnSessionActionCompleted OnDestroySessionCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Session")
	FAROnSessionActionCompleted OnRefreshJoinabilityCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Session")
	FAROnSessionActionCompleted OnLocalJoinCompleted;

private:
	enum class ESessionOperation : uint8
	{
		None = 0,
		Create,
		Find,
		Join,
		Destroy,
		Refresh
	};

	IOnlineSessionPtr ResolveSessionInterface(bool bPreferLAN, FName& OutSubsystemName, const TCHAR* RequestType) const;
	IOnlineSessionPtr GetSessionInterfaceForSubsystem(FName SubsystemName) const;
	IOnlineSessionPtr GetActiveSessionInterface() const;
	IOnlineSessionPtr FindSessionInterfaceOwningGameSession(FName& OutSubsystemName) const;

	bool BeginJoinSession(IOnlineSessionPtr Session, int32 LocalUserNum, const FOnlineSessionSearchResult& SearchResult, FARSessionResult& OutResult);
	int32 CountCurrentARPlayers() const;
	int32 ComputeOpenPublicConnections() const;
	bool BuildDesiredSessionSettings(bool bPreferLAN, const FString& SessionDisplayName, FOnlineSessionSettings& OutSettings, FARSessionResult& OutResult);
	FString ResolveSessionDisplayName(const FString& RequestedSessionDisplayName);
	void FillResult(FARSessionResult& OutResult, bool bSuccess, EARSessionResultCode Code, const FString& Error = FString()) const;
	void BroadcastFindCompleted(const FARSessionResult& Result);
	void RebuildLastFindResults();
	void DestroySessionBestEffort();

	void ClearTrackedSessionDelegateHandles(const IOnlineSessionPtr& Session, bool bClearFindFriendHandle);
	void ResetOperationState();
	void ResetFindState();
	const UARNetworkRoutingSettings* GetRoutingSettings() const;
	FName GetConfiguredInternetSubsystemNameInternal() const;
	FName GetConfiguredLanSubsystemNameInternal() const;
	void GetConfiguredInternetFallbackOrderInternal(TArray<FName>& OutFallbackOrder) const;
	void LogRouteDecision(const TCHAR* RequestType, bool bPreferLAN, bool bStayOffline, const FName& ChosenSubsystem, const FString& Reason) const;

	void BindInviteAcceptedDelegate();
	void ClearInviteAcceptedDelegate();

	void HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleUpdateSessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleFindSessionsComplete(bool bWasSuccessful);
	void HandleCancelFindSessionsComplete(bool bWasSuccessful);
	void HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void HandleFindFriendSessionComplete(int32 LocalUserNum, bool bWasSuccessful, const TArray<FOnlineSessionSearchResult>& SessionInfo);
	void HandleSessionUserInviteAccepted(bool bWasSuccessful, int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult);

	FDelegateHandle CreateSessionCompleteHandle;
	FDelegateHandle UpdateSessionCompleteHandle;
	FDelegateHandle DestroySessionCompleteHandle;
	FDelegateHandle FindSessionsCompleteHandle;
	FDelegateHandle CancelFindSessionsCompleteHandle;
	FDelegateHandle JoinSessionCompleteHandle;
	FDelegateHandle FindFriendSessionCompleteHandle;
	FDelegateHandle SessionUserInviteAcceptedHandle;

	TSharedPtr<FOnlineSessionSearch> ActiveSessionSearch;
	TArray<FOnlineSessionSearchResult> CachedNativeSearchResults;

	UPROPERTY(Transient)
	TArray<FARSessionSearchResultData> LastFindResults;

	FName ActiveSubsystemName = NAME_None;
	FName InviteDelegateSubsystemName = NAME_None;

	bool bOperationInFlight = false;
	bool bLastFindWasLANQuery = false;
	bool bFindRetryWithoutFilters = false;
	bool bPendingInviteJoinAfterDestroy = false;
	bool bPendingJoinAfterDestroy = false;

	int32 LastFindMaxResults = 50;
	int32 PendingInviteControllerId = 0;
	int32 PendingJoinControllerId = 0;

	FOnlineSessionSearchResult PendingInviteSearchResult;
	FOnlineSessionSearchResult PendingJoinSearchResult;

	ESessionOperation CurrentOperation = ESessionOperation::None;
};
