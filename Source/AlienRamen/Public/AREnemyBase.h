// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffectTypes.h"
#include "GameFramework/Character.h"
#include "ARInvaderTypes.h"
#include "AREnemyBase.generated.h"

class UAbilitySystemComponent;
class UARAttributeSetCore;
class UAREnemyAttributeSet;
class UGameplayEffect;
struct FOnAttributeChangeData;

UCLASS()
class ALIENRAMEN_API AAREnemyBase : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AAREnemyBase();

	// IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintCallable, Category = "AR|Enemy|GAS")
	UAbilitySystemComponent* GetASC() const { return AbilitySystemComponent; }

	UFUNCTION(BlueprintCallable, Category = "AR|Enemy|GAS")
	UARAttributeSetCore* GetCoreAttributes() const { return AttributeSetCore; }

	UFUNCTION(BlueprintCallable, Category = "AR|Enemy|GAS")
	UAREnemyAttributeSet* GetEnemyAttributes() const { return EnemyAttributeSet; }

	UFUNCTION(BlueprintCallable, Category = "AR|Enemy|GAS", meta = (BlueprintAuthorityOnly))
	bool ApplyDamageViaGAS(float Damage, AActor* Offender, float& OutCurrentHealth);
	bool ApplyDamageViaGAS(float Damage, AActor* Offender);

	UFUNCTION(BlueprintPure, Category = "AR|Enemy|GAS")
	float GetCurrentDamageFromGAS() const;

	UFUNCTION(BlueprintPure, Category = "AR|Enemy|GAS")
	float GetCurrentCollisionDamageFromGAS() const;

	// Applies DamageOverride if >= 0; otherwise uses GetCurrentDamageFromGAS().
	UFUNCTION(BlueprintCallable, Category = "AR|Enemy|GAS", meta = (BlueprintAuthorityOnly))
	bool ApplyDamageToTargetViaGAS(AActor* Target, float DamageOverride = -1.f);

	// Applies DamageOverride if >= 0; otherwise uses GetCurrentCollisionDamageFromGAS().
	UFUNCTION(BlueprintCallable, Category = "AR|Enemy|GAS", meta = (BlueprintAuthorityOnly))
	bool ApplyCollisionDamageToTargetViaGAS(AActor* Target, float DamageOverride = -1.f);

	virtual float TakeDamage(
		float DamageAmount,
		struct FDamageEvent const& DamageEvent,
		class AController* EventInstigator,
		AActor* DamageCauser) override;

	UFUNCTION(BlueprintCallable, Category = "AR|Enemy|GAS", meta = (BlueprintAuthorityOnly))
	bool ActivateAbilityByTag(FGameplayTag AbilityTag, bool bAllowPartialMatch = false);

	UFUNCTION(BlueprintCallable, Category = "AR|Enemy|GAS", meta = (BlueprintAuthorityOnly))
	void CancelAbilitiesByTag(FGameplayTag AbilityTag, bool bAllowPartialMatch = true);

	UFUNCTION(BlueprintPure, Category = "AR|Enemy|GAS")
	bool HasASCGameplayTag(FGameplayTag TagToCheck) const;

	UFUNCTION(BlueprintPure, Category = "AR|Enemy|GAS")
	bool HasAnyASCGameplayTags(const FGameplayTagContainer& TagsToCheck) const;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "AR|Enemy|Life", meta = (BlueprintAuthorityOnly))
	void HandleDeath(AActor* InstigatorActor);

	// Final lifecycle release step after death cleanup/signals. Default implementation destroys actor.
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "AR|Enemy|Life", meta = (BlueprintAuthorityOnly))
	void ReleaseEnemyActor();

	UFUNCTION(BlueprintImplementableEvent, Category = "AR|Enemy|Lifecycle")
	void BP_OnEnemyInitialized();

	UFUNCTION(BlueprintImplementableEvent, Category = "AR|Enemy|Lifecycle")
	void BP_OnEnemyDied(AActor* InstigatorActor);

	// Fired on authority immediately before ReleaseEnemyActor() is called.
	UFUNCTION(BlueprintImplementableEvent, Category = "AR|Enemy|Lifecycle")
	void BP_OnEnemyPreRelease(AActor* InstigatorActor);

	UFUNCTION(BlueprintPure, Category = "AR|Enemy|Lifecycle")
	bool IsDead() const { return bIsDead; }

	UFUNCTION(BlueprintCallable, Category = "AR|Enemy|Gameplay", meta = (BlueprintAuthorityOnly))
	void SetEnemyColor(EAREnemyColor InColor);

	UFUNCTION(BlueprintCallable, Category = "AR|Enemy|Gameplay", meta = (BlueprintAuthorityOnly))
	void SetEnemyIdentifierTag(FGameplayTag InIdentifierTag);

	UFUNCTION(BlueprintPure, Category = "AR|Enemy|Gameplay")
	FGameplayTag GetEnemyIdentifierTag() const { return EnemyIdentifierTag; }

	UFUNCTION(BlueprintImplementableEvent, Category = "AR|Enemy|Gameplay")
	void BP_OnEnemyIdentifierTagChanged(FGameplayTag NewIdentifierTag);

	UFUNCTION(BlueprintCallable, Category = "AR|Enemy|Gameplay", meta = (BlueprintAuthorityOnly))
	bool InitializeFromEnemyDefinitionTag();

	UFUNCTION(BlueprintImplementableEvent, Category = "AR|Enemy|Gameplay")
	void BP_OnEnemyDefinitionApplied();

	UFUNCTION(BlueprintCallable, Category = "AR|Enemy|Invader", meta = (BlueprintAuthorityOnly))
	void SetWaveRuntimeContext(
		int32 InWaveInstanceId,
		int32 InFormationSlotIndex,
		EARWavePhase InWavePhase,
		float InPhaseStartServerTime,
		bool bInFormationLockEnter = false,
		bool bInFormationLockActive = false);

	UFUNCTION(BlueprintCallable, Category = "AR|Enemy|Invader", meta = (BlueprintAuthorityOnly))
	void SetWavePhase(EARWavePhase InWavePhase, float InPhaseStartServerTime);

	UFUNCTION(BlueprintCallable, Category = "AR|Enemy|Invader", meta = (BlueprintAuthorityOnly))
	void NotifyEnteredGameplayScreen(float InServerTime);

	UFUNCTION(BlueprintPure, Category = "AR|Enemy|Invader")
	bool CanFireByWaveRules() const;

	UFUNCTION(BlueprintCallable, Category = "AR|Enemy|Invader", meta = (BlueprintAuthorityOnly))
	void SetFormationLockRules(bool bInFormationLockEnter, bool bInFormationLockActive);

	UFUNCTION(BlueprintPure, Category = "AR|Enemy|Invader")
	int32 GetWaveInstanceId() const { return WaveInstanceId; }

	UFUNCTION(BlueprintPure, Category = "AR|Enemy|Invader")
	EARWavePhase GetWavePhase() const { return WavePhase; }

	UFUNCTION(BlueprintPure, Category = "AR|Enemy|Invader")
	bool GetFormationLockEnter() const { return bFormationLockEnter; }

	UFUNCTION(BlueprintPure, Category = "AR|Enemy|Invader")
	bool GetFormationLockActive() const { return bFormationLockActive; }
	
	UFUNCTION(BlueprintPure, Category = "AR|Enemy|Invader")
	bool HasEnteredGameplayScreen() const { return bHasEnteredGameplayScreen; }

	UFUNCTION(BlueprintPure, Category = "AR|Enemy|Invader")
	bool HasReachedFormationSlot() const { return bReachedFormationSlot; }

	UFUNCTION(BlueprintCallable, Category = "AR|Enemy|Invader", meta = (BlueprintAuthorityOnly))
	void SetReachedFormationSlot(bool bInReachedFormationSlot);

	UFUNCTION(BlueprintCallable, Category = "AR|Enemy|Invader", meta = (BlueprintAuthorityOnly))
	void SetFormationTargetWorldLocation(const FVector& InFormationTargetWorldLocation);

	UFUNCTION(BlueprintPure, Category = "AR|Enemy|Invader")
	FVector GetFormationTargetWorldLocation() const { return FormationTargetWorldLocation; }

	UFUNCTION(BlueprintPure, Category = "AR|Enemy|Invader")
	bool HasFormationTargetWorldLocation() const { return bHasFormationTargetWorldLocation; }

	// Returns true only on the first frame this enemy is considered leaked.
	UFUNCTION(BlueprintCallable, Category = "AR|Enemy|Invader")
	bool CheckAndMarkLeaked(float LeakBoundaryX);

	UFUNCTION(BlueprintPure, Category = "AR|Enemy|Invader")
	bool IsLeaked() const { return bCountedAsLeak; }

	bool HasBeenCountedAsLeak() const { return bCountedAsLeak; }
	void MarkCountedAsLeak() { bCountedAsLeak = true; }

