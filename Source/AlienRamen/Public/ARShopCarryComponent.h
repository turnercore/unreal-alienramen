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

	/** Returns the actor currently held by this component (authoritative copy). */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Shop|Carry")
	AActor* GetHeldActor() const { return HeldActor; }

	/** Convenience check: true when any actor is currently held. */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Shop|Carry")
	bool HasHeldActor() const { return HeldActor != nullptr; }

	/** Returns the held actor cast as meat when valid; nullptr otherwise. */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Shop|Carry")
	AARRamenMeatActor* GetHeldMeatActor() const;

	/** Returns the held actor cast as ramen bowl when valid; nullptr otherwise. */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Shop|Carry")
	AARRamenBowlActor* GetHeldBowlActor() const;

	/**
	 * Set a new held actor on authority. Intended entry point when a pickup request succeeds.
	 * Automatically drops any existing held actor first.
	 */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop|Carry", meta = (BlueprintAuthorityOnly))
	bool TrySetHeldActor(AActor* NewHeldActor);

	/**
	 * Clears the held actor on authority and optionally drops it into the world (restores collision/physics).
	 * Set `bDropInWorld=false` when handing off to another component or consuming the item in the same frame.
	 */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop|Carry", meta = (BlueprintAuthorityOnly))
	AActor* ClearHeldActor(bool bDropInWorld = true);

	// Clears authoritative held reference without modifying actor attach/collision state.
	// Use when transferring ownership to another runtime holder in the same frame.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop|Carry", meta = (BlueprintAuthorityOnly))
	AActor* ReleaseHeldActorForTransfer();

	/**
	 * Returns true when the held bowl is fully completed and supplies the resolved bowl spec + actor.
	 * Useful for serve/turn-in flows that only accept completed bowls.
	 */
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

	/** Socket on the owning pawn to attach held items to (e.g., a hand). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Shop|Carry", meta = (AllowPrivateAccess = "true"))
	FName HoldAttachSocketName = NAME_None;

	/** Local offset from the attach socket used to position the held actor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Shop|Carry", meta = (AllowPrivateAccess = "true"))
	FVector HoldRelativeLocation = FVector(30.0f, 0.0f, 40.0f);

	/** Local rotation applied after attachment; tweak to align visuals. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Shop|Carry", meta = (AllowPrivateAccess = "true"))
	FRotator HoldRelativeRotation = FRotator::ZeroRotator;

	// Replicated release mode used by OnRep_HeldActor when old held actor is cleared.
	// true = world drop/throw (restore collision/physics), false = transfer/consume (keep non-world presentation).
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Alien Ramen|Shop|Carry", meta = (AllowPrivateAccess = "true"))
	bool bLastReleaseDroppedInWorld = true;

	UPROPERTY(ReplicatedUsing = OnRep_HeldActor, BlueprintReadOnly, Category = "Alien Ramen|Shop|Carry", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AActor> HeldActor = nullptr;
};
