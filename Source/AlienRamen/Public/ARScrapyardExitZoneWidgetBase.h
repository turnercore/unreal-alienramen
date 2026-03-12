/**
 * @file ARScrapyardExitZoneWidgetBase.h
 * @brief Reusable widget bridge for a single Scrapyard exit zone.
 */
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ARScrapyardExitZoneWidgetBase.generated.h"

class AARScrapyardExitZoneActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FAROnObservedScrapyardExitZoneChangedSignature,
	AARScrapyardExitZoneActor*,
	ObservedExitZone,
	int32,
	DepositedReservedScrapValue);

UCLASS(Abstract, Blueprintable)
class ALIENRAMEN_API UARScrapyardExitZoneWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Scrapyard|UI|Exit")
	void InitializeScrapyardExitZoneWidget(AARScrapyardExitZoneActor* InObservedExitZone);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Scrapyard|UI|Exit")
	void DeinitializeScrapyardExitZoneWidget();

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Scrapyard|UI|Exit")
	void SetObservedExitZone(AARScrapyardExitZoneActor* InObservedExitZone);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Scrapyard|UI|Exit")
	AARScrapyardExitZoneActor* GetObservedExitZone() const { return ObservedExitZone.Get(); }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Scrapyard|UI|Exit")
	int32 GetCachedObservedExitReservedScrapValue() const { return CachedObservedExitReservedScrapValue; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Scrapyard|UI|Exit")
	bool HasCachedObservedExitReservedScrapValue() const { return bHasCachedObservedExitReservedScrapValue; }

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Scrapyard|UI|Exit")
	FAROnObservedScrapyardExitZoneChangedSignature OnObservedExitZoneChanged;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|Scrapyard|UI|Exit")
	void BP_OnScrapyardExitZoneWidgetInitialized(AARScrapyardExitZoneActor* InObservedExitZone);

	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|Scrapyard|UI|Exit")
	void BP_OnScrapyardExitZoneWidgetDeinitialized();

	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|Scrapyard|UI|Exit")
	void BP_OnObservedExitZoneChanged(AARScrapyardExitZoneActor* InObservedExitZone, int32 DepositedReservedScrapValue);

	// Optional default zone for auto-binding during NativeConstruct.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Scrapyard|UI|Exit", meta = (ExposeOnSpawn = "true"))
	TObjectPtr<AARScrapyardExitZoneActor> InitialObservedExitZone = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Scrapyard|UI|Exit")
	bool bAutoObserveInitialExitZoneOnConstruct = true;

private:
	void BindObservedExitZone();
	void UnbindObservedExitZone();
	void RefreshCachedObservedExitZoneValue();

	UFUNCTION()
	void HandleObservedExitZoneChanged();

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Alien Ramen|Scrapyard|UI|Exit", meta = (AllowPrivateAccess = "true"))
	TWeakObjectPtr<AARScrapyardExitZoneActor> ObservedExitZone;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Alien Ramen|Scrapyard|UI|Exit", meta = (AllowPrivateAccess = "true"))
	int32 CachedObservedExitReservedScrapValue = 0;

	UPROPERTY(Transient)
	bool bHasCachedObservedExitReservedScrapValue = false;
};
