/**
 * @file ARCustomerComponent.h
 * @brief Server-authoritative NPC customer/order runtime for shop serving.
 */
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "ARShopRamenTypes.h"
#include "ARCustomerComponent.generated.h"

class AARPlayerController;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAROnCustomerOrderChanged, const FARRamenOrderRequest&, NewOrder);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAROnCustomerOrderResolved, const FARRamenServeResult&, ServeResult);

UCLASS(ClassGroup=(AlienRamen), BlueprintType, Blueprintable, meta=(BlueprintSpawnableComponent))
class ALIENRAMEN_API UARCustomerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UARCustomerComponent();

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Shop|Customer")
	bool HasActiveOrder() const { return bHasActiveOrder; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Shop|Customer")
	const FARRamenOrderRequest& GetActiveOrder() const { return ActiveOrder; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Shop|Customer")
	bool IsPickyExactMatchCustomer() const { return bPickyExactMatch; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Shop|Customer")
	FGameplayTag GetNpcIdentityTag() const;

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop|Customer", meta = (BlueprintAuthorityOnly))
	bool GenerateNextOrder();

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop|Customer", meta = (BlueprintAuthorityOnly))
	void ClearActiveOrder();

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop|Customer", meta = (BlueprintAuthorityOnly))
	bool TryServeBowl(AARPlayerController* InteractingController, const FARRamenBowlSpec& ServedBowl, FARRamenServeResult& OutResult);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop|Customer", meta = (BlueprintAuthorityOnly))
	bool TryServeHeldBowlFromController(AARPlayerController* InteractingController, FARRamenServeResult& OutResult);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Shop|Customer")
	FGameplayTag ResolveReactionEmotionTag(EARRamenTasteReaction Reaction) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Shop|Customer")
	int32 ResolveReactionRelationshipDelta(EARRamenTasteReaction Reaction) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Shop|Customer")
	static FARRamenServeResult EvaluateServeResult(
		const FARRamenOrderRequest& RequestedOrder,
		const FARRamenBowlSpec& ServedBowl,
		bool bUsePickyExactRule,
		int32 HatePoints = 0,
		int32 OkPoints = 1,
		int32 LikePoints = 3,
		int32 LovePoints = 5);

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Shop|Customer")
	FAROnCustomerOrderChanged OnCustomerOrderChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Shop|Customer")
	FAROnCustomerOrderResolved OnCustomerOrderResolved;

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_ActiveOrder();

private:
	static void NormalizeOrderRequest(FARRamenOrderRequest& InOutOrder, bool bPadToThreeSlots = false);
	bool ResolveDefinitionRow(FARCustomerDefinitionRow& OutRow) const;
	bool SelectOrderForRelationshipLevel(const FARCustomerDefinitionRow& Row, int32 RelationshipLevel, FARRamenOrderRequest& OutOrder) const;
	bool BuildProceduralFallbackOrder(FARRamenOrderRequest& OutOrder) const;
	bool ApplyServeOutcomeToDialogue(const FARRamenServeResult& ServeResult) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Shop|Customer", meta = (AllowPrivateAccess = "true", Categories = "NPC.Identity"))
	FGameplayTag NpcIdentityTagOverride;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Shop|Customer", meta = (AllowPrivateAccess = "true"))
	bool bGenerateOrderOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Shop|Customer", meta = (AllowPrivateAccess = "true"))
	bool bGenerateNextOrderAfterServe = true;

	UPROPERTY(ReplicatedUsing = OnRep_ActiveOrder, BlueprintReadOnly, Category = "Alien Ramen|Shop|Customer", meta = (AllowPrivateAccess = "true"))
	FARRamenOrderRequest ActiveOrder;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Alien Ramen|Shop|Customer", meta = (AllowPrivateAccess = "true"))
	bool bHasActiveOrder = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Alien Ramen|Shop|Customer", meta = (AllowPrivateAccess = "true"))
	bool bPickyExactMatch = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Alien Ramen|Shop|Customer", meta = (AllowPrivateAccess = "true", Categories = "NPC.Identity"))
	FGameplayTag CachedNpcIdentityTag;

	UPROPERTY()
	FGameplayTag HateEmotionOverride;

	UPROPERTY()
	FGameplayTag OkEmotionOverride;

	UPROPERTY()
	FGameplayTag LikeEmotionOverride;

	UPROPERTY()
	FGameplayTag LoveEmotionOverride;
};
