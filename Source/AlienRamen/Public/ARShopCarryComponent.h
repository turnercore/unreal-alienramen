/**
 * @file ARShopCarryComponent.h
 * @brief Replicated held-item state for shop-mode player pawns.
 */
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ARShopRamenTypes.h"
#include "ARShopCarryComponent.generated.h"

class AARRamenBowlActor;
class AARRamenMeatActor;
class AActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAROnShopHeldActorChanged, AActor*, NewHeldActor, AActor*, OldHeldActor);

UCLASS(ClassGroup=(AlienRamen), BlueprintType, Blueprintable, meta=(BlueprintSpawnableComponent))
class ALIENRAMEN_API UARShopCarryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UARShopCarryComponent();

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Shop|Carry")
	AActor* GetHeldActor() const { return HeldActor; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Shop|Carry")
	bool HasHeldActor() const { return HeldActor != nullptr; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Shop|Carry")
	AARRamenMeatActor* GetHeldMeatActor() const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Shop|Carry")
	AARRamenBowlActor* GetHeldBowlActor() const;

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop|Carry", meta = (BlueprintAuthorityOnly))
	bool TrySetHeldActor(AActor* NewHeldActor);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop|Carry", meta = (BlueprintAuthorityOnly))
	AActor* ClearHeldActor(bool bDropInWorld = true);

	// Clears authoritative held reference without modifying actor attach/collision state.
	// Use when transferring ownership to another runtime holder in the same frame.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop|Carry", meta = (BlueprintAuthorityOnly))
	AActor* ReleaseHeldActorForTransfer();

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Shop|Carry")
	bool HasCompletedHeldBowl(FARRamenBowlSpec& OutBowlSpec, AARRamenBowlActor*& OutBowlActor) const;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Shop|Carry")
	FAROnShopHeldActorChanged OnHeldActorChanged;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_HeldActor(AActor* OldHeldActor);

private:
	void ApplyHoldPresentation(AActor* ActorToHold) const;
	void ClearHoldPresentation(AActor* ActorToRelease, bool bDropInWorld) const;
	bool IsAuthorityOwner() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Shop|Carry", meta = (AllowPrivateAccess = "true"))
	FName HoldAttachSocketName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Shop|Carry", meta = (AllowPrivateAccess = "true"))
	FVector HoldRelativeLocation = FVector(30.0f, 0.0f, 40.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Shop|Carry", meta = (AllowPrivateAccess = "true"))
	FRotator HoldRelativeRotation = FRotator::ZeroRotator;

	// Replicated release mode used by OnRep_HeldActor when old held actor is cleared.
	// true = world drop/throw (restore collision/physics), false = transfer/consume (keep non-world presentation).
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Alien Ramen|Shop|Carry", meta = (AllowPrivateAccess = "true"))
	bool bLastReleaseDroppedInWorld = true;

	UPROPERTY(ReplicatedUsing = OnRep_HeldActor, BlueprintReadOnly, Category = "Alien Ramen|Shop|Carry", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AActor> HeldActor = nullptr;
};
