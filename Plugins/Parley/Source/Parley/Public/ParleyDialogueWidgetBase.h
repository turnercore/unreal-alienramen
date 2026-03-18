/**
 * @file ParleyDialogueWidgetBase.h
 * @brief Reusable dialogue UI bridge widget base for Parley.
 */
#pragma once

#include "CoreMinimal.h"
#include "ParleyDialogueTypes.h"
#include "Blueprint/UserWidget.h"
#include "ParleyDialogueWidgetBase.generated.h"

class APlayerController;
class AActor;

UCLASS(Abstract, Blueprintable)
class PARLEY_API UParleyDialogueWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	// Binds this widget to a specific AR player controller for dialogue updates/input forwarding.
	UFUNCTION(BlueprintCallable, Category = "Parley|Dialogue|UI", meta = (ToolTip = "Binds this widget to a player controller and primes the cached dialogue view."))
	void InitializeDialogueWidget(APlayerController* InOwningController);

	// Unbinds current controller and clears local dialogue state cache.
	UFUNCTION(BlueprintCallable, Category = "Parley|Dialogue|UI", meta = (ToolTip = "Unbinds the current controller and clears cached dialogue state."))
	void DeinitializeDialogueWidget();

	UFUNCTION(BlueprintCallable, Category = "Parley|Dialogue|UI", meta = (ToolTip = "Requests an advance input through the bound controller."))
	void AdvanceDialogue();

	UFUNCTION(BlueprintCallable, Category = "Parley|Dialogue|UI", meta = (ToolTip = "Requests a choice submission through the bound controller."))
	void SubmitChoice(FGuid ChoiceBranchId);

	UFUNCTION(BlueprintCallable, Category = "Parley|Dialogue|UI", meta = (ToolTip = "Requests eavesdrop state for a target character through the bound controller."))
	void SetEavesdrop(bool bEnable, FGameplayTag TargetCharacterTag);

	UFUNCTION(BlueprintCallable, Category = "Parley|Dialogue|UI", meta = (ToolTip = "Requests eavesdrop state for the other local player through the bound controller."))
	void SetEavesdropOtherPlayer(bool bEnable);

	UFUNCTION(BlueprintCallable, Category = "Parley|Dialogue|UI", meta = (ToolTip = "Requests a dialogue start for the specified speaker tag through the bound controller."))
	void StartDialogueWithSpeakerTag(FGameplayTag SpeakerTag);

	UFUNCTION(BlueprintCallable, Category = "Parley|Interaction|UI", meta = (ToolTip = "Forwards interaction input for the specified actor through the bound controller."))
	void InteractWithCharacter(AActor* CharacterActor);

	// Toggles local player's dialogue auto-advance preference through the bound controller.
	UFUNCTION(BlueprintCallable, Category = "Parley|Dialogue|UI", meta = (ToolTip = "Toggles the bound controller's dialogue auto-advance preference."))
	void ToggleAutoAdvance();

	// Submits selected choice when waiting for choice; otherwise advances dialogue.
	UFUNCTION(BlueprintCallable, Category = "Parley|Dialogue|UI", meta = (ToolTip = "Advances dialogue or submits the active choice, depending on session state."))
	void AdvanceOrSubmitDialogue();

	// Moves selected choice index by Delta on the bound controller.
	UFUNCTION(BlueprintCallable, Category = "Parley|Dialogue|UI", meta = (ToolTip = "Moves the active dialogue choice selection by the supplied delta."))
	void ChoiceDelta(int32 Delta);

	UFUNCTION(BlueprintPure, Category = "Parley|Dialogue|UI", meta = (ToolTip = "Returns cached dialogue widget state without mutating runtime data."))
	bool GetCurrentDialogueView(FDialogueClientView& OutView) const;

	UFUNCTION(BlueprintPure, Category = "Parley|Dialogue|UI", meta = (ToolTip = "Returns cached dialogue widget state without mutating runtime data."))
	bool HasActiveDialogueView() const { return bHasActiveDialogueView; }

	UFUNCTION(BlueprintPure, Category = "Parley|Dialogue|UI", meta = (ToolTip = "Returns cached dialogue widget state without mutating runtime data."))
	APlayerController* GetBoundController() const { return BoundController; }

	// BP hook for visual setup when controller binding changes.
	UFUNCTION(BlueprintImplementableEvent, Category = "Parley|Dialogue|UI", meta = (ToolTip = "Blueprint hook fired after controller binding is established."))
	void BP_OnDialogueWidgetInitialized(APlayerController* InOwningController);

	// BP hook to update portrait/text/choices UI when runtime view changes.
	UFUNCTION(BlueprintImplementableEvent, Category = "Parley|Dialogue|UI", meta = (ToolTip = "Blueprint hook fired when the cached dialogue view changes."))
	void BP_OnDialogueViewUpdated(const FDialogueClientView& View);

	// BP hook to close/hide UI when session ends.
	UFUNCTION(BlueprintImplementableEvent, Category = "Parley|Dialogue|UI", meta = (ToolTip = "Blueprint hook fired when the active dialogue session ends."))
	void BP_OnDialogueSessionEnded(const FString& SessionId);

	// BP hook for cleanup when this widget unbinds from controller.
	UFUNCTION(BlueprintImplementableEvent, Category = "Parley|Dialogue|UI", meta = (ToolTip = "Blueprint hook fired after controller binding is removed."))
	void BP_OnDialogueWidgetDeinitialized();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// Auto-bind to owning AR player controller on construct.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parley|Dialogue|UI", meta = (ToolTip = "Configures automatic dialogue widget binding and visibility behavior."))
	bool bAutoBindOwningPlayerControllerOnConstruct = true;

	// Auto-toggle visibility from dialogue session state (Visible on update, Collapsed on end/deinit).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parley|Dialogue|UI", meta = (ToolTip = "Configures automatic dialogue widget binding and visibility behavior."))
	bool bAutoToggleVisibilityFromSessionState = true;

private:
	UFUNCTION()
	void HandleControllerDialogueViewUpdated(const FDialogueClientView& View);

	UFUNCTION()
	void HandleControllerDialogueSessionEnded(const FString& SessionId);

	void BindControllerDelegates();
	void UnbindControllerDelegates();
	void PushInitialViewFromController();
	void ClearCachedDialogueView(bool bCollapseVisibility);

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Parley|Dialogue|UI", meta = (AllowPrivateAccess = "true", ToolTip = "Runtime dialogue widget state cached for Blueprint UI reads."))
	TObjectPtr<APlayerController> BoundController = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Parley|Dialogue|UI", meta = (AllowPrivateAccess = "true", ToolTip = "Runtime dialogue widget state cached for Blueprint UI reads."))
	FDialogueClientView CurrentDialogueView;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Parley|Dialogue|UI", meta = (AllowPrivateAccess = "true", ToolTip = "Runtime dialogue widget state cached for Blueprint UI reads."))
	bool bHasActiveDialogueView = false;
};
