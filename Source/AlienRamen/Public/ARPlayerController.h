/**
 * @file ARPlayerController.h
 * @brief ARPlayerController header for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "TimerManager.h"
#include "ARPlayerController.generated.h"

class UARAbilitySet;
class UUserWidget;

/** Base player controller: owns save sync RPCs, travel requests, and common ability set handoff. */
UCLASS()
class ALIENRAMEN_API AARPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AARPlayerController();

	// Common abilities/effects every pawn gets when possessed (server grants via pawn).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AR|Abilities")
	TObjectPtr<UARAbilitySet> CommonAbilitySet;

	// Convenience accessor
	UFUNCTION(BlueprintCallable, Category = "AR|Abilities")
	const UARAbilitySet* GetCommonAbilitySet() const { return CommonAbilitySet; }

	// Client endpoint for persisting server-canonical save snapshots locally.
	UFUNCTION(Client, Reliable)
	void ClientPersistCanonicalSave(const TArray<uint8>& SaveBytes, FName SlotBaseName, int32 SlotNumber);

	// Client requests current server-canonical save snapshot on join/connect.
	UFUNCTION(Server, Reliable)
	void ServerRequestCanonicalSaveSync();

	// Session leave entrypoint for UI/BP. Routes to server when called by clients.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Session")
	void LeaveSession();

	// Server-side leave request handler.
	UFUNCTION(Server, Reliable)
	void ServerLeaveSession();

	// Controller travel entrypoint for UI/BP. Routes to server when called by clients.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Travel")
	void TryStartTravel(const FString& URL, const FString& Options = "", bool bSkipReadyChecks = false, bool bAbsolute = false, bool bSkipGameNotify = false, bool bUseOpenLevelInPIE = false);

	// Server-side travel request handler.
	UFUNCTION(Server, Reliable)
	void ServerTryStartTravel(const FString& URL, const FString& Options = "", bool bSkipReadyChecks = false, bool bAbsolute = false, bool bSkipGameNotify = false, bool bUseOpenLevelInPIE = false);

	// Unlock mutation entrypoints for UI/BP. Route to server when called by clients.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Save")
	void RequestAddUnlock(const FGameplayTag& UnlockTag);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Save")
	void RequestRemoveUnlock(const FGameplayTag& UnlockTag);

	UFUNCTION(Server, Reliable)
	void ServerRequestAddUnlock(const FGameplayTag& UnlockTag);

	UFUNCTION(Server, Reliable)
	void ServerRequestRemoveUnlock(const FGameplayTag& UnlockTag);

	// Initializes a custom default cursor widget on local controllers only.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|UI|Cursor")
	void InitializeCustomCursor();

	// Requests HUD initialization/rebind for the local controller context.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|UI|HUD")
	void RequestHUDInitialization();

	// BP hook to create/rebind HUD widgets when local controller context is ready or refreshed.
	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|UI|HUD")
	void BP_OnHUDInitializationRequested(AARPlayerController* SourceController, APlayerState* CurrentPlayerState, AGameStateBase* CurrentGameState);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Enables local-only custom cursor initialization from BeginPlay.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|UI|Cursor")
	bool bEnableCustomCursorInit = false;

	// Widget class to create and assign as the default mouse cursor when enabled.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|UI|Cursor", meta = (EditCondition = "bEnableCustomCursorInit"))
	TSubclassOf<UUserWidget> CursorDefaultWidgetClass;

	// Runtime cursor widget instance used for EMouseCursor::Default.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Alien Ramen|UI|Cursor")
	TObjectPtr<UUserWidget> Cursor = nullptr;

private:
	void LeaveSessionInternal();
	void TryStartTravelInternal(const FString& URL, const FString& Options, bool bSkipReadyChecks, bool bAbsolute, bool bSkipGameNotify, bool bUseOpenLevelInPIE);
	void RequestAddUnlockInternal(const FGameplayTag& UnlockTag);
	void RequestRemoveUnlockInternal(const FGameplayTag& UnlockTag);
	void RequestHUDInitializationInternal(bool bForceBroadcast);
	void StartHUDInitializationRetry();
	void StopHUDInitializationRetry();
	void HandleHUDInitializationRetry();

	UPROPERTY(Transient)
	bool bRequestedInitialCanonicalSaveSync = false;

	UPROPERTY(Transient)
	bool bHasBroadcastHUDInitialization = false;

	UPROPERTY(Transient)
	TWeakObjectPtr<APlayerState> LastHUDInitPlayerState;

	UPROPERTY(Transient)
	TWeakObjectPtr<AGameStateBase> LastHUDInitGameState;

	FTimerHandle HUDInitializationRetryTimer;
};
