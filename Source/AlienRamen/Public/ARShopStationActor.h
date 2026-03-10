/**
 * @file ARShopStationActor.h
 * @brief Server-authoritative ramen station runtime with slot + processing progress replication.
 */
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameFramework/Actor.h"
#include "ARShopRamenTypes.h"
#include "ARShopStationActor.generated.h"

class AARPlayerController;
class AARRamenMeatActor;
class USceneComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAROnShopStationRuntimeChanged);

UCLASS(Blueprintable)
class ALIENRAMEN_API AARShopStationActor : public AActor
{
	GENERATED_BODY()

public:
	AARShopStationActor();

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Shop|Station")
	EARRamenStationType GetStationType() const { return StationType; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Shop|Station")
	EARRamenStationRuntimeState GetRuntimeState() const { return RuntimeState; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Shop|Station")
	bool IsStationUpgraded() const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Shop|Station")
	int32 GetProcessedStockAmount() const { return ProcessedStockAmount; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Shop|Station")
	EARAffinityColor GetProcessedStockColor() const { return ProcessedStockColor; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Shop|Station")
	float GetProcessingProgress01() const { return ProcessingProgress01; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Shop|Station")
	AARRamenMeatActor* GetSlottedMeatActor() const { return SlottedMeatActor; }

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop|Station", meta = (BlueprintAuthorityOnly))
	bool TryPlaceMeatActor(AARRamenMeatActor* MeatActor);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop|Station", meta = (BlueprintAuthorityOnly))
	bool TryPlaceHeldMeatFromController(AARPlayerController* Controller);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop|Station", meta = (BlueprintAuthorityOnly))
	bool TryPickupSlottedMeatToController(AARPlayerController* Controller);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop|Station", meta = (BlueprintAuthorityOnly))
	bool StartProcessingByController(AARPlayerController* Controller);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop|Station", meta = (BlueprintAuthorityOnly))
	bool StopProcessingByController(AARPlayerController* Controller);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop|Station", meta = (BlueprintAuthorityOnly))
	void StopAllProcessingControllers();

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop|Station", meta = (BlueprintAuthorityOnly))
	bool TryFillHeldBowlFromController(AARPlayerController* Controller);

	// Station->bowl consume endpoint used by bowl fill flow. Drains one processed stock unit.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop|Station", meta = (BlueprintAuthorityOnly))
	bool TryConsumeForBowl(EARRamenStationType RequestedStationType, EARAffinityColor& OutColor);

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Shop|Station")
	FAROnShopStationRuntimeChanged OnRuntimeStateChanged;

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_RuntimeState(EARRamenStationRuntimeState OldState);

	UFUNCTION()
	void OnRep_SlottedMeatActor(AARRamenMeatActor* OldSlottedMeatActor);

	UFUNCTION()
	void OnRep_ProcessedStockAmount(int32 OldProcessedStockAmount);

	UFUNCTION()
	void OnRep_ProcessingState();

private:
	static EARAffinityColor SanitizeColor(EARAffinityColor InColor);
	static class UARShopCarryComponent* ResolveCarryComponentFromController(AARPlayerController* Controller);

	void BroadcastRuntimeChanged();
	void ApplyConfigFromRowIfAvailable();
	bool ConsumeSlottedMeatAndEnterProcessing();
	bool BeginProcessingNoneIfAllowed();
	void CompleteProcessingCycle();
	void RefreshProcessingActiveFlag();
	void AttachSlottedMeatToSlot() const;
	bool HasColoredProcessedStock() const;
	void SetRuntimeState(EARRamenStationRuntimeState NewState);
	int32 ResolveEffectiveMaxStock() const;
	float ResolveEffectiveProcessingDuration() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Shop|Station", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Shop|Station", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> MeatSlotAnchor;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Shop|Station", meta = (AllowPrivateAccess = "true"))
	EARRamenStationType StationType = EARRamenStationType::Noodles;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Shop|Station", meta = (AllowPrivateAccess = "true"))
	bool bStartsUpgraded = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Shop|Station", meta = (AllowPrivateAccess = "true"))
	FGameplayTagContainer RequiredUpgradeTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Shop|Station", meta = (AllowPrivateAccess = "true", ClampMin = "1", UIMin = "1"))
	int32 MaxStock = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Shop|Station", meta = (AllowPrivateAccess = "true", ClampMin = "0.05", UIMin = "0.05"))
	float ProcessingDurationSeconds = 1.5f;

	// Optional config lookup tag. When valid, BeginPlay resolves FARShopStationConfigRow for this station and overrides station config fields.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Shop|Station", meta = (AllowPrivateAccess = "true"))
	FGameplayTag StationConfigTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Shop|Station", meta = (AllowPrivateAccess = "true"))
	bool bResolveConfigFromData = true;

	UPROPERTY(ReplicatedUsing = OnRep_RuntimeState, BlueprintReadOnly, Category = "Alien Ramen|Shop|Station", meta = (AllowPrivateAccess = "true"))
	EARRamenStationRuntimeState RuntimeState = EARRamenStationRuntimeState::Idle;

	UPROPERTY(ReplicatedUsing = OnRep_SlottedMeatActor, BlueprintReadOnly, Category = "Alien Ramen|Shop|Station", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AARRamenMeatActor> SlottedMeatActor = nullptr;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Alien Ramen|Shop|Station", meta = (AllowPrivateAccess = "true"))
	EARAffinityColor PendingProcessColor = EARAffinityColor::None;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Alien Ramen|Shop|Station", meta = (AllowPrivateAccess = "true"))
	int32 PendingProcessAmount = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Alien Ramen|Shop|Station", meta = (AllowPrivateAccess = "true"))
	EARAffinityColor ProcessedStockColor = EARAffinityColor::None;

	UPROPERTY(ReplicatedUsing = OnRep_ProcessedStockAmount, BlueprintReadOnly, Category = "Alien Ramen|Shop|Station", meta = (AllowPrivateAccess = "true"))
	int32 ProcessedStockAmount = 0;

	UPROPERTY(ReplicatedUsing = OnRep_ProcessingState, BlueprintReadOnly, Category = "Alien Ramen|Shop|Station", meta = (AllowPrivateAccess = "true"))
	float ProcessingProgress01 = 0.0f;

	UPROPERTY(ReplicatedUsing = OnRep_ProcessingState, BlueprintReadOnly, Category = "Alien Ramen|Shop|Station", meta = (AllowPrivateAccess = "true"))
	bool bProcessingActive = false;

	// Runtime-only non-replicated hold processors for hold-to-process behavior.
	TSet<TWeakObjectPtr<AARPlayerController>> ActiveProcessingControllers;
};
