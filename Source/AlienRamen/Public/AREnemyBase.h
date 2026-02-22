// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Pawn.h"
#include "AREnemyBase.generated.h"

class UAbilitySystemComponent;
class UARAttributeSetCore;

UCLASS()
class ALIENRAMEN_API AAREnemyBase : public APawn, public IAbilitySystemInterface
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

protected:
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_Controller() override;
	virtual void UnPossessed() override;

	void InitAbilityActorInfo();

public:	
	// Set to false by default; enable in child Blueprints if needed.
	virtual void Tick(float DeltaTime) override;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AR|Enemy|GAS")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AR|Enemy|GAS")
	TObjectPtr<UARAttributeSetCore> AttributeSetCore;

};
