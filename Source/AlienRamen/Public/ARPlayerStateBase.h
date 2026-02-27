#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "StructSerializable.h"
#include "ARPlayerStateBase.generated.h"

class UAbilitySystemComponent;
class UAttributeSet;
class UARAttributeSetCore;

UENUM(BlueprintType)
enum class EARCoreAttributeType : uint8
{
	Health,
	MaxHealth,
	Spice,
	MaxSpice,
	MoveSpeed
};

USTRUCT(BlueprintType)
struct FARPlayerCoreAttributeSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|Player|Attributes")
	float Health = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|Player|Attributes")
	float MaxHealth = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|Player|Attributes")
	float Spice = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|Player|Attributes")
	float MaxSpice = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|Player|Attributes")
	float MoveSpeed = 0.f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FAROnCoreAttributeChangedSignature,
	EARCoreAttributeType,
	AttributeType,
	float,
	NewValue,
	float,
	OldValue);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAROnScalarAttributeChangedSignature, float, NewValue, float, OldValue);

UCLASS()
class ALIENRAMEN_API AARPlayerStateBase : public APlayerState, public IAbilitySystemInterface, public IStructSerializable
{
	GENERATED_BODY()

public:
	AARPlayerStateBase();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UAbilitySystemComponent* GetASC() const { return AbilitySystemComponent; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Player|Attributes")
	float GetCoreAttributeValue(EARCoreAttributeType AttributeType) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Player|Attributes")
	FARPlayerCoreAttributeSnapshot GetCoreAttributeSnapshot() const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Player|Attributes")
	float GetSpiceNormalized() const;

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Player|Attributes")
	void SetSpiceMeter(float NewSpiceValue);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Player|Attributes")
	void ClearSpiceMeter();

	UFUNCTION(Server, Reliable)
	void ServerSetSpiceMeter(float NewSpiceValue);

	// ---- LOADOUT (GameplayTag driven) ----

	UPROPERTY(ReplicatedUsing = OnRep_Loadout, BlueprintReadWrite, Category = "Loadout")
	FGameplayTagContainer LoadoutTags;

	UFUNCTION()
	void OnRep_Loadout();

	UPROPERTY()
	TObjectPtr<class UARAttributeSetCore> AttributeSetCore;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Player|Attributes")
	FAROnCoreAttributeChangedSignature OnCoreAttributeChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Player|Attributes")
	FAROnScalarAttributeChangedSignature OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Player|Attributes")
	FAROnScalarAttributeChangedSignature OnMaxHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Player|Attributes")
	FAROnScalarAttributeChangedSignature OnSpiceChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Player|Attributes")
	FAROnScalarAttributeChangedSignature OnMaxSpiceChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Player|Attributes")
	FAROnScalarAttributeChangedSignature OnMoveSpeedChanged;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "State Serialization")
	TObjectPtr<UScriptStruct> ClassStateStruct;

	virtual bool ApplyStateFromStruct_Implementation(const FInstancedStruct& SavedState) override;

	UFUNCTION(Server, Reliable)
	void ServerApplyStateFromStruct(const FInstancedStruct& SavedState);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	void EnsureDefaultLoadoutIfEmpty();
	void BindTrackedAttributeDelegates();
	void UnbindTrackedAttributeDelegates();
	void BroadcastTrackedAttributeSnapshot();
	void HandleHealthAttributeChanged(const FOnAttributeChangeData& ChangeData);
	void HandleMaxHealthAttributeChanged(const FOnAttributeChangeData& ChangeData);
	void HandleSpiceAttributeChanged(const FOnAttributeChangeData& ChangeData);
	void HandleMaxSpiceAttributeChanged(const FOnAttributeChangeData& ChangeData);
	void HandleMoveSpeedAttributeChanged(const FOnAttributeChangeData& ChangeData);
	void BroadcastCoreAttributeChanged(EARCoreAttributeType AttributeType, float NewValue, float OldValue);
	void SetSpiceMeter_Internal(float NewSpiceValue);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	FDelegateHandle HealthChangedDelegateHandle;
	FDelegateHandle MaxHealthChangedDelegateHandle;
	FDelegateHandle SpiceChangedDelegateHandle;
	FDelegateHandle MaxSpiceChangedDelegateHandle;
	FDelegateHandle MoveSpeedChangedDelegateHandle;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
