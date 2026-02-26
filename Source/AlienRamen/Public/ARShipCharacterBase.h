// ARShipCharacterBase.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffectTypes.h"
#include "StructUtils/InstancedStruct.h"
#include "Templates/UnrealTemplate.h"

#include "ARShipCharacterBase.generated.h"

class UAbilitySystemComponent;
class UGameplayAbility;
class UGameplayEffect;

class AARPlayerStateBase;
class AARPlayerController;

class UARWeaponDefinition;
class UARAbilitySet;
struct FTimerHandle;

/**
 * Base ship character for Alien Ramen.
 * - ASC is owned by PlayerState, Avatar is this pawn.
 * - On server possession: clears loadout, grants common ability set, then resolves ship/secondary/gadget rows from LoadoutTags and applies baseline.
 * - Exposes generic tag-based activation/cancel API for PlayerController / Blueprint.
 */
UCLASS()
class ALIENRAMEN_API AARShipCharacterBase : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AARShipCharacterBase();

	// IAbilitySystemInterface (forward to PlayerState ASC)
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// Current primary weapon (resolved from ship row). May be nullptr.
	UFUNCTION(BlueprintCallable, Category = "AR|Ship|Weapon")
	const UARWeaponDefinition* GetPrimaryWeaponDefinition() const;

	// -------------------------
	// Generic activation API
	// -------------------------

	/** Activates ONE ability that matches this tag (deterministic pick). */
	UFUNCTION(BlueprintCallable, Category = "AR|Abilities")
	bool ActivateAbilityByTag(FGameplayTag Tag, bool bAllowRemoteActivation = true);

	// Applies incoming damage through GAS (server-authoritative) and outputs current health.
	UFUNCTION(BlueprintCallable, Category = "AR|Ship|GAS", meta = (BlueprintAuthorityOnly))
	bool ApplyDamageViaGAS(float Damage, AActor* Offender, float& OutCurrentHealth);
	bool ApplyDamageViaGAS(float Damage, AActor* Offender);

	// Reads current Damage attribute from GAS.
	UFUNCTION(BlueprintPure, Category = "AR|Ship|GAS")
	float GetCurrentDamageFromGAS() const;

	// Applies DamageOverride if >= 0; otherwise uses GetCurrentDamageFromGAS().
	UFUNCTION(BlueprintCallable, Category = "AR|Ship|GAS", meta = (BlueprintAuthorityOnly))
	bool ApplyDamageToTargetViaGAS(AActor* Target, float DamageOverride = -1.f);

	/** Activates ONE ability that matches ANY of the tags (deterministic pick). */
	UFUNCTION(BlueprintCallable, Category = "AR|Abilities")
	bool ActivateAbilityByTags(const FGameplayTagContainer& InTagsToActivate, bool bAllowRemoteActivation = true);

	/** Activates ALL abilities matching this tag (optional use-case). */
	UFUNCTION(BlueprintCallable, Category = "AR|Abilities")
	int32 ActivateAllAbilitiesByTag(FGameplayTag Tag, bool bAllowRemoteActivation = true);

	/** Cancels abilities that match this tag (tag must be on the ability). */
	UFUNCTION(BlueprintCallable, Category = "AR|Abilities")
	void CancelAbilityByTag(FGameplayTag Tag);

	/** Cancels abilities that match ANY of the tags (tags must be on the ability). */
	UFUNCTION(BlueprintCallable, Category = "AR|Abilities")
	void CancelAbilityByTags(const FGameplayTagContainer& InTagsToCancel);

	// Helper: Find a property by name prefix (handles Unreal's auto-generated suffixes)
	static FProperty* FindPropertyByNamePrefix(const UScriptStruct* StructType, const FString& Prefix);

	// Debug helper: logs all properties on a struct
	static void LogAllPropertiesOnStruct(const UScriptStruct* StructType);

