/**
 * @file ARSessionAsyncActions.h
 * @brief Blueprint async session action nodes with success/failure exec pins.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARSessionSubsystem.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "ARSessionAsyncActions.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FARSessionAsyncActionCompleted, const FARSessionResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FARSessionFindAsyncActionCompleted, const FARSessionResult&, Result, const TArray<FARSessionSearchResultData>&, Results);

UCLASS()
class ALIENRAMEN_API UARCreateSessionAsyncAction : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UFUNCTION(
		BlueprintCallable,
		meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Create Session Async", Keywords = "session host lan steam async"),
		Category = "Alien Ramen|Session|Async")
	static UARCreateSessionAsyncAction* CreateSessionAsync(UObject* WorldContextObject, bool bUseLAN, const FString& SessionDisplayName = TEXT(""));

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Session|Async")
	FARSessionAsyncActionCompleted OnSuccess;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Session|Async")
	FARSessionAsyncActionCompleted OnFailure;

	virtual void Activate() override;

private:
	UFUNCTION()
	void HandleCompleted(const FARSessionResult& Result);

	void CleanupBinding();

	TWeakObjectPtr<UObject> WorldContextObject;
	TWeakObjectPtr<UARSessionSubsystem> SessionSubsystem;
	bool bUseLAN = false;
	bool bCompleted = false;
	FString SessionDisplayName;
};

UCLASS()
class ALIENRAMEN_API UARFindSessionsAsyncAction : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UFUNCTION(
		BlueprintCallable,
		meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Find Sessions Async", Keywords = "session find search lan steam async"),
		Category = "Alien Ramen|Session|Async")
	static UARFindSessionsAsyncAction* FindSessionsAsync(UObject* WorldContextObject, bool bLANQuery, int32 MaxResults = 50);

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Session|Async")
	FARSessionFindAsyncActionCompleted OnSuccess;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Session|Async")
	FARSessionFindAsyncActionCompleted OnFailure;

	virtual void Activate() override;

private:
	UFUNCTION()
	void HandleCompleted(const FARSessionResult& Result, const TArray<FARSessionSearchResultData>& Results);

	void CleanupBinding();

	TWeakObjectPtr<UObject> WorldContextObject;
	TWeakObjectPtr<UARSessionSubsystem> SessionSubsystem;
	bool bLANQuery = false;
	int32 MaxResults = 50;
	bool bCompleted = false;
};

UCLASS()
class ALIENRAMEN_API UARJoinSessionAsyncAction : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UFUNCTION(
		BlueprintCallable,
		meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Join Session By Index Async", Keywords = "session join async"),
		Category = "Alien Ramen|Session|Async")
	static UARJoinSessionAsyncAction* JoinSessionByIndexAsync(UObject* WorldContextObject, int32 ResultIndex);

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Session|Async")
	FARSessionAsyncActionCompleted OnSuccess;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Session|Async")
	FARSessionAsyncActionCompleted OnFailure;

	virtual void Activate() override;

private:
	UFUNCTION()
	void HandleCompleted(const FARSessionResult& Result);

	void CleanupBinding();

	TWeakObjectPtr<UObject> WorldContextObject;
	TWeakObjectPtr<UARSessionSubsystem> SessionSubsystem;
	int32 ResultIndex = INDEX_NONE;
	bool bCompleted = false;
};

UCLASS()
class ALIENRAMEN_API UARDestroySessionAsyncAction : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UFUNCTION(
		BlueprintCallable,
		meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Destroy Session Async", Keywords = "session destroy leave async"),
		Category = "Alien Ramen|Session|Async")
	static UARDestroySessionAsyncAction* DestroySessionAsync(UObject* WorldContextObject);

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Session|Async")
	FARSessionAsyncActionCompleted OnSuccess;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Session|Async")
	FARSessionAsyncActionCompleted OnFailure;

	virtual void Activate() override;

private:
	UFUNCTION()
	void HandleCompleted(const FARSessionResult& Result);

	void CleanupBinding();

	TWeakObjectPtr<UObject> WorldContextObject;
	TWeakObjectPtr<UARSessionSubsystem> SessionSubsystem;
	bool bCompleted = false;
};
