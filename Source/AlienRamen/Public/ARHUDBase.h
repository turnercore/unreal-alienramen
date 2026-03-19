/**
 * @file ARHUDBase.h
 * @brief Game-side HUD base that extends EmoHUDBase.
 */
#pragma once

#include "CoreMinimal.h"
#include "EmoHUDBase.h"
#include "ARHUDBase.generated.h"

class AGameStateBase;
class APlayerState;
class APlayerController;
class UParleyDialogueWidgetBase;

UCLASS()
class ALIENRAMEN_API AARHUDBase : public AEmoHUDBase
{
	GENERATED_BODY()

public:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void RequestHUDInitialization(APlayerController* SourceController, APlayerState* CurrentPlayerState, AGameStateBase* CurrentGameState) override;

	/** Ensures the HUD-owned dialogue widget exists and is attached to the owning player screen. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|UI", meta = (ToolTip = "Ensures the HUD-owned dialogue widget exists and is attached to the owning player screen."))
	void EnsureDialogueWidget(APlayerController* SourceController, APlayerState* CurrentPlayerState, AGameStateBase* CurrentGameState);

	/** Removes the HUD-owned dialogue widget from the owning player screen. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|UI", meta = (ToolTip = "Removes the HUD-owned dialogue widget from the owning player screen."))
	void RemoveDialogueWidget();

	/** Returns the current HUD-owned dialogue widget instance, if any. */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Dialogue|UI", meta = (ToolTip = "Returns the current HUD-owned dialogue widget instance, if any."))
	UParleyDialogueWidgetBase* GetDialogueWidget() const { return DialogueWidget; }

protected:
	/** Automatically creates the dialogue widget when HUD initialization runs. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Dialogue|UI", meta = (ToolTip = "Automatically creates the dialogue widget when HUD initialization runs."))
	bool bAutoCreateDialogueWidget = false;

	/** Widget class for dialogue presentation and input. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Dialogue|UI", meta = (EditCondition = "bAutoCreateDialogueWidget", ToolTip = "Widget class for dialogue presentation and input."))
	TSubclassOf<UParleyDialogueWidgetBase> DialogueWidgetClass;

	/** Player-screen z-order for the HUD-owned dialogue widget. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Dialogue|UI", meta = (EditCondition = "bAutoCreateDialogueWidget", ToolTip = "Player-screen z-order for the HUD-owned dialogue widget."))
	int32 DialogueWidgetZOrder = 1800;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Alien Ramen|Dialogue|UI", meta = (AllowPrivateAccess = "true", ToolTip = "HUD-owned dialogue widget instance, if created."))
	TObjectPtr<UParleyDialogueWidgetBase> DialogueWidget = nullptr;
};
