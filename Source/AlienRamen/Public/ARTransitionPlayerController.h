/**
 * @file ARTransitionPlayerController.h
 * @brief Transition-map controller entrypoint for continue-ready voting.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARPlayerController.h"
#include "ARTransitionPlayerController.generated.h"

class UUserWidget;

UCLASS(Abstract)
class ALIENRAMEN_API AARTransitionPlayerController : public AARPlayerController
{
	GENERATED_BODY()

public:
	AARTransitionPlayerController();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Toggle this player's ready state in the transition map. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Transition")
	void RequestTransitionContinue(bool bReady = true);

	/** Creates (or reuses) the resolved transition widget and adds it to the local viewport. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Transition|UI")
	UUserWidget* ShowTransitionWidgetFromContext();

	/** Creates (or reuses) the provided transition widget class and adds it to the local viewport. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Transition|UI")
	UUserWidget* ShowTransitionWidget(TSubclassOf<UUserWidget> WidgetClass);

	/** Removes the current transition widget instance from viewport if present. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Transition|UI")
	void HideTransitionWidget();

	/** Returns the currently resolved transition widget class for this map/context. */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Transition|UI")
	TSubclassOf<UUserWidget> ResolveTransitionWidgetClassFromContext() const;

	UFUNCTION(Server, Reliable)
	void ServerRequestTransitionContinue(bool bReady = true);

protected:
	/** Automatically creates and adds transition widget in BeginPlay on local controllers. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Transition|UI")
	bool bAutoCreateTransitionWidget = true;

	/** When true, transition controllers enforce UI-only input mode + visible cursor while active. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Transition|UI")
	bool bAutoApplyTransitionInputMode = true;

	/** If true, EndPlay restores gameplay input mode on this local controller. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Transition|UI", meta = (EditCondition = "bAutoApplyTransitionInputMode"))
	bool bRestoreGameplayInputModeOnEndPlay = true;

	/** Default transition widget class when no context-specific override applies. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Transition|UI")
	TSubclassOf<UUserWidget> DefaultTransitionWidgetClass;

	/** Optional override widget class for fresh load entry transition context. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Transition|UI")
	TSubclassOf<UUserWidget> FreshLoadTransitionWidgetClass;

	/** Optional widget overrides keyed by transition reason. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Transition|UI")
	TMap<EARTransitionReason, TSubclassOf<UUserWidget>> TransitionWidgetClassByReason;

	/** Optional widget overrides keyed by transition source mode. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Transition|UI")
	TMap<EARTransitionSourceMode, TSubclassOf<UUserWidget>> TransitionWidgetClassBySourceMode;

	/** Viewport z-order for transition widget instances. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Transition|UI")
	int32 TransitionWidgetZOrder = 1900;

private:
	void ApplyTransitionInputMode(bool bEnable, UUserWidget* FocusWidget = nullptr);

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> TransitionWidget = nullptr;
};

