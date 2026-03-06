/**
 * @file ARSessionSubsystem.h
 * @brief Session subsystem for Steam/LAN/local multiplayer flow.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARSaveTypes.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ARSessionSubsystem.generated.h"

class IOnlineSession;
class FOnlineSessionSearch;
class FOnlineSessionSettings;

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

	// Ensures a host session exists for current flow (lobby/run). If already present, updates joinability.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Session")
	bool EnsureSessionForCurrentFlow(bool bPreferLAN, FARSessionResult& OutResult);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Session")
	bool FindSessions(bool bLANQuery, int32 MaxResults, FARSessionResult& OutResult);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Session")
	bool JoinSessionByIndex(int32 ResultIndex, FARSessionResult& OutResult);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Session")
	bool DestroySession(FARSessionResult& OutResult);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Session")
	bool RefreshJoinability(FARSessionResult& OutResult);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Session")
	bool RequestLocalPlayerJoin(FARSessionResult& OutResult);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Session")
	const TArray<FARSessionSearchResultData>& GetLastFindResults() const { return LastFindResults; }

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Session")
	FAROnSessionActionCompleted OnEnsureSessionCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Session")
	FAROnSessionFindCompleted OnFindSessionsCompleted;

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
		Ensure,
		Find,
		Join,
		Destroy,
		Refresh
	};

	IOnlineSessionPtr ResolveSessionInterface(bool bPreferLAN, FName& OutSubsystemName) const;
	int32 CountCurrentARPlayers() const;
	int32 ComputeOpenPublicConnections() const;
	bool BuildDesiredSessionSettings(bool bPreferLAN, FOnlineSessionSettings& OutSettings, FARSessionResult& OutResult) const;
	void FillResult(FARSessionResult& OutResult, bool bSuccess, EARSessionResultCode Code, const FString& Error = FString()) const;
	void BroadcastFindCompleted(const FARSessionResult& Result);
	void RebuildLastFindResults();
	void DestroySessionBestEffort();

	void HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleUpdateSessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleFindSessionsComplete(bool bWasSuccessful);
	void HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);

	FDelegateHandle CreateSessionCompleteHandle;
	FDelegateHandle UpdateSessionCompleteHandle;
	FDelegateHandle DestroySessionCompleteHandle;
	FDelegateHandle FindSessionsCompleteHandle;
	FDelegateHandle JoinSessionCompleteHandle;

	TSharedPtr<FOnlineSessionSearch> ActiveSessionSearch;
	TArray<FOnlineSessionSearchResult> CachedNativeSearchResults;

	UPROPERTY(Transient)
	TArray<FARSessionSearchResultData> LastFindResults;

	FName ActiveSubsystemName = NAME_None;
	bool bOperationInFlight = false;
	ESessionOperation CurrentOperation = ESessionOperation::None;
};