protected:
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_Controller() override;
	virtual void UnPossessed() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void InitAbilityActorInfo();
	void ApplyStartupAbilitySet();
	void ClearStartupAbilitySet();
	void ApplyRuntimeEnemyEffects(const TArray<TSubclassOf<UGameplayEffect>>& Effects);
	void ClearRuntimeEnemyEffects();
	void ApplyRuntimeEnemyTags(const FGameplayTagContainer& InTags);
	void ClearRuntimeEnemyTags();
	bool ResolveEnemyDefinition(FARInvaderEnemyDefRow& OutRow, FString& OutError) const;
	void ApplyEnemyRuntimeInitData(const FARInvaderEnemyRuntimeInitData& RuntimeInit);
	void BindHealthChangeDelegate();
	void UnbindHealthChangeDelegate();
	void OnHealthChanged(const FOnAttributeChangeData& ChangeData);
	void BindMoveSpeedChangeDelegate();
	void UnbindMoveSpeedChangeDelegate();
	void OnMoveSpeedChanged(const FOnAttributeChangeData& ChangeData);
	void RefreshCharacterMovementSpeedFromAttributes();

public:	
	// Set to false by default; enable in child Blueprints if needed.
	virtual void Tick(float DeltaTime) override;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AR|Enemy|GAS")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	// StateTree-friendly alias so tasks/conditions can bind ASC directly from Actor context.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AR|Enemy|GAS")
	TObjectPtr<UAbilitySystemComponent> StateTreeASC;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AR|Enemy|GAS")
	TObjectPtr<UARAttributeSetCore> AttributeSetCore;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AR|Enemy|GAS")
	TObjectPtr<UAREnemyAttributeSet> EnemyAttributeSet;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_EnemyColor, Category = "AR|Enemy|Gameplay")
	EAREnemyColor EnemyColor = EAREnemyColor::Red;

	UFUNCTION()
	void OnRep_EnemyColor();

	UFUNCTION(BlueprintImplementableEvent, Category = "AR|Enemy|Gameplay")
	void BP_OnEnemyColorChanged(EAREnemyColor NewColor);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "AR|Enemy|Gameplay")
	FGameplayTag EnemyArchetypeTag;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_EnemyIdentifierTag, Category = "AR|Enemy|Gameplay")
	FGameplayTag EnemyIdentifierTag;

	UFUNCTION()
	void OnRep_EnemyIdentifierTag();

	UPROPERTY(ReplicatedUsing=OnRep_IsDead, BlueprintReadOnly, Category = "AR|Enemy|Lifecycle")
	bool bIsDead = false;

	UFUNCTION()
	void OnRep_IsDead();

	UPROPERTY(ReplicatedUsing=OnRep_WaveRuntimeContext, BlueprintReadOnly, Category = "AR|Enemy|Invader")
	int32 WaveInstanceId = INDEX_NONE;

	UPROPERTY(ReplicatedUsing=OnRep_WaveRuntimeContext, BlueprintReadOnly, Category = "AR|Enemy|Invader")
	int32 FormationSlotIndex = INDEX_NONE;

	UPROPERTY(ReplicatedUsing=OnRep_WaveRuntimeContext, BlueprintReadOnly, Category = "AR|Enemy|Invader")
	EARWavePhase WavePhase = EARWavePhase::Active;

	UPROPERTY(ReplicatedUsing=OnRep_WaveRuntimeContext, BlueprintReadOnly, Category = "AR|Enemy|Invader")
	float WavePhaseStartServerTime = 0.f;

	UPROPERTY(VisibleAnywhere, ReplicatedUsing=OnRep_WaveRuntimeContext, BlueprintReadOnly, Category = "AR|Enemy|Invader", meta = (AllowPrivateAccess = "true"))
	bool bFormationLockEnter = false;

	UPROPERTY(VisibleAnywhere, ReplicatedUsing=OnRep_WaveRuntimeContext, BlueprintReadOnly, Category = "AR|Enemy|Invader", meta = (AllowPrivateAccess = "true"))
	bool bFormationLockActive = false;

	UPROPERTY(ReplicatedUsing=OnRep_WaveRuntimeContext, BlueprintReadOnly, Category = "AR|Enemy|Invader")
	FVector FormationTargetWorldLocation = FVector::ZeroVector;

	UPROPERTY(ReplicatedUsing=OnRep_WaveRuntimeContext, BlueprintReadOnly, Category = "AR|Enemy|Invader")
	bool bHasFormationTargetWorldLocation = false;

	UFUNCTION()
	void OnRep_WaveRuntimeContext();

	UFUNCTION(BlueprintImplementableEvent, Category = "AR|Enemy|Invader")
	void BP_OnWavePhaseChanged(EARWavePhase NewPhase);

