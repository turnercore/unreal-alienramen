/**
 * @file ARInvaderFullBlastMenuWidget.h
 * @brief Base widget bridge for Invader full-blast offer selection UI.
 */
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ARInvaderSpicyTrackTypes.h"
#include "ARInvaderFullBlastMenuWidget.generated.h"

class AARInvaderPlayerController;

UCLASS(Abstract, Blueprintable)
class ALIENRAMEN_API UARInvaderFullBlastMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Called by controller whenever full-blast session data updates.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader|Spice Track|Full Blast")
	void InitializeFullBlastMenu(
		AARInvaderPlayerController* InOwningController,
		const FARInvaderFullBlastSessionState& InSessionState,
		const TArray<FARInvaderUpgradeDefRow>& InOfferDefinitions,
		bool bInIsChooser);

	// Forward selected offer to authoritative game state through owning controller.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader|Spice Track|Full Blast")
	void SubmitSelection(FGameplayTag SelectedUpgradeTag, int32 DesiredDestinationSlot = -1);

	// Forward skip choice to authoritative game state through owning controller.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader|Spice Track|Full Blast")
	void SubmitSkip();

	// Optional live hover/cursor presence publishing during offer session.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader|Spice Track|Full Blast")
	void PublishOfferPresence(
		FGameplayTag HoveredUpgradeTag,
		int32 HoveredDestinationSlot,
		FVector2D CursorNormalized,
		bool bHasCursor);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader|Spice Track|Full Blast")
	void ClearOfferPresence();

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Invader|Spice Track|Full Blast")
	const FARInvaderFullBlastSessionState& GetSessionState() const { return SessionState; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Invader|Spice Track|Full Blast")
	const TArray<FARInvaderUpgradeDefRow>& GetOfferDefinitions() const { return OfferDefinitions; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Invader|Spice Track|Full Blast")
	bool IsChooser() const { return bIsChooser; }

	// BP hook to (re)build cards and update visuals whenever session payload changes.
	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|Invader|Spice Track|Full Blast")
	void BP_OnFullBlastMenuUpdated(
		const FARInvaderFullBlastSessionState& InSessionState,
		const TArray<FARInvaderUpgradeDefRow>& InOfferDefinitions,
		bool bInIsChooser);

	// BP hook called before widget is removed from viewport due to session ending.
	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|Invader|Spice Track|Full Blast")
	void BP_OnFullBlastMenuClosed();

	// Called by controller before removing this widget from viewport.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader|Spice Track|Full Blast")
	void NotifyMenuClosed();

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<AARInvaderPlayerController> OwningInvaderController;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Alien Ramen|Invader|Spice Track|Full Blast", meta = (AllowPrivateAccess = "true"))
	FARInvaderFullBlastSessionState SessionState;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Alien Ramen|Invader|Spice Track|Full Blast", meta = (AllowPrivateAccess = "true"))
	TArray<FARInvaderUpgradeDefRow> OfferDefinitions;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Alien Ramen|Invader|Spice Track|Full Blast", meta = (AllowPrivateAccess = "true"))
	bool bIsChooser = false;
};

