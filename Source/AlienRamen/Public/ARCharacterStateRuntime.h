/**
 * @file ARCharacterStateRuntime.h
 * @brief Replicated per-character runtime owner for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "GameFramework/Info.h"
#include "ARInvaderSpicyTrackTypes.h"
#include "ARPlayerTypes.h"
#include "ARCharacterStateRuntime.generated.h"

class AARPlayerStateBase;
class APawn;
class UAbilitySystemComponent;
class UARAttributeSetCore;
class UARAttributeSetPlayer;
struct FARCharacterSaveData;

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARCharacterRuntimeCoreAttributeSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|Character Runtime|Attributes")
	float Health = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|Character Runtime|Attributes")
	float MaxHealth = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|Character Runtime|Attributes")
	float MoveSpeed = 0.f;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARCharacterRuntimePlayerAttributeSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|Character Runtime|Attributes")
	float Spice = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|Character Runtime|Attributes")
	float MaxSpice = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|Character Runtime|Attributes")
	float Strength = 0.f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FAROnCharacterRuntimeLoadoutChangedSignature,
	AARCharacterStateRuntime*,
	SourceRuntime,
	const FGameplayTagContainer&,
	NewLoadoutTags,
	const FGameplayTagContainer&,
	OldLoadoutTags);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FAROnCharacterRuntimePawnChangedSignature,
	AARCharacterStateRuntime*,
	SourceRuntime,
	APawn*,
	NewPawn,
	APawn*,
	OldPawn);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FAROnCharacterRuntimeDownedChangedSignature,
	AARCharacterStateRuntime*,
	SourceRuntime,
	FGameplayTag,
	CharacterTag,
	bool,
	bNewDowned,
	bool,
	bOldDowned);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FAROnCharacterRuntimeDeadChangedSignature,
	AARCharacterStateRuntime*,
	SourceRuntime,
	FGameplayTag,
	CharacterTag,
	bool,
	bNewDead,
	bool,
	bOldDead);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FAROnCharacterRuntimeInvaderColorChangedSignature,
	EARAffinityColor,
	NewColor,
	EARAffinityColor,
	OldColor);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FAROnCharacterRuntimeInvaderComboChangedSignature,
	AARCharacterStateRuntime*,
	SourceRuntime,
	FGameplayTag,
	CharacterTag,
	int32,
	NewCombo,
	int32,
	OldCombo);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FAROnCharacterRuntimeActivatedUpgradesChangedSignature,
	AARCharacterStateRuntime*,
	SourceRuntime,
	FGameplayTag,
	CharacterTag,
	const FGameplayTagContainer&,
	NewActivatedTags,
	const FGameplayTagContainer&,
	OldActivatedTags);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FAROnCharacterRuntimeSpiceSharingChangedSignature,
	AARCharacterStateRuntime*,
	SourceRuntime,
	FGameplayTag,
	CharacterTag,
	bool,
	bNewSharing,
	bool,
	bOldSharing);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FAROnCharacterRuntimeSpicyTrackCursorChangedSignature,
	AARCharacterStateRuntime*,
	SourceRuntime,
	FGameplayTag,
	CharacterTag,
	int32,
	NewCursorTier,
	int32,
	OldCursorTier);

/**
 * Replicated runtime owner for a canonical character tag.
 *
 * Responsibilities:
 * - owns ASC + attribute set for character-scoped gameplay state
 * - owns projected loadout and invader runtime fields for this character
 * - tracks current pawn binding (OwnerActor = runtime, AvatarActor = pawn)
 */
