/**
 * @file ARDialogueWidgetBase.h
 * @brief Reusable dialogue UI bridge widget base for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARDialogueTypes.h"
#include "Blueprint/UserWidget.h"
#include "ARDialogueWidgetBase.generated.h"

class AARPlayerController;
class AARNPCCharacterBase;

UCLASS(Abstract, Blueprintable)
class ALIENRAMEN_API UARDialogueWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	// Binds this widget to a specific AR player controller for dialogue updates/input forwarding.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|UI")
	void InitializeDialogueWidget(AARPlayerController* InOwningController);

	// Unbinds current controller and clears local dialogue state cache.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|UI")
	void DeinitializeDialogueWidget();

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|UI")
	void AdvanceDialogue();

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|UI")
	void SubmitChoice(FGuid ChoiceBranchId);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|UI")
	void SetEavesdrop(bool bEnable, EARPlayerSlot TargetSlot);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|UI")
	void SetEavesdropOtherPlayer(bool bEnable);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|UI")
	void StartDialogueWithNpcTag(FGameplayTag NpcTag);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|UI")
	void InteractWithNpc(AARNPCCharacterBase* NpcActor);

	// Toggles local player's dialogue auto-advance preference through the bound controller.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|UI")
	void ToggleAutoAdvance();

	// Submits selected choice when waiting for choice; otherwise advances dialogue.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|UI")
	void AdvanceOrSubmitDialogue();

	// Moves selected choice index by Delta on the bound controller.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|UI")
	void ChoiceDelta(int32 Delta);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Dialogue|UI")
	bool GetCurrentDialogueView(FDialogueClientView& OutView) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Dialogue|UI")
	bool HasActiveDialogueView() const { return bHasActiveDialogueView; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Dialogue|UI")
	AARPlayerController* GetBoundController() const { return BoundController; }

	// BP hook for visual setup when controller binding changes.
	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|Dialogue|UI")
	void BP_OnDialogueWidgetInitialized(AARPlayerController* InOwningController);

	// BP hook to update portrait/text/choices UI when runtime view changes.
	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|Dialogue|UI")
	void BP_OnDialogueViewUpdated(const FDialogueClientView& View);

	// BP hook to close/hide UI when session ends.
	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|Dialogue|UI")
	void BP_OnDialogueSessionEnded(const FString& SessionId);

	// BP hook for cleanup when this widget unbinds from controller.
	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|Dialogue|UI")
	void BP_OnDialogueWidgetDeinitialized();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// Auto-bind to owning AR player controller on construct.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Dialogue|UI")
	bool bAutoBindOwningARPlayerControllerOnConstruct = true;

	// Auto-toggle visibility from dialogue session state (Visible on update, Collapsed on end/deinit).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Dialogue|UI")
	bool bAutoToggleVisibilityFromSessionState = true;

private:
	UFUNCTION()
	void HandleControllerDialogueViewUpdated(const FDialogueClientView& View);

	UFUNCTION()
	void HandleControllerDialogueSessionEnded(const FString& SessionId);

	void BindControllerDelegates();
	void UnbindControllerDelegates();
	void PushInitialViewFromController();

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Alien Ramen|Dialogue|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AARPlayerController> BoundController = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Alien Ramen|Dialogue|UI", meta = (AllowPrivateAccess = "true"))
	FDialogueClientView CurrentDialogueView;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Alien Ramen|Dialogue|UI", meta = (AllowPrivateAccess = "true"))
	bool bHasActiveDialogueView = false;
};
