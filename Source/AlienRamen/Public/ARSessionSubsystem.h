/**
 * @file ARSessionSubsystem.h
 * @brief Session subsystem for Steam/LAN/local multiplayer flow.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARSaveTypes.h"
#include "BlueprintDataDefinitions.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ARSessionSubsystem.generated.h"

class IOnlineSession;
class FOnlineSessionSearch;
class FOnlineSessionSettings;
class UCreateSessionCallbackProxyAdvanced;
class UFindSessionsCallbackProxyAdvanced;

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
	CancelFailed,
	JoinFailed,
	InviteFailed,
	DestroyFailed,
	UpdateFailed,
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
	FString SessionDisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|Session")
	FString SessionId;

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

/** Server/client session orchestration surface for Steam/LAN/local multiplayer. */
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

	// Creates/updates a host session for this runtime flow.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Session")
	bool CreateSession(bool bUseLAN, FARSessionResult& OutResult);

	// Creates/updates a host session and advertises a friendly lobby name.
	// If SessionDisplayName is empty, a default will be derived (current save slot base name, then random fallback).
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

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Session")
	FAROnSessionActionCompleted OnCreateSessionCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Session")
	FAROnSessionFindCompleted OnFindSessionsCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Session")
	FAROnSessionFindCompleted OnFindFriendSessionCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Session")
	FAROnSessionActionCompleted OnInviteFriendCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Session")
	FAROnSessionActionCompleted OnCancelFindSessionsCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Session")
	FAROnSessionActionCompleted OnJoinSessionCompleted;

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

	IOnlineSessionPtr ResolveSessionInterface(bool bPreferLAN, FName& OutSubsystemName) const;
	bool BeginJoinSession(IOnlineSessionPtr Session, int32 LocalUserNum, const FOnlineSessionSearchResult& SearchResult, FARSessionResult& OutResult);
	void BindInviteAcceptedDelegate();
	void ClearInviteAcceptedDelegate();
	int32 CountCurrentARPlayers() const;
	int32 ComputeOpenPublicConnections() const;
	bool BuildDesiredSessionSettings(bool bPreferLAN, const FString& SessionDisplayName, FOnlineSessionSettings& OutSettings, FARSessionResult& OutResult);
	FString ResolveSessionDisplayName(const FString& RequestedSessionDisplayName);
	void FillResult(FARSessionResult& OutResult, bool bSuccess, EARSessionResultCode Code, const FString& Error = FString()) const;
	void BroadcastFindCompleted(const FARSessionResult& Result);
	void RebuildLastFindResults();
	void DestroySessionBestEffort();

	void HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleUpdateSessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleFindSessionsComplete(bool bWasSuccessful);
	void HandleCancelFindSessionsComplete(bool bWasSuccessful);
	void HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void HandleSessionUserInviteAccepted(bool bWasSuccessful, int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult);
	void HandleFindFriendSessionComplete(int32 LocalUserNum, bool bWasSuccessful, const TArray<FOnlineSessionSearchResult>& SessionInfo);
	UFUNCTION()
	void HandleAdvancedCreateSuccess();
	UFUNCTION()
	void HandleAdvancedCreateFailure();
	UFUNCTION()
	void HandleAdvancedFindSuccess(const TArray<FBlueprintSessionResult>& Results);
	UFUNCTION()
	void HandleAdvancedFindFailure(const TArray<FBlueprintSessionResult>& Results);

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
	TObjectPtr<UCreateSessionCallbackProxyAdvanced> ActiveAdvancedCreateProxy = nullptr;
	UPROPERTY(Transient)
	TObjectPtr<UFindSessionsCallbackProxyAdvanced> ActiveAdvancedFindProxy = nullptr;

	UPROPERTY(Transient)
	TArray<FARSessionSearchResultData> LastFindResults;

	bool bLastFindWasLANQuery = false;
	int32 LastFindMaxResults = 50;
	bool bFindRetryWithoutFilters = false;
	bool bPendingInviteJoinAfterDestroy = false;
	int32 PendingInviteControllerId = 0;
	FOnlineSessionSearchResult PendingInviteSearchResult;
	bool bPendingJoinAfterDestroy = false;
	int32 PendingJoinControllerId = 0;
	FOnlineSessionSearchResult PendingJoinSearchResult;
	FName InviteDelegateSubsystemName = NAME_None;
	FName ActiveSubsystemName = NAME_None;
	bool bOperationInFlight = false;
	ESessionOperation CurrentOperation = ESessionOperation::None;
};
