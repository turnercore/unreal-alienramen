/**
 * @file ARScrapyardExitZoneActor.h
 * @brief Scrapyard extraction zone tracking deposited items and in-zone held items.
 */
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ARScrapyardExitZoneActor.generated.h"

class AARScrapyardCarryItemBase;
class AARScrapyardPlayerController;
class AARScrapyardGameState;
class AARPlayerStateBase;
class UBoxComponent;
class UPrimitiveComponent;
class FLifetimeProperty;
struct FHitResult;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAROnScrapyardExitZoneChangedSignature);

UCLASS()
class ALIENRAMEN_API AARScrapyardExitZoneActor : public AActor
{
	GENERATED_BODY()

public:
	AARScrapyardExitZoneActor();

	/** Deposits currently held scrapyard item from controller into this exit zone. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Scrapyard|Exit")
	bool TryDepositHeldItem(AARScrapyardPlayerController* Controller);

	/** Withdraws a deposited item back into controller hands and refunds reserved scrap. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Scrapyard|Exit")
	bool TryWithdrawDepositedItem(AARScrapyardPlayerController* Controller, AARScrapyardCarryItemBase* ItemActor);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Scrapyard|Exit", meta = (BlueprintAuthorityOnly))
	TArray<AARScrapyardCarryItemBase*> GetDepositedItems() const;

	const TArray<TObjectPtr<AARScrapyardCarryItemBase>>& GetDepositedItemsRef() const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Scrapyard|Exit")
	bool IsPlayerStateInsideExit(const AARPlayerStateBase* PlayerState) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Scrapyard|Exit")
	int32 GetDepositedReservedScrapValue() const { return DepositedReservedScrapValue; }

	void GatherHeldItemsInZone(TArray<AARScrapyardCarryItemBase*>& OutHeldItems) const;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Scrapyard|Exit")
	FAROnScrapyardExitZoneChangedSignature OnExitZoneChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void HandleExitVolumeBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleExitVolumeEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	AARScrapyardGameState* ResolveScrapyardGameState() const;
	bool IsControllerEligibleForExit(const AARScrapyardPlayerController* Controller) const;
	void PruneInvalidDeposits();
	void RefreshDepositedReservedScrapValue(bool bBroadcast = true);

	UFUNCTION()
	void OnRep_DepositedReservedScrapValue(int32 OldValue);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Scrapyard|Exit")
	TObjectPtr<UBoxComponent> ExitVolume = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Scrapyard|Exit")
	FVector DepositOffset = FVector(0.0f, 0.0f, 40.0f);

	UPROPERTY(Transient)
	TSet<TWeakObjectPtr<AARPlayerStateBase>> PlayerStatesInZone;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AARScrapyardCarryItemBase>> DepositedItems;

	UPROPERTY(ReplicatedUsing = OnRep_DepositedReservedScrapValue, BlueprintReadOnly, Category = "Alien Ramen|Scrapyard|Exit", meta = (AllowPrivateAccess = "true"))
	int32 DepositedReservedScrapValue = 0;
};
