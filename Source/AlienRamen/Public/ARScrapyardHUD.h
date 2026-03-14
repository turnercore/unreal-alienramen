/**
 * @file ARScrapyardHUD.h
 * @brief Scrapyard HUD base class with local UI binding surfaces.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARHUDBase.h"
#include "ARRunBuffTypes.h"
#include "ARScrapyardTypes.h"
#include "ARScrapyardHUD.generated.h"

class AARPlayerController;
class AARScrapyardGameState;
class AGameStateBase;
class APlayerController;
class APlayerState;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAROnScrapyardHUDExtractionSummaryChangedSignature, const FARScrapyardExtractionSummary&, Summary);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAROnScrapyardHUDRunTimerChangedSignature, float, RemainingSeconds);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAROnScrapyardHUDRunActiveChangedSignature, bool, bIsRunActive);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAROnScrapyardHUDRunBuffStateChangedSignature, const FARRunBuffStateSnapshot&, Snapshot);

UCLASS()
class ALIENRAMEN_API AARScrapyardHUD : public AARHUDBase
{
	GENERATED_BODY()

public:
	virtual void RequestHUDInitialization(APlayerController* SourceController, APlayerState* CurrentPlayerState, AGameStateBase* CurrentGameState) override;

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Scrapyard|UI")
	void InitializeScrapyardHUD(AARPlayerController* SourceController, AARScrapyardGameState* CurrentScrapyardGameState);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Scrapyard|UI")
	void DeinitializeScrapyardHUD();

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Scrapyard|UI")
	AARPlayerController* GetBoundPlayerController() const { return BoundPlayerController.Get(); }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Scrapyard|UI")
	AARScrapyardGameState* GetBoundScrapyardGameState() const { return BoundScrapyardGameState.Get(); }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Scrapyard|UI")
	bool GetCachedExtractionSummary(FARScrapyardExtractionSummary& OutSummary) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Scrapyard|UI")
	bool GetCachedRunRemainingSeconds(float& OutRemainingSeconds) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Scrapyard|UI")
	bool GetCachedRunActive(bool& OutRunActive) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Scrapyard|UI")
	bool GetCachedRunBuffStateSnapshot(FARRunBuffStateSnapshot& OutSnapshot) const;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Scrapyard|UI")
	FAROnScrapyardHUDExtractionSummaryChangedSignature OnScrapyardHUDExtractionSummaryChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Scrapyard|UI")
	FAROnScrapyardHUDRunTimerChangedSignature OnScrapyardHUDRunTimerChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Scrapyard|UI")
	FAROnScrapyardHUDRunActiveChangedSignature OnScrapyardHUDRunActiveChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Scrapyard|UI")
	FAROnScrapyardHUDRunBuffStateChangedSignature OnScrapyardHUDRunBuffStateChanged;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|Scrapyard|UI")
	void BP_OnScrapyardHUDInitialized(AARPlayerController* SourceController, AARScrapyardGameState* CurrentScrapyardGameState);

	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|Scrapyard|UI")
	void BP_OnScrapyardHUDDeinitialized();

	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|Scrapyard|UI")
	void BP_OnScrapyardExtractionSummaryChanged(const FARScrapyardExtractionSummary& Summary);

	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|Scrapyard|UI")
	void BP_OnScrapyardRunTimerChanged(float RemainingSeconds);

	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|Scrapyard|UI")
	void BP_OnScrapyardRunActiveChanged(bool bIsRunActive);

	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|Scrapyard|UI")
	void BP_OnScrapyardRunBuffStateChanged(const FARRunBuffStateSnapshot& Snapshot);

private:
	void BindScrapyardGameState(AARScrapyardGameState* InGameState);
	void UnbindScrapyardGameState();
	void RefreshCachedState();

	UFUNCTION()
	void HandleScrapyardExtractionSummaryChanged(const FARScrapyardExtractionSummary& Summary);

	UFUNCTION()
	void HandleScrapyardRunTimerChanged(float RemainingSeconds);

	UFUNCTION()
	void HandleScrapyardRunActiveChanged(bool bIsRunActive);

	UFUNCTION()
	void HandleRunBuffStateChanged(const FARRunBuffStateSnapshot& Snapshot);

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Alien Ramen|Scrapyard|UI", meta = (AllowPrivateAccess = "true"))
	TWeakObjectPtr<AARPlayerController> BoundPlayerController;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Alien Ramen|Scrapyard|UI", meta = (AllowPrivateAccess = "true"))
	TWeakObjectPtr<AARScrapyardGameState> BoundScrapyardGameState;

	/** Cached summary for widgets that poll instead of binding delegates. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Alien Ramen|Scrapyard|UI", meta = (AllowPrivateAccess = "true"))
	FARScrapyardExtractionSummary CachedExtractionSummary;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Alien Ramen|Scrapyard|UI", meta = (AllowPrivateAccess = "true"))
	float CachedRunRemainingSeconds = 0.0f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Alien Ramen|Scrapyard|UI", meta = (AllowPrivateAccess = "true"))
	bool bCachedRunActive = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Alien Ramen|Scrapyard|UI", meta = (AllowPrivateAccess = "true"))
	FARRunBuffStateSnapshot CachedRunBuffStateSnapshot;

	UPROPERTY(Transient)
	bool bHasCachedExtractionSummary = false;

	UPROPERTY(Transient)
	bool bHasCachedRunTimer = false;

	UPROPERTY(Transient)
	bool bHasCachedRunActive = false;

	UPROPERTY(Transient)
	bool bHasCachedRunBuffStateSnapshot = false;
};
