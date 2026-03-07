#include "ARSessionAsyncActions.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

namespace
{
static UARSessionSubsystem* ResolveSessionSubsystem(UObject* WorldContextObject)
{
	if (!WorldContextObject || !GEngine)
	{
		return nullptr;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	if (!World)
	{
		return nullptr;
	}

	UGameInstance* GI = World->GetGameInstance();
	return GI ? GI->GetSubsystem<UARSessionSubsystem>() : nullptr;
}

static FARSessionResult MakeSubsystemMissingResult()
{
	FARSessionResult Result;
	Result.bSuccess = false;
	Result.ResultCode = EARSessionResultCode::NoSessionInterface;
	Result.Error = TEXT("ARSessionSubsystem is unavailable.");
	return Result;
}
}

UARCreateSessionAsyncAction* UARCreateSessionAsyncAction::CreateSessionAsync(
	UObject* WorldContextObject,
	const bool bUseLAN,
	const FString& SessionDisplayName)
{
	UARCreateSessionAsyncAction* Action = NewObject<UARCreateSessionAsyncAction>();
	Action->WorldContextObject = WorldContextObject;
	Action->bUseLAN = bUseLAN;
	Action->SessionDisplayName = SessionDisplayName;
	Action->RegisterWithGameInstance(WorldContextObject);
	return Action;
}

void UARCreateSessionAsyncAction::Activate()
{
	UARSessionSubsystem* Subsystem = ResolveSessionSubsystem(WorldContextObject.Get());
	if (!Subsystem)
	{
		HandleCompleted(MakeSubsystemMissingResult());
		return;
	}

	SessionSubsystem = Subsystem;
	Subsystem->OnCreateSessionCompleted.AddDynamic(this, &UARCreateSessionAsyncAction::HandleCompleted);

	FARSessionResult StartResult;
	const bool bStarted = Subsystem->CreateSessionNamed(bUseLAN, SessionDisplayName, StartResult);
	if (!bStarted && !bCompleted)
	{
		HandleCompleted(StartResult);
	}
}

void UARCreateSessionAsyncAction::HandleCompleted(const FARSessionResult& Result)
{
	if (bCompleted)
	{
		return;
	}

	bCompleted = true;
	CleanupBinding();

	if (Result.bSuccess)
	{
		OnSuccess.Broadcast(Result);
	}
	else
	{
		OnFailure.Broadcast(Result);
	}

	SetReadyToDestroy();
}

void UARCreateSessionAsyncAction::CleanupBinding()
{
	if (UARSessionSubsystem* Subsystem = SessionSubsystem.Get())
	{
		Subsystem->OnCreateSessionCompleted.RemoveDynamic(this, &UARCreateSessionAsyncAction::HandleCompleted);
	}
}

UARFindSessionsAsyncAction* UARFindSessionsAsyncAction::FindSessionsAsync(UObject* WorldContextObject, const bool bLANQuery, const int32 MaxResults)
{
	UARFindSessionsAsyncAction* Action = NewObject<UARFindSessionsAsyncAction>();
	Action->WorldContextObject = WorldContextObject;
	Action->bLANQuery = bLANQuery;
	Action->MaxResults = MaxResults;
	Action->RegisterWithGameInstance(WorldContextObject);
	return Action;
}

void UARFindSessionsAsyncAction::Activate()
{
	UARSessionSubsystem* Subsystem = ResolveSessionSubsystem(WorldContextObject.Get());
	if (!Subsystem)
	{
		HandleCompleted(MakeSubsystemMissingResult(), TArray<FARSessionSearchResultData>());
		return;
	}

	SessionSubsystem = Subsystem;
	Subsystem->OnFindSessionsCompleted.AddDynamic(this, &UARFindSessionsAsyncAction::HandleCompleted);

	FARSessionResult StartResult;
	const bool bStarted = Subsystem->FindSessions(bLANQuery, MaxResults, StartResult);
	if (!bStarted && !bCompleted)
	{
		HandleCompleted(StartResult, Subsystem->GetLastFindResults());
	}
}

void UARFindSessionsAsyncAction::HandleCompleted(const FARSessionResult& Result, const TArray<FARSessionSearchResultData>& Results)
{
	if (bCompleted)
	{
		return;
	}

	bCompleted = true;
	CleanupBinding();

	if (Result.bSuccess)
	{
		OnSuccess.Broadcast(Result, Results);
	}
	else
	{
		OnFailure.Broadcast(Result, Results);
	}

	SetReadyToDestroy();
}

void UARFindSessionsAsyncAction::CleanupBinding()
{
	if (UARSessionSubsystem* Subsystem = SessionSubsystem.Get())
	{
		Subsystem->OnFindSessionsCompleted.RemoveDynamic(this, &UARFindSessionsAsyncAction::HandleCompleted);
	}
}

UARJoinSessionAsyncAction* UARJoinSessionAsyncAction::JoinSessionByIndexAsync(UObject* WorldContextObject, const int32 ResultIndex)
{
	UARJoinSessionAsyncAction* Action = NewObject<UARJoinSessionAsyncAction>();
	Action->WorldContextObject = WorldContextObject;
	Action->ResultIndex = ResultIndex;
	Action->RegisterWithGameInstance(WorldContextObject);
	return Action;
}

void UARJoinSessionAsyncAction::Activate()
{
	UARSessionSubsystem* Subsystem = ResolveSessionSubsystem(WorldContextObject.Get());
	if (!Subsystem)
	{
		HandleCompleted(MakeSubsystemMissingResult());
		return;
	}

	SessionSubsystem = Subsystem;
	Subsystem->OnJoinSessionCompleted.AddDynamic(this, &UARJoinSessionAsyncAction::HandleCompleted);

	FARSessionResult StartResult;
	const bool bStarted = Subsystem->JoinSessionByIndex(ResultIndex, StartResult);
	if (!bStarted && !bCompleted)
	{
		HandleCompleted(StartResult);
	}
}

void UARJoinSessionAsyncAction::HandleCompleted(const FARSessionResult& Result)
{
	if (bCompleted)
	{
		return;
	}

	bCompleted = true;
	CleanupBinding();

	if (Result.bSuccess)
	{
		OnSuccess.Broadcast(Result);
	}
	else
	{
		OnFailure.Broadcast(Result);
	}

	SetReadyToDestroy();
}

void UARJoinSessionAsyncAction::CleanupBinding()
{
	if (UARSessionSubsystem* Subsystem = SessionSubsystem.Get())
	{
		Subsystem->OnJoinSessionCompleted.RemoveDynamic(this, &UARJoinSessionAsyncAction::HandleCompleted);
	}
}

UARDestroySessionAsyncAction* UARDestroySessionAsyncAction::DestroySessionAsync(UObject* WorldContextObject)
{
	UARDestroySessionAsyncAction* Action = NewObject<UARDestroySessionAsyncAction>();
	Action->WorldContextObject = WorldContextObject;
	Action->RegisterWithGameInstance(WorldContextObject);
	return Action;
}

void UARDestroySessionAsyncAction::Activate()
{
	UARSessionSubsystem* Subsystem = ResolveSessionSubsystem(WorldContextObject.Get());
	if (!Subsystem)
	{
		HandleCompleted(MakeSubsystemMissingResult());
		return;
	}

	SessionSubsystem = Subsystem;
	Subsystem->OnDestroySessionCompleted.AddDynamic(this, &UARDestroySessionAsyncAction::HandleCompleted);

	FARSessionResult StartResult;
	const bool bStarted = Subsystem->DestroySession(StartResult);
	if (!bStarted && !bCompleted)
	{
		HandleCompleted(StartResult);
	}
}

void UARDestroySessionAsyncAction::HandleCompleted(const FARSessionResult& Result)
{
	if (bCompleted)
	{
		return;
	}

	bCompleted = true;
	CleanupBinding();

	if (Result.bSuccess)
	{
		OnSuccess.Broadcast(Result);
	}
	else
	{
		OnFailure.Broadcast(Result);
	}

	SetReadyToDestroy();
}

void UARDestroySessionAsyncAction::CleanupBinding()
{
	if (UARSessionSubsystem* Subsystem = SessionSubsystem.Get())
	{
		Subsystem->OnDestroySessionCompleted.RemoveDynamic(this, &UARDestroySessionAsyncAction::HandleCompleted);
	}
}
