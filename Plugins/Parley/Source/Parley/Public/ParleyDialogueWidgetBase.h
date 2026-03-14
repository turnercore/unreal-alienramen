/**
 * @file ParleyDialogueWidgetBase.h
 * @brief Reusable dialogue UI bridge widget base for Alien Ramen.
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
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|UI", meta = (ToolTip = "Executes dialogue widget interaction or control behavior."))
	void InitializeDialogueWidget(APlayerController* InOwningController);

	// Unbinds current controller and clears local dialogue state cache.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|UI", meta = (ToolTip = "Executes dialogue widget interaction or control behavior."))
	void DeinitializeDialogueWidget();

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|UI", meta = (ToolTip = "Executes dialogue widget interaction or control behavior."))
	void AdvanceDialogue();

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|UI", meta = (ToolTip = "Executes dialogue widget interaction or control behavior."))
	void SubmitChoice(FGuid ChoiceBranchId);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|UI", meta = (ToolTip = "Executes dialogue widget interaction or control behavior."))
	void SetEavesdrop(bool bEnable, FGameplayTag TargetSlotTag);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|UI", meta = (ToolTip = "Executes dialogue widget interaction or control behavior."))
	void SetEavesdropOtherPlayer(bool bEnable);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|UI", meta = (ToolTip = "Executes dialogue widget interaction or control behavior."))
	void StartDialogueWithSpeakerTag(FGameplayTag SpeakerTag);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Interaction|UI", meta = (ToolTip = "Executes interaction UI behavior through the dialogue widget bridge."))
	void InteractWithCharacter(AActor* CharacterActor);

	// Toggles local player's dialogue auto-advance preference through the bound controller.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|UI", meta = (ToolTip = "Executes dialogue widget interaction or control behavior."))
	void ToggleAutoAdvance();

	// Submits selected choice when waiting for choice; otherwise advances dialogue.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|UI", meta = (ToolTip = "Executes dialogue widget interaction or control behavior."))
	void AdvanceOrSubmitDialogue();

	// Moves selected choice index by Delta on the bound controller.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|UI", meta = (ToolTip = "Executes dialogue widget interaction or control behavior."))
	void ChoiceDelta(int32 Delta);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Dialogue|UI", meta = (ToolTip = "Returns dialogue widget state without mutating runtime data."))
	bool GetCurrentDialogueView(FDialogueClientView& OutView) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Dialogue|UI", meta = (ToolTip = "Returns dialogue widget state without mutating runtime data."))
	bool HasActiveDialogueView() const { return bHasActiveDialogueView; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Dialogue|UI", meta = (ToolTip = "Returns dialogue widget state without mutating runtime data."))
	APlayerController* GetBoundController() const { return BoundController; }

	// BP hook for visual setup when controller binding changes.
	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|Dialogue|UI", meta = (ToolTip = "Blueprint hook for dialogue widget lifecycle and view updates."))
	void BP_OnDialogueWidgetInitialized(APlayerController* InOwningController);

	// BP hook to update portrait/text/choices UI when runtime view changes.
	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|Dialogue|UI", meta = (ToolTip = "Blueprint hook for dialogue widget lifecycle and view updates."))
	void BP_OnDialogueViewUpdated(const FDialogueClientView& View);

	// BP hook to close/hide UI when session ends.
	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|Dialogue|UI", meta = (ToolTip = "Blueprint hook for dialogue widget lifecycle and view updates."))
	void BP_OnDialogueSessionEnded(const FString& SessionId);

	// BP hook for cleanup when this widget unbinds from controller.
	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|Dialogue|UI", meta = (ToolTip = "Blueprint hook for dialogue widget lifecycle and view updates."))
	void BP_OnDialogueWidgetDeinitialized();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// Auto-bind to owning AR player controller on construct.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Dialogue|UI", meta = (ToolTip = "Configures automatic dialogue widget binding and visibility behavior."))
	bool bAutoBindOwningPlayerControllerOnConstruct = true;

	// Auto-toggle visibility from dialogue session state (Visible on update, Collapsed on end/deinit).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Dialogue|UI", meta = (ToolTip = "Configures automatic dialogue widget binding and visibility behavior."))
	bool bAutoToggleVisibilityFromSessionState = true;

private:
	UFUNCTION()
	void HandleControllerDialogueViewUpdated(const FDialogueClientView& View);

	UFUNCTION()
	void HandleControllerDialogueSessionEnded(const FString& SessionId);

	void BindControllerDelegates();
	void UnbindControllerDelegates();
	void PushInitialViewFromController();

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Alien Ramen|Dialogue|UI", meta = (AllowPrivateAccess = "true", ToolTip = "Runtime dialogue widget state cached for Blueprint UI reads."))
	TObjectPtr<APlayerController> BoundController = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Alien Ramen|Dialogue|UI", meta = (AllowPrivateAccess = "true", ToolTip = "Runtime dialogue widget state cached for Blueprint UI reads."))
	FDialogueClientView CurrentDialogueView;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Alien Ramen|Dialogue|UI", meta = (AllowPrivateAccess = "true", ToolTip = "Runtime dialogue widget state cached for Blueprint UI reads."))
	bool bHasActiveDialogueView = false;
};
