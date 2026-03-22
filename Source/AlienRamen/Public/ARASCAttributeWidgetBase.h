/**
 * @file ARASCAttributeWidgetBase.h
 * @brief Generic event-driven ASC attribute widget base.
 */
#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "Blueprint/UserWidget.h"
#include "GameplayEffectTypes.h"
#include "ARASCAttributeWidgetBase.generated.h"

class AActor;
class UAbilitySystemComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAROnASCAttributeWidgetInitializedSignature, UAbilitySystemComponent*, BoundASC);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAROnASCAttributeWidgetDeinitializedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FAROnASCAttributeWidgetTrackedAttributeChangedSignature,
	FName,
	AttributeName,
	float,
	NewValue,
	float,
	OldValue);

/**
 * Reusable ASC-driven widget base for reacting to gameplay attribute changes without polling.
 *
 * Usage:
 * - Initialize directly from an ASC (`InitializeFromASC`) or from an actor implementing `IAbilitySystemInterface` (`InitializeFromActor`).
 * - Subclasses declare tracked attributes by overriding `BuildTrackedAttributeDefinitions`.
 * - Widget automatically unregisters all delegates in `DeinitializeASCAttributeWidget` and `NativeDestruct`.
 */
UCLASS(Abstract, Blueprintable)
class ALIENRAMEN_API UARASCAttributeWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Binds this widget directly to a specific ASC and starts attribute delegate tracking. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|UI|ASC Attributes")
	void InitializeFromASC(UAbilitySystemComponent* InASC);

	/**
	 * Binds this widget from an actor source.
	 * The actor must implement `IAbilitySystemInterface` and return a valid ASC.
	 */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|UI|ASC Attributes")
	void InitializeFromActor(AActor* InActor);

	/** Unbinds all ASC delegates, source-actor lifecycle binding, and cached attribute state. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|UI|ASC Attributes")
	void DeinitializeASCAttributeWidget();

	/**
	 * Re-resolves ASC from the currently bound source actor and rebinds if it changed.
	 * Useful when actor/ASC ownership is rebuilt during runtime.
	 */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|UI|ASC Attributes")
	bool RefreshResolvedASCBinding();

	/** Attempts to initialize from `GetOwningPlayerPawn()`. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|UI|ASC Attributes")
	bool TryBindOwningPawn();

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|UI|ASC Attributes")
	UAbilitySystemComponent* GetBoundASC() const { return BoundASC.Get(); }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|UI|ASC Attributes")
	AActor* GetBoundSourceActor() const { return BoundSourceActor.Get(); }

	/** Returns a cached tracked attribute value by attribute name. */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|UI|ASC Attributes")
	bool GetTrackedAttributeValue(FName AttributeName, float& OutValue) const;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|UI|ASC Attributes")
	FAROnASCAttributeWidgetInitializedSignature OnASCAttributeWidgetInitialized;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|UI|ASC Attributes")
	FAROnASCAttributeWidgetDeinitializedSignature OnASCAttributeWidgetDeinitialized;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|UI|ASC Attributes")
	FAROnASCAttributeWidgetTrackedAttributeChangedSignature OnASCAttributeWidgetTrackedAttributeChanged;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Called when the widget has successfully bound to an ASC and delegate tracking is active. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|UI|ASC Attributes")
	void BP_OnASCAttributeWidgetInitialized(UAbilitySystemComponent* InASC, AActor* InSourceActor);

	/** Called when ASC/source bindings and tracked state are fully cleared. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|UI|ASC Attributes")
	void BP_OnASCAttributeWidgetDeinitialized();

	/** Generic per-attribute callback for all tracked attributes. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|UI|ASC Attributes")
	void BP_OnASCAttributeTrackedChanged(FName AttributeName, float NewValue, float OldValue);

	/** Auto-bind from owning pawn during `NativeConstruct` when true. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|UI|ASC Attributes")
	bool bAutoBindOwningPawnOnConstruct = false;

	/** Attribute definition produced by subclasses when declaring tracked ASC attributes. */
	struct FARASCTrackedAttributeDefinition
	{
		FGameplayAttribute Attribute;
		FName AttributeName = NAME_None;
	};

	/** Runtime-tracked state for one attribute binding. */
	struct FARASCTrackedAttributeRuntime : FARASCTrackedAttributeDefinition
	{
		FDelegateHandle DelegateHandle;
		float CachedValue = 0.0f;
		bool bHasCachedValue = false;
	};

	/** Subclasses override this to declare which attributes should be tracked. */
	virtual void BuildTrackedAttributeDefinitions(TArray<FARASCTrackedAttributeDefinition>& OutDefinitions) const;

	/** Called after generic dispatch whenever a tracked attribute changes. */
	virtual void HandleTrackedAttributeValueChanged(
		const FARASCTrackedAttributeRuntime& RuntimeState,
		float NewValue,
		float OldValue);

	/** Helper for subclass `BuildTrackedAttributeDefinitions` implementations. */
	void AddTrackedAttributeDefinition(
		TArray<FARASCTrackedAttributeDefinition>& OutDefinitions,
		const FGameplayAttribute& Attribute,
		FName AttributeName = NAME_None) const;

private:
	void InitializeFromASCInternal(UAbilitySystemComponent* InASC);
	void BindSourceActorEndPlay();
	void UnbindSourceActorEndPlay();
	void BindTrackedAttributeDelegates();
	void UnbindTrackedAttributeDelegates();
	void RefreshTrackedAttributeSnapshot(bool bBroadcastSnapshotEvents);
	void BroadcastTrackedAttributeChange(
		const FARASCTrackedAttributeRuntime& RuntimeState,
		float NewValue,
		float OldValue);
	UAbilitySystemComponent* ResolveAbilitySystemComponentFromActor(AActor* InActor) const;
	int32 FindRuntimeIndexForAttribute(const FGameplayAttribute& Attribute) const;
	FName ResolveAttributeName(const FGameplayAttribute& Attribute, FName RequestedName) const;

	void HandleBoundASCAttributeChanged(const FOnAttributeChangeData& ChangeData);

	UFUNCTION()
	void HandleBoundSourceActorEndPlay(AActor* InActor, EEndPlayReason::Type EndPlayReason);

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Alien Ramen|UI|ASC Attributes", meta = (AllowPrivateAccess = "true"))
	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Alien Ramen|UI|ASC Attributes", meta = (AllowPrivateAccess = "true"))
	TWeakObjectPtr<AActor> BoundSourceActor;

	TArray<FARASCTrackedAttributeRuntime> TrackedAttributeRuntimes;
	bool bHasASCAttributeWidgetInitialized = false;
};
