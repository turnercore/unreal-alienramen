/**
 * @file ARScrapyardHUDWidgetBase.h
 * @brief Reusable widget bridge for Scrapyard HUD runtime state.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARRunBuffTypes.h"
#include "ARScrapyardTypes.h"
#include "Blueprint/UserWidget.h"
#include "ARScrapyardHUDWidgetBase.generated.h"

class AARPlayerController;
class AARScrapyardGameState;
class AARScrapyardHUD;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAROnScrapyardWidgetExtractionSummaryChangedSignature, const FARScrapyardExtractionSummary&, Summary);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAROnScrapyardWidgetRunTimerChangedSignature, float, RemainingSeconds);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAROnScrapyardWidgetRunActiveChangedSignature, bool, bIsRunActive);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAROnScrapyardWidgetRunBuffStateChangedSignature, const FARRunBuffStateSnapshot&, Snapshot);

UCLASS(Abstract, Blueprintable)
class ALIENRAMEN_API UARScrapyardHUDWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Scrapyard|UI")
	void InitializeScrapyardHUDWidget(AARScrapyardHUD* InScrapyardHUD);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Scrapyard|UI")
	void DeinitializeScrapyardHUDWidget();

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Scrapyard|UI")
	bool TryBindOwningScrapyardHUD();

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Scrapyard|UI")
	AARScrapyardHUD* GetBoundScrapyardHUD() const { return BoundScrapyardHUD.Get(); }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Scrapyard|UI")
	bool GetCurrentExtractionSummary(FARScrapyardExtractionSummary& OutSummary) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Scrapyard|UI")
	bool GetCurrentRunRemainingSeconds(float& OutRemainingSeconds) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Scrapyard|UI")
	bool GetCurrentRunActive(bool& OutRunActive) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Scrapyard|UI")
	bool GetCurrentRunBuffStateSnapshot(FARRunBuffStateSnapshot& OutSnapshot) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Scrapyard|UI")
	bool HasCurrentExtractionSummary() const { return bHasCurrentExtractionSummary; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Scrapyard|UI")
	bool HasCurrentRunTimer() const { return bHasCurrentRunTimer; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Scrapyard|UI")
	bool HasCurrentRunActive() const { return bHasCurrentRunActive; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Scrapyard|UI")
	bool HasCurrentRunBuffStateSnapshot() const { return bHasCurrentRunBuffStateSnapshot; }

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Scrapyard|UI")
	FAROnScrapyardWidgetExtractionSummaryChangedSignature OnScrapyardWidgetExtractionSummaryChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Scrapyard|UI")
	FAROnScrapyardWidgetRunTimerChangedSignature OnScrapyardWidgetRunTimerChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Scrapyard|UI")
	FAROnScrapyardWidgetRunActiveChangedSignature OnScrapyardWidgetRunActiveChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Scrapyard|UI")
	FAROnScrapyardWidgetRunBuffStateChangedSignature OnScrapyardWidgetRunBuffStateChanged;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|Scrapyard|UI")
	void BP_OnScrapyardHUDWidgetInitialized(
		AARScrapyardHUD* InScrapyardHUD,
		AARPlayerController* InSourceController,
		AARScrapyardGameState* InScrapyardGameState);

	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|Scrapyard|UI")
	void BP_OnScrapyardHUDWidgetDeinitialized();

	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|Scrapyard|UI")
	void BP_OnScrapyardWidgetExtractionSummaryChanged(const FARScrapyardExtractionSummary& Summary);

	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|Scrapyard|UI")
	void BP_OnScrapyardWidgetRunTimerChanged(float RemainingSeconds);

	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|Scrapyard|UI")
	void BP_OnScrapyardWidgetRunActiveChanged(bool bIsRunActive);

	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|Scrapyard|UI")
	void BP_OnScrapyardWidgetRunBuffStateChanged(const FARRunBuffStateSnapshot& Snapshot);

	// Attempts to bind this widget to owning-player Scrapyard HUD in NativeConstruct.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Scrapyard|UI")
	bool bAutoBindOwningScrapyardHUDOnConstruct = true;

private:
	void BindScrapyardHUDDelegates();
	void UnbindScrapyardHUDDelegates();
	void RefreshCachedStateFromHUD();

	UFUNCTION()
	void HandleScrapyardExtractionSummaryChanged(const FARScrapyardExtractionSummary& Summary);

	UFUNCTION()
	void HandleScrapyardRunTimerChanged(float RemainingSeconds);

	UFUNCTION()
	void HandleScrapyardRunActiveChanged(bool bIsRunActive);

	UFUNCTION()
	void HandleRunBuffStateChanged(const FARRunBuffStateSnapshot& Snapshot);

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Alien Ramen|Scrapyard|UI", meta = (AllowPrivateAccess = "true"))
	TWeakObjectPtr<AARScrapyardHUD> BoundScrapyardHUD;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Alien Ramen|Scrapyard|UI", meta = (AllowPrivateAccess = "true"))
	FARScrapyardExtractionSummary CurrentExtractionSummary;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Alien Ramen|Scrapyard|UI", meta = (AllowPrivateAccess = "true"))
	float CurrentRunRemainingSeconds = 0.0f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Alien Ramen|Scrapyard|UI", meta = (AllowPrivateAccess = "true"))
	bool bCurrentRunActive = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Alien Ramen|Scrapyard|UI", meta = (AllowPrivateAccess = "true"))
	FARRunBuffStateSnapshot CurrentRunBuffStateSnapshot;

	UPROPERTY(Transient)
	bool bHasCurrentExtractionSummary = false;

	UPROPERTY(Transient)
	bool bHasCurrentRunTimer = false;

	UPROPERTY(Transient)
	bool bHasCurrentRunActive = false;

	UPROPERTY(Transient)
	bool bHasCurrentRunBuffStateSnapshot = false;
};
