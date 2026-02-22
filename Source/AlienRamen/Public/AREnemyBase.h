// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffectTypes.h"
#include "GameFramework/Character.h"
#include "AREnemyBase.generated.h"

class UAbilitySystemComponent;
class UARAttributeSetCore;
class UARAbilitySet;
struct FOnAttributeChangeData;

UENUM(BlueprintType)
enum class EAREnemyColor : uint8
{
	Red = 0,
	White = 1,
	Blue = 2,
};

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
	bool ActivateAbilityByTag(FGameplayTag AbilityTag, bool bAllowPartialMatch = false);

	UFUNCTION(BlueprintCallable, Category = "AR|Enemy|GAS")
	void CancelAbilitiesByTag(FGameplayTag AbilityTag, bool bAllowPartialMatch = true);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "AR|Enemy|Life")
	void HandleDeath(AActor* InstigatorActor);

	UFUNCTION(BlueprintImplementableEvent, Category = "AR|Enemy|Lifecycle")
	void BP_OnEnemyInitialized();

	UFUNCTION(BlueprintImplementableEvent, Category = "AR|Enemy|Lifecycle")
	void BP_OnEnemyDied(AActor* InstigatorActor);

	UFUNCTION(BlueprintPure, Category = "AR|Enemy|Lifecycle")
	bool IsDead() const { return bIsDead; }

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
	void BindHealthChangeDelegate();
	void UnbindHealthChangeDelegate();
	void OnHealthChanged(const FOnAttributeChangeData& ChangeData);

public:	
	// Set to false by default; enable in child Blueprints if needed.
	virtual void Tick(float DeltaTime) override;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AR|Enemy|GAS")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AR|Enemy|GAS")
	TObjectPtr<UARAttributeSetCore> AttributeSetCore;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AR|Enemy|GAS")
	TObjectPtr<UARAbilitySet> StartupAbilitySet;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "AR|Enemy|Gameplay")
	EAREnemyColor EnemyColor = EAREnemyColor::Red;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "AR|Enemy|Gameplay")
	FGameplayTag EnemyArchetypeTag;

	UPROPERTY(ReplicatedUsing=OnRep_IsDead, BlueprintReadOnly, Category = "AR|Enemy|Lifecycle")
	bool bIsDead = false;

	UFUNCTION()
	void OnRep_IsDead();

private:
	UPROPERTY()
	TArray<FGameplayAbilitySpecHandle> StartupGrantedAbilityHandles;

	UPROPERTY()
	TArray<FActiveGameplayEffectHandle> StartupAppliedEffectHandles;

	FDelegateHandle HealthChangedDelegateHandle;
	bool bStartupSetApplied = false;
};
