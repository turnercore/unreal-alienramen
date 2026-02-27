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
class AARPlayerStateBase;

UENUM(BlueprintType)
enum class EARPlayerSlot : uint8
{
	Unknown = 0,
	P1,
	P2
};

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

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(
	FAROnCoreAttributeChangedSignature,
	AARPlayerStateBase*,
	SourcePlayerState,
	EARPlayerSlot,
	SourcePlayerSlot,
	EARCoreAttributeType,
	AttributeType,
	float,
	NewValue,
	float,
	OldValue);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FAROnScalarAttributeChangedSignature,
	AARPlayerStateBase*,
	SourcePlayerState,
	EARPlayerSlot,
	SourcePlayerSlot,
	float,
	NewValue,
	float,
	OldValue);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FAROnPlayerSlotChangedSignature,
	EARPlayerSlot,
	NewSlot,
	EARPlayerSlot,
	OldSlot);

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

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Player")
	EARPlayerSlot GetPlayerSlot() const { return PlayerSlot; }

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Player", meta = (BlueprintAuthorityOnly))
	void SetPlayerSlot(EARPlayerSlot NewSlot);

	// UI-friendly slot index for local co-op style displays (0-based, from GameState PlayerArray order).
	// Returns INDEX_NONE if not currently resolvable.
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Player|Attributes")
	int32 GetHUDPlayerSlotIndex() const;

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

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Player")
	FAROnPlayerSlotChangedSignature OnPlayerSlotChanged;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "State Serialization")
	TObjectPtr<UScriptStruct> ClassStateStruct;

	virtual bool ApplyStateFromStruct_Implementation(const FInstancedStruct& SavedState) override;

	UFUNCTION(Server, Reliable)
	void ServerApplyStateFromStruct(const FInstancedStruct& SavedState);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	UFUNCTION()
	void OnRep_PlayerSlot(EARPlayerSlot OldSlot);
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

	UPROPERTY(ReplicatedUsing=OnRep_PlayerSlot, EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Player")
	EARPlayerSlot PlayerSlot = EARPlayerSlot::Unknown;

	FDelegateHandle HealthChangedDelegateHandle;
	FDelegateHandle MaxHealthChangedDelegateHandle;
	FDelegateHandle SpiceChangedDelegateHandle;
	FDelegateHandle MaxSpiceChangedDelegateHandle;
	FDelegateHandle MoveSpeedChangedDelegateHandle;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
