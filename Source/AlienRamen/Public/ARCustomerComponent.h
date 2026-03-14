/**
 * @file ARCustomerComponent.h
 * @brief Server-authoritative customer/order runtime for shop serving.
 */
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "ARShopRamenTypes.h"
#include "ARCustomerComponent.generated.h"

class AARPlayerController;
class APlayerController;
class UARCustomerOrderWidgetBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAROnCustomerOrderChanged, const FARRamenOrderRequest&, NewOrder);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAROnCustomerOrderResolved, const FARRamenServeResult&, ServeResult);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FAROnCustomerOrderGeneratedDetailed, const FARRamenOrderRequest&, NewOrder, int32, OrdersGeneratedCount, int32, OrdersServedCount, int32, RemainingOrdersToGenerate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FAROnCustomerOrderServedDetailed, const FARRamenServeResult&, ServeResult, int32, OrdersGeneratedCount, int32, OrdersServedCount, int32, RemainingOrdersToGenerate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FAROnCustomerDoneOrdering, int32, OrdersGeneratedCount, int32, OrdersServedCount, int32, RemainingOrdersToGenerate);

UCLASS(
	ClassGroup=(AlienRamen),
	BlueprintType,
	Blueprintable,
	meta=(BlueprintSpawnableComponent, DisplayName="Shop Customer Component", ToolTip="Server-authoritative shop customer/order runtime keyed by speaker identity."))
class ALIENRAMEN_API UARCustomerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UARCustomerComponent();

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Shop|Customer")
	bool HasActiveOrder() const { return bHasActiveOrder; }

	// StateTree-friendly alias for gating interaction branches.
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Shop|Customer")
	bool HasOrderForInteraction() const { return bHasActiveOrder; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Shop|Customer")
	const FARRamenOrderRequest& GetActiveOrder() const { return ActiveOrder; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Shop|Customer")
	bool IsPickyExactMatchCustomer() const { return bPickyExactMatch; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Shop|Customer")
	int32 GetOrdersGeneratedCount() const { return OrdersGeneratedCount; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Shop|Customer")
	int32 GetOrdersServedCount() const { return OrdersServedCount; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Shop|Customer")
	bool IsDoneOrdering() const { return bDoneOrdering; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Shop|Customer")
	int32 GetRemainingOrdersToGenerate() const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Shop|Customer")
	FGameplayTag GetSpeakerTag() const;

	// Optional per-customer UI style class used to create an order widget instance.
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Shop|Customer|UI")
	TSubclassOf<UARCustomerOrderWidgetBase> GetOrderWidgetClass() const { return OrderWidgetClass; }

	// Creates the configured order widget class for a player and initializes it from this component.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop|Customer|UI")
	UARCustomerOrderWidgetBase* CreateAndInitializeOrderWidget(APlayerController* OwningPlayer) const;

	// Initializes an existing order widget instance from this component.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop|Customer|UI")
	void InitializeOrderWidget(UARCustomerOrderWidgetBase* WidgetInstance) const;

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

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Shop|Customer")
	FAROnCustomerOrderGeneratedDetailed OnCustomerOrderGeneratedDetailed;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Shop|Customer")
	FAROnCustomerOrderServedDetailed OnCustomerOrderServedDetailed;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Shop|Customer")
	FAROnCustomerDoneOrdering OnCustomerDoneOrdering;

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_ActiveOrder();

	UFUNCTION()
	void OnRep_DoneOrdering(bool bOldDoneOrdering);

private:
	static void NormalizeOrderRequest(FARRamenOrderRequest& InOutOrder, bool bPadToThreeSlots = false);
	bool ResolveDefinitionRow(FARCustomerDefinitionRow& OutRow) const;
	bool SelectOrderForRelationshipLevel(const FARCustomerDefinitionRow& Row, int32 RelationshipLevel, FARRamenOrderRequest& OutOrder) const;
	bool BuildProceduralFallbackOrder(FARRamenOrderRequest& OutOrder) const;
	bool ApplyServeOutcomeToDialogue(const FARRamenServeResult& ServeResult) const;
	bool CanGenerateAdditionalOrders() const;
	void SetDoneOrdering(bool bNewDoneOrdering);
	void UpdateDialogueGateFromOrderState() const;
	void RefreshOrderingEmotionState() const;
	void ApplyOrderingReactionEmotion(const FGameplayTag& ReactionEmotionTag) const;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Alien Ramen|Shop|Customer",
		meta = (AllowPrivateAccess = "true", Categories = "Dialogue.Speaker", DisplayName = "Speaker Tag Override", ToolTip = "Optional shop-specific speaker identity override. When unset, this uses the owning speaker tag from UParleySpeakerComponent."))
	FGameplayTag SpeakerTagOverride;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Shop|Customer|UI", meta = (AllowPrivateAccess = "true", DisplayName = "Order Widget Class", ToolTip = "Optional widget class used for this customer's order display. Must derive from ARCustomerOrderWidgetBase."))
	TSubclassOf<UARCustomerOrderWidgetBase> OrderWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Shop|Customer", meta = (AllowPrivateAccess = "true", ToolTip = "If true, this customer auto-generates an order at BeginPlay on authority."))
	bool bGenerateOrderOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Shop|Customer", meta = (AllowPrivateAccess = "true", ToolTip = "If true, this customer automatically generates the next order after a serve outcome resolves."))
	bool bGenerateNextOrderAfterServe = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Shop|Customer", meta = (AllowPrivateAccess = "true", ClampMin = "0", UIMin = "0", ToolTip = "Total number of orders this customer can generate over its lifetime. 0 means unlimited."))
	int32 MaxOrdersToGenerate = 0;

	UPROPERTY(ReplicatedUsing = OnRep_ActiveOrder, BlueprintReadOnly, Category = "Alien Ramen|Shop|Customer", meta = (AllowPrivateAccess = "true"))
	FARRamenOrderRequest ActiveOrder;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Alien Ramen|Shop|Customer", meta = (AllowPrivateAccess = "true"))
	bool bHasActiveOrder = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Alien Ramen|Shop|Customer", meta = (AllowPrivateAccess = "true"))
	bool bPickyExactMatch = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Alien Ramen|Shop|Customer", meta = (AllowPrivateAccess = "true"))
	int32 OrdersGeneratedCount = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Alien Ramen|Shop|Customer", meta = (AllowPrivateAccess = "true"))
	int32 OrdersServedCount = 0;

	UPROPERTY(ReplicatedUsing = OnRep_DoneOrdering, BlueprintReadOnly, Category = "Alien Ramen|Shop|Customer", meta = (AllowPrivateAccess = "true"))
	bool bDoneOrdering = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Alien Ramen|Shop|Customer", meta = (AllowPrivateAccess = "true", Categories = "Dialogue.Speaker"))
	FGameplayTag CachedSpeakerTag;

	UPROPERTY()
	FGameplayTag HateEmotionOverride;

	UPROPERTY()
	FGameplayTag OkEmotionOverride;

	UPROPERTY()
	FGameplayTag LikeEmotionOverride;

	UPROPERTY()
	FGameplayTag LoveEmotionOverride;
};
