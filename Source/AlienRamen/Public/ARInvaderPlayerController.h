/**
 * @file ARInvaderPlayerController.h
 * @brief ARInvaderPlayerController header for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARPlayerController.h"
#include "ARInvaderPlayerController.generated.h"

class AARPlayerStateBase;

UCLASS()
class ALIENRAMEN_API AARInvaderPlayerController : public AARPlayerController
{
	GENERATED_BODY()

public:
	AARInvaderPlayerController();

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
	// activates the selected cursor tier, or Full Blast when cursor tier is 0.
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
		FVector2D CursorNormalized,
		bool bHasCursor);

	UFUNCTION(Server, Reliable)
	void ServerRequestSetOfferPresence(
		FGameplayTag HoveredUpgradeTag,
		int32 HoveredDestinationSlot,
		FVector2D CursorNormalized,
		bool bHasCursor);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader|Spice Track")
	void RequestClearOfferPresence();

	UFUNCTION(Server, Reliable)
	void ServerRequestClearOfferPresence();

private:
	AARPlayerStateBase* GetInvaderPlayerState() const;
};
