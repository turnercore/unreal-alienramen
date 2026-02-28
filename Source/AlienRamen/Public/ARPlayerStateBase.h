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
enum class EARCharacterChoice : uint8
{
	None = 0,
	Brother,
	Sister
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

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FAROnCharacterPickedChangedSignature,
	AARPlayerStateBase*,
	SourcePlayerState,
	EARPlayerSlot,
	SourcePlayerSlot,
	EARCharacterChoice,
	NewCharacter,
	EARCharacterChoice,
	OldCharacter);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FAROnDisplayNameChangedSignature,
	AARPlayerStateBase*,
	SourcePlayerState,
	EARPlayerSlot,
	SourcePlayerSlot,
	const FString&,
	NewDisplayName,
	const FString&,
	OldDisplayName);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FAROnReadyStatusChangedSignature,
	AARPlayerStateBase*,
	SourcePlayerState,
	EARPlayerSlot,
	SourcePlayerSlot,
	bool,
	bNewReady,
	bool,
	bOldReady);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FAROnSetupStateChangedSignature,
	bool,
	bNewIsSetup,
	bool,
	bOldIsSetup);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FAROnLoadoutTagsChangedSignature,
	AARPlayerStateBase*,
	SourcePlayerState,
	EARPlayerSlot,
	SourcePlayerSlot,
	const FGameplayTagContainer&,
	NewLoadoutTags,
	const FGameplayTagContainer&,
	OldLoadoutTags);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAROnTravelReadinessChangedSignature, bool, bIsReadyForTravel);

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

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Player")
	EARCharacterChoice GetCharacterPicked() const { return CharacterPicked; }

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Player")
	void SetCharacterPicked(EARCharacterChoice NewCharacter);

	UFUNCTION(Server, Reliable)
	void ServerPickCharacter(EARCharacterChoice NewCharacter);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Player")
	FString GetDisplayNameValue() const { return DisplayName; }

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Player")
	void SetDisplayNameValue(const FString& NewDisplayName);

	UFUNCTION(Server, Reliable)
	void ServerUpdateDisplayName(const FString& NewDisplayName);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Player")
	bool IsReadyForRun() const { return bIsReady; }

	// Composite readiness for travel: requires slot, character choice, and ready flag.
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Player")
	bool IsTravelReady() const;

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Player")
	void SetReadyForRun(bool bNewReady);

	UFUNCTION(Server, Reliable)
	void ServerUpdateReady(bool bNewReady);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Player")
	bool IsSetupComplete() const { return bIsSetup; }

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Player", meta = (BlueprintAuthorityOnly))
	void SetIsSetupComplete(bool bNewIsSetup);

	// First-join initialization path for non-travel PlayerStates when no saved identity row was found.
	// Intentionally keeps display name untouched and resets character choice to None.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Player", meta = (BlueprintAuthorityOnly))
	void InitializeForFirstSessionJoin();

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

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Player|Loadout")
	void SetLoadoutTags(const FGameplayTagContainer& NewLoadoutTags);

	UFUNCTION()
	void OnRep_Loadout(const FGameplayTagContainer& OldLoadoutTags);

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

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Player")
	FAROnCharacterPickedChangedSignature OnCharacterPickedChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Player")
	FAROnDisplayNameChangedSignature OnDisplayNameChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Player")
	FAROnReadyStatusChangedSignature OnReadyStatusChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Player")
	FAROnSetupStateChangedSignature OnSetupStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Player")
	FAROnLoadoutTagsChangedSignature OnLoadoutTagsChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Player")
	FAROnTravelReadinessChangedSignature OnTravelReadinessChanged;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "State Serialization")
	TObjectPtr<UScriptStruct> ClassStateStruct;

	virtual void CopyProperties(APlayerState* PlayerState) override;
	virtual bool ApplyStateFromStruct_Implementation(const FInstancedStruct& SavedState) override;

	UFUNCTION(Server, Reliable)
	void ServerApplyStateFromStruct(const FInstancedStruct& SavedState);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	UFUNCTION()
	void OnRep_PlayerSlot(EARPlayerSlot OldSlot);
	UFUNCTION()
	void OnRep_CharacterPicked(EARCharacterChoice OldCharacter);
	UFUNCTION()
	void OnRep_DisplayName(const FString& OldDisplayName);
	UFUNCTION()
	void OnRep_IsReady(bool bOldReady);
	UFUNCTION()
	void OnRep_IsSetup(bool bOldIsSetup);
	void SetCharacterPicked_Internal(EARCharacterChoice NewCharacter);
	void SetDisplayName_Internal(const FString& NewDisplayName);
	void SetReady_Internal(bool bNewReady);
	void SetLoadoutTags_Internal(const FGameplayTagContainer& NewLoadoutTags);
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
	void EvaluateTravelReadinessAndBroadcast();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(ReplicatedUsing=OnRep_PlayerSlot, EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Player")
	EARPlayerSlot PlayerSlot = EARPlayerSlot::Unknown;

	UPROPERTY(ReplicatedUsing=OnRep_CharacterPicked, EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Player")
	EARCharacterChoice CharacterPicked = EARCharacterChoice::None;

	UPROPERTY(ReplicatedUsing=OnRep_DisplayName, EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Player")
	FString DisplayName;

	UPROPERTY(ReplicatedUsing=OnRep_IsReady, EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Player")
	bool bIsReady = false;

	UPROPERTY(ReplicatedUsing=OnRep_IsSetup, EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Player")
	bool bIsSetup = false;

	// Cached travel readiness for change detection.
	UPROPERTY(Transient)
	bool bCachedTravelReady = false;

	FDelegateHandle HealthChangedDelegateHandle;
	FDelegateHandle MaxHealthChangedDelegateHandle;
	FDelegateHandle SpiceChangedDelegateHandle;
	FDelegateHandle MaxSpiceChangedDelegateHandle;
	FDelegateHandle MoveSpeedChangedDelegateHandle;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