private:
	UPROPERTY()
	TArray<FGameplayAbilitySpecHandle> StartupGrantedAbilityHandles;

	UPROPERTY()
	TArray<FActiveGameplayEffectHandle> StartupAppliedEffectHandles;

	UPROPERTY()
	TArray<FActiveGameplayEffectHandle> RuntimeAppliedEffectHandles;

	UPROPERTY()
	FGameplayTagContainer RuntimeAppliedLooseTags;

	UPROPERTY()
	TArray<FARAbilitySet_AbilityEntry> RuntimeSpecificAbilities;

	FDelegateHandle HealthChangedDelegateHandle;
	FDelegateHandle MoveSpeedChangedDelegateHandle;
	bool bStartupSetApplied = false;
	bool bCountedAsLeak = false;
	bool bHasEnteredGameplayScreen = false;
	bool bReachedFormationSlot = false;
	bool bHasDispatchedEnteredScreenEvent = false;
	bool bHasDispatchedInFormationEvent = false;
	int32 LastDispatchedWavePhaseWaveId = INDEX_NONE;
	EARWavePhase LastDispatchedWavePhase = EARWavePhase::Berserk;
	float EnteredGameplayScreenServerTime = 0.f;

	void TryDispatchWavePhaseEvent();
	void TryDispatchEnteredScreenEvent();
	void TryDispatchInFormationEvent();
};