protected:
	// Server: possession
	virtual void PossessedBy(AController* NewController) override;

	// Clients: PlayerState replication arrives
	virtual void OnRep_PlayerState() override;

	// Server/client: cleanup when unpossessed
	virtual void UnPossessed() override;

	// Shared init: Owner = PlayerState, Avatar = this
	void InitAbilityActorInfo();
	void ApplyOrRefreshPrimaryWeaponRuntimeEffects();
	void ClearPrimaryWeaponRuntimeEffects();

	// ---- Loadout application (server only) ----
	void ClearAppliedLoadout();
	void GrantCommonAbilitySetFromController(AController* NewController);
	void ApplyLoadoutTagsToASC(const FGameplayTagContainer& InLoadoutTags);

	// Apply baseline for any row struct that contains common fields:
	// Stats, StartupAbilities, StartupEffects, ShipTags, MovementType, PrimaryWeapon(optional)
	void ApplyResolvedRowBaseline(const FInstancedStruct& RowStruct);

	// Reads LoadoutTags from PlayerState even if it's only defined in a BP child (reflection).
	bool GetPlayerLoadoutTags(FGameplayTagContainer& OutLoadoutTags) const;

	// Find the first tag under a root (e.g. Unlocks.Ships.*)
	bool FindFirstTagUnderRoot(const FGameplayTagContainer& InTags, const FGameplayTag& Root, FGameplayTag& OutTag) const;

	// Resolve a row using ContentLookupSubsystem (returns an InstancedStruct)
	bool ResolveRowFromTag(FGameplayTag Tag, FInstancedStruct& OutRow, FString& OutError) const;
	bool TryApplyServerLoadoutFromPlayerState(bool bLogErrors);
	void RetryServerLoadoutInit();

	// -------------------------
	// Internal activation helpers
	// -------------------------

	// Returns true if this spec matches ANY tag in TagContainer (exact match is preferred in scoring).
	static bool SpecMatchesAnyTag(const FGameplayAbilitySpec& Spec, const FGameplayTagContainer& InTagsToMatch, int32& OutBestScore);

	// Pick one matching ability handle deterministically.
	static bool PickBestMatchingAbilityHandle(
		UAbilitySystemComponent* ASC,
		const FGameplayTagContainer& InTagsToMatch,
		FGameplayAbilitySpecHandle& OutHandle
	);
	
protected:
	// Cached ASC (owned by PlayerState)
	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> CachedASC;

	// Current primary weapon (resolved from ship row)
	UPROPERTY(Transient)
	TObjectPtr<UARWeaponDefinition> CurrentPrimaryWeapon;

	// ---- Tracking so we can remove things on swap ----
	UPROPERTY(Transient)
	TArray<FGameplayAbilitySpecHandle> GrantedAbilityHandles;

	UPROPERTY(Transient)
	TArray<FActiveGameplayEffectHandle> AppliedEffectHandles;

	// Tracks loose tags applied at runtime (row baseline + mirrored loadout tags) for cleanup.
	UPROPERTY(Transient)
	FGameplayTagContainer AppliedLooseTags;

	// Server-only deferred loadout init state for possess/order races.
	UPROPERTY(Transient)
	bool bServerLoadoutApplied = false;

	UPROPERTY(Transient)
	int32 LoadoutInitRetryCount = 0;

	// BP compatibility: call legacy BP _Init once ASC is actually ready.
	UPROPERTY(Transient)
	bool bLegacyBPInitInvoked = false;

	FTimerHandle LoadoutInitRetryTimer;

	// Runtime weapon tuning effect (formerly applied from BP _Init).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AR|Ship|Weapon")
	TSubclassOf<UGameplayEffect> PrimaryWeaponFireRateEffectClass;

	UPROPERTY(Transient)
	FActiveGameplayEffectHandle BasePrimaryFireRateEffectHandle;

	// ---- BP row struct field names (must match your row struct fields) ----
	static const FName NAME_PrimaryWeapon;
	static const FName NAME_StartupAbilities;
	static const FName NAME_StartupEffects;
	static const FName NAME_ShipTags;
	static const FName NAME_Stats;
	static const FName NAME_MovementType;

	// ---- PlayerState field name (must match your BP variable name) ----
	static const FName NAME_LoadoutTags;

	// ---- Tag roots ----
	static const FGameplayTag TAGROOT_Ships;
	static const FGameplayTag TAGROOT_Secondaries;
	static const FGameplayTag TAGROOT_Gadgets;
};
