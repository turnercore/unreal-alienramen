/**
 * @file ARInvaderPlayerController.h
 * @brief ARInvaderPlayerController header for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARInvaderTypes.h"
#include "ARPlayerController.h"
#include "ARInvaderSpicyTrackTypes.h"
#include "ARInvaderPlayerController.generated.h"

class AARPlayerStateBase;
class AARInvaderGameState;
class UARInvaderFullBlastMenuWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FAROnInvaderFullBlastMenuSessionUpdatedSignature,
	bool,
	bIsActive,
	const FARInvaderFullBlastSessionState&,
	SessionState,
	const TArray<FARInvaderUpgradeDefRow>&,
	OfferDefinitions);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAROnInvaderControllerRunEndedSignature, EARInvaderRunEndReason, EndReason);

UCLASS()
class ALIENRAMEN_API AARInvaderPlayerController : public AARPlayerController
{
	GENERATED_BODY()

public:
	AARInvaderPlayerController();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetPawn(APawn* InPawn) override;
	virtual void OnRep_PlayerState() override;

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader|Spice Track")
	void RequestActivateFullBlast();

	UFUNCTION(Server, Reliable)
	void ServerRequestActivateFullBlast();

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader|Spice Track")
	void RequestResolveFullBlastSelection(FGameplayTag SelectedUpgradeTag, int32 DesiredDestinationSlot = -1);

	UFUNCTION(Server, Reliable)
	void ServerRequestResolveFullBlastSelection(FGameplayTag SelectedUpgradeTag, int32 DesiredDestinationSlot = -1);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader|Spice Track")
	void RequestResolveFullBlastSkip();

	UFUNCTION(Server, Reliable)
	void ServerRequestResolveFullBlastSkip();

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader|Spice Track")
	void RequestActivateTrackUpgrade(int32 SlotIndex);

	UFUNCTION(Server, Reliable)
	void ServerRequestActivateTrackUpgrade(int32 SlotIndex);

	// Convenience entrypoint for IA_SpiceTrackDelta axis input.
	// Positive values move cursor up one tier, negative values move down one tier.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader|Spice Track|Input")
	void HandleSpiceTrackDeltaInput(float AxisValue);

	// Convenience entrypoint for IA_SpiceTrackActivate trigger:
	// activates the selected cursor tier; when cursor is at/above the current
	// full-blast tier it triggers Full Blast activation.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader|Spice Track|Input")
	void HandleSpiceTrackActivateFromCursor();

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader|Spice Track")
	void RequestStartSharingSpice();

	UFUNCTION(Server, Reliable)
	void ServerRequestStartSharingSpice();

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader|Spice Track")
	void RequestStopSharingSpice();

	UFUNCTION(Server, Reliable)
	void ServerRequestStopSharingSpice();

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader|Spice Track")
	void RequestSetOfferPresence(
		FGameplayTag HoveredUpgradeTag,
		int32 HoveredDestinationSlot,
		FGameplayTag SelectedUpgradeTag,
		int32 SelectedDestinationSlot,
		bool bHasSelection);

	UFUNCTION(Server, Reliable)
	void ServerRequestSetOfferPresence(
		FGameplayTag HoveredUpgradeTag,
		int32 HoveredDestinationSlot,
		FGameplayTag SelectedUpgradeTag,
		int32 SelectedDestinationSlot,
		bool bHasSelection);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader|Spice Track")
	void RequestClearOfferPresence();

	UFUNCTION(Server, Reliable)
	void ServerRequestClearOfferPresence();

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader")
	void RequestVoteEndRunEarly(bool bVoteYes = true);

	UFUNCTION(Server, Reliable)
	void ServerRequestVoteEndRunEarly(bool bVoteYes = true);

	/**
	 * Client notification from authoritative Invader GameMode that the director run has ended.
	 * Use this to start local end-sequence UI/animation; travel may follow immediately or after delay.
	 */
	UFUNCTION(Client, Reliable, Category = "Alien Ramen|Invader|Run End")
	void ClientHandleInvaderRunEnded(EARInvaderRunEndReason EndReason);

	/**
	 * Blueprint hook for local end-sequence handling when the run ends (for example fades, score cards, or
	 * input lock). Triggered from ClientHandleInvaderRunEnded.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|Invader|Run End", meta = (DisplayName = "On Invader Run Ended"))
	void OnInvaderRunEnded(EARInvaderRunEndReason EndReason);

	// Broadcast whenever full-blast menu state/data is refreshed for this local controller.
	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Invader|Spice Track|Full Blast")
	FAROnInvaderFullBlastMenuSessionUpdatedSignature OnInvaderFullBlastMenuSessionUpdated;

	/** Broadcast whenever this local controller is notified that the Invader run has ended. */
	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Invader|Run End")
	FAROnInvaderControllerRunEndedSignature OnInvaderRunEndedSignal;

private:
	AARPlayerStateBase* GetInvaderPlayerState() const;
	UFUNCTION()
	void HandleInvaderFullBlastSessionChanged(bool bIsActive);
	void TryBindInvaderGameState();
	void StopBindInvaderGameStateRetry();
	void SyncFullBlastMenuFromGameState();
	void BuildOfferDefinitionsForSession(const FARInvaderFullBlastSessionState& Session, TArray<FARInvaderUpgradeDefRow>& OutDefinitions) const;
	bool ShouldDisplayFullBlastMenuForSession(const FARInvaderFullBlastSessionState& Session) const;
	bool IsChooserForSession(const FARInvaderFullBlastSessionState& Session) const;
	void SyncLegacyShipReferenceFromPawn(APawn* InPawn);
	void ShowOrUpdateFullBlastMenu(const FARInvaderFullBlastSessionState& Session, const TArray<FARInvaderUpgradeDefRow>& OfferDefinitions);
	void CloseFullBlastMenu();

	UPROPERTY(Transient)
	TWeakObjectPtr<AARInvaderGameState> BoundInvaderGameState;

	UPROPERTY(Transient)
	TObjectPtr<UARInvaderFullBlastMenuWidget> FullBlastMenuWidget = nullptr;

	FTimerHandle BindInvaderGameStateRetryTimer;
	bool bCachedShowMouseCursorForFullBlast = false;
	bool bCapturedInputForFullBlast = false;
};