UCLASS(BlueprintType)
class ALIENRAMEN_API AARCharacterStateRuntime : public AInfo, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AARCharacterStateRuntime();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Character Runtime")
	FGameplayTag GetCharacterTag() const { return CharacterTag; }

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Character Runtime", meta = (BlueprintAuthorityOnly))
	void SetCharacterTag(FGameplayTag NewCharacterTag);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Character Runtime")
	AARPlayerStateBase* GetOwningPlayerState() const { return OwningPlayerState; }

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Character Runtime", meta = (BlueprintAuthorityOnly))
	void SetOwningPlayerState(AARPlayerStateBase* NewOwningPlayerState);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Character Runtime")
	APawn* GetCurrentPawn() const { return CurrentPawn; }

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Character Runtime", meta = (BlueprintAuthorityOnly))
	void SetCurrentPawn(APawn* NewPawn);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Character Runtime|Loadout")
	const FGameplayTagContainer& GetLoadoutTags() const { return LoadoutTags; }

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Character Runtime|Loadout", meta = (BlueprintAuthorityOnly))
	void SetLoadoutTags(const FGameplayTagContainer& NewLoadoutTags);

	/** Character-owned persistent progression tags currently projected onto this runtime. */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Character Runtime|Progression")
	const FGameplayTagContainer& GetCharacterProgressionTags() const { return CharacterProgressionTags; }

	/** Authority-only full replacement for character-owned progression tags. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Character Runtime|Progression", meta = (BlueprintAuthorityOnly))
	void SetCharacterProgressionTags(const FGameplayTagContainer& NewCharacterProgressionTags);

	/** Authority-only add for one character-owned progression tag. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Character Runtime|Progression", meta = (BlueprintAuthorityOnly))
	bool AddCharacterProgressionTag(FGameplayTag ProgressionTag);

	/** Authority-only removal for one character-owned progression tag. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Character Runtime|Progression", meta = (BlueprintAuthorityOnly))
	bool RemoveCharacterProgressionTag(FGameplayTag ProgressionTag);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Character Runtime|Attributes")
	float GetCoreAttributeValue(EARCoreAttributeType AttributeType) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Character Runtime|Attributes")
	FARCharacterRuntimeCoreAttributeSnapshot GetCoreAttributeSnapshot() const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Character Runtime|Attributes")
	float GetPlayerAttributeValue(EARPlayerAttributeType AttributeType) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Character Runtime|Attributes")
	FARCharacterRuntimePlayerAttributeSnapshot GetPlayerAttributeSnapshot() const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Character Runtime|Attributes")
	float GetSpiceNormalized() const;

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Character Runtime|Attributes", meta = (BlueprintAuthorityOnly))
	void SetSpiceMeter(float NewSpiceValue);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Character Runtime|Attributes", meta = (BlueprintAuthorityOnly))
	void SetStrength(float NewStrength);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Character Runtime")
	bool IsDowned() const { return bIsDowned; }

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Character Runtime", meta = (BlueprintAuthorityOnly))
	void SetDownedState(bool bNewDowned);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Character Runtime")
	bool IsDeadState() const { return bIsDeadState; }

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Character Runtime", meta = (BlueprintAuthorityOnly))
	void SetDeadState(bool bNewDead);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Character Runtime|Invader|Spice Track")
	EARAffinityColor GetInvaderPlayerColor() const { return InvaderPlayerColor; }

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Character Runtime|Invader|Spice Track", meta = (BlueprintAuthorityOnly))
	void SetInvaderPlayerColor(EARAffinityColor NewColor);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Character Runtime|Invader|Spice Track")
	int32 GetInvaderComboCount() const { return InvaderComboCount; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Character Runtime|Invader|Spice Track")
	float GetInvaderLastKillCreditServerTime() const { return LastInvaderKillCreditServerTime; }

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Character Runtime|Invader|Spice Track", meta = (BlueprintAuthorityOnly))
	void ResetInvaderCombo();

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Character Runtime|Invader|Spice Track", meta = (BlueprintAuthorityOnly))
	void ReportInvaderKillCredit(EARAffinityColor EnemyColor, float ServerTimeSeconds, float ComboTimeoutSeconds);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Character Runtime|Invader|Spice Track", meta = (BlueprintAuthorityOnly))
	void MarkInvaderUpgradeActivated(FGameplayTag UpgradeTag);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Character Runtime|Invader|Spice Track", meta = (BlueprintAuthorityOnly))
	void ClearActivatedInvaderUpgrades();

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Character Runtime|Invader|Spice Track")
	bool HasActivatedInvaderUpgrade(FGameplayTag UpgradeTag) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Character Runtime|Invader|Spice Track")
	const FGameplayTagContainer& GetActivatedInvaderUpgrades() const { return ActivatedInvaderUpgradeTags; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Character Runtime|Invader|Spice Track")
	bool IsSpiceSharingActive() const { return bIsSharingSpice; }

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Character Runtime|Invader|Spice Track", meta = (BlueprintAuthorityOnly))
	void SetSpiceSharingActive(bool bNewIsSharing);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Character Runtime|Invader|Spice Track|Cursor")
	int32 GetSpicyTrackCursorTier() const { return SpicyTrackCursorTier; }

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Character Runtime|Invader|Spice Track|Cursor", meta = (BlueprintAuthorityOnly))
	void SetSpicyTrackCursorTier(int32 NewCursorTier);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Character Runtime|Invader|Spice Track|Cursor", meta = (BlueprintAuthorityOnly))
	void AdjustSpicyTrackCursorTier(int32 DeltaTier);

	/** Writes replicated runtime state into a save row for this character. */
	void WriteSaveData(FARCharacterSaveData& InOutSaveData) const;

	/** Restores replicated runtime state from a save row for this character. */
	void ApplySaveData(const FARCharacterSaveData& InSaveData);

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Character Runtime")
	FAROnCharacterRuntimeLoadoutChangedSignature OnLoadoutChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Character Runtime")
	FAROnCharacterRuntimePawnChangedSignature OnCurrentPawnChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Character Runtime")
	FAROnCharacterRuntimeDownedChangedSignature OnDownedStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Character Runtime")
	FAROnCharacterRuntimeDeadChangedSignature OnDeadStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Character Runtime|Invader|Spice Track")
	FAROnCharacterRuntimeInvaderColorChangedSignature OnInvaderPlayerColorChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Character Runtime|Invader|Spice Track")
	FAROnCharacterRuntimeInvaderComboChangedSignature OnInvaderComboChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Character Runtime|Invader|Spice Track")
	FAROnCharacterRuntimeActivatedUpgradesChangedSignature OnActivatedInvaderUpgradesChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Character Runtime|Invader|Spice Track")
	FAROnCharacterRuntimeSpiceSharingChangedSignature OnSpiceSharingStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Character Runtime|Invader|Spice Track|Cursor")
	FAROnCharacterRuntimeSpicyTrackCursorChangedSignature OnSpicyTrackCursorChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_CharacterTag(FGameplayTag OldCharacterTag);

	UFUNCTION()
	void OnRep_OwningPlayerState(AARPlayerStateBase* OldOwningPlayerState);

	UFUNCTION()
	void OnRep_CurrentPawn(APawn* OldPawn);

	UFUNCTION()
	void OnRep_LoadoutTags(const FGameplayTagContainer& OldLoadoutTags);

	UFUNCTION()
	void OnRep_Downed(bool bOldDowned);

	UFUNCTION()
	void OnRep_Dead(bool bOldDead);

	UFUNCTION()
	void OnRep_InvaderPlayerColor(EARAffinityColor OldColor);

	UFUNCTION()
	void OnRep_InvaderComboCount(int32 OldComboCount);

	UFUNCTION()
	void OnRep_ActivatedInvaderUpgrades(const FGameplayTagContainer& OldActivatedTags);

	UFUNCTION()
	void OnRep_IsSharingSpice(bool bOldIsSharingSpice);

	UFUNCTION()
	void OnRep_SpicyTrackCursorTier(int32 OldCursorTier);

private:
	void RefreshAbilityActorInfo();
	static bool DoesInvaderColorMatch(EARAffinityColor PlayerColor, EARAffinityColor EnemyColor);
	int32 ClampSpicyTrackCursorTier(int32 RequestedCursorTier) const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Character Runtime|GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UARAttributeSetCore> AttributeSetCore;

	UPROPERTY()
	TObjectPtr<UARAttributeSetPlayer> AttributeSetPlayer;

	UPROPERTY(ReplicatedUsing = OnRep_CharacterTag, BlueprintReadOnly, Category = "Alien Ramen|Character Runtime", meta = (AllowPrivateAccess = "true"))
	FGameplayTag CharacterTag;

	UPROPERTY(ReplicatedUsing = OnRep_OwningPlayerState, BlueprintReadOnly, Category = "Alien Ramen|Character Runtime", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AARPlayerStateBase> OwningPlayerState = nullptr;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentPawn, BlueprintReadOnly, Category = "Alien Ramen|Character Runtime", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<APawn> CurrentPawn = nullptr;

	UPROPERTY(ReplicatedUsing = OnRep_LoadoutTags, BlueprintReadOnly, Category = "Alien Ramen|Character Runtime|Loadout", meta = (AllowPrivateAccess = "true"))
	FGameplayTagContainer LoadoutTags;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Alien Ramen|Character Runtime|Progression", meta = (AllowPrivateAccess = "true", ToolTip = "Character-owned persistent progression currently projected onto this runtime."))
	FGameplayTagContainer CharacterProgressionTags;

	UPROPERTY(ReplicatedUsing = OnRep_Downed, Transient, BlueprintReadOnly, Category = "Alien Ramen|Character Runtime", meta = (AllowPrivateAccess = "true"))
	bool bIsDowned = false;

	UPROPERTY(ReplicatedUsing = OnRep_Dead, Transient, BlueprintReadOnly, Category = "Alien Ramen|Character Runtime", meta = (AllowPrivateAccess = "true"))
	bool bIsDeadState = false;

	UPROPERTY(ReplicatedUsing = OnRep_InvaderPlayerColor, Transient, BlueprintReadOnly, Category = "Alien Ramen|Character Runtime|Invader|Spice Track", meta = (AllowPrivateAccess = "true"))
	EARAffinityColor InvaderPlayerColor = EARAffinityColor::None;

	UPROPERTY(ReplicatedUsing = OnRep_InvaderComboCount, Transient, BlueprintReadOnly, Category = "Alien Ramen|Character Runtime|Invader|Spice Track", meta = (AllowPrivateAccess = "true"))
	int32 InvaderComboCount = 0;

	UPROPERTY(ReplicatedUsing = OnRep_ActivatedInvaderUpgrades, Transient, BlueprintReadOnly, Category = "Alien Ramen|Character Runtime|Invader|Spice Track", meta = (AllowPrivateAccess = "true"))
	FGameplayTagContainer ActivatedInvaderUpgradeTags;

	UPROPERTY(ReplicatedUsing = OnRep_IsSharingSpice, Transient, BlueprintReadOnly, Category = "Alien Ramen|Character Runtime|Invader|Spice Track", meta = (AllowPrivateAccess = "true"))
	bool bIsSharingSpice = false;

	UPROPERTY(ReplicatedUsing = OnRep_SpicyTrackCursorTier, Transient, BlueprintReadOnly, Category = "Alien Ramen|Character Runtime|Invader|Spice Track|Cursor", meta = (AllowPrivateAccess = "true"))
	int32 SpicyTrackCursorTier = 0;

	UPROPERTY(Transient)
	float LastInvaderKillCreditServerTime = -1.0f;
};
