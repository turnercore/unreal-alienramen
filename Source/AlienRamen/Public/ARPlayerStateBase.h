#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "GameplayEffect.h"
#include "ARPlayerStateBase.generated.h"

class UAbilitySystemComponent;
class UAttributeSet;

UCLASS()
class ALIENRAMEN_API AARPlayerStateBase : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AARPlayerStateBase();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UAbilitySystemComponent* GetASC() const { return AbilitySystemComponent; }

	// ---- LOADOUT (GameplayTag driven) ----

	UPROPERTY(ReplicatedUsing = OnRep_Loadout, BlueprintReadWrite, Category = "Loadout")
	FGameplayTagContainer LoadoutTags;

	UFUNCTION()
	void OnRep_Loadout();

	UPROPERTY()
	TObjectPtr<class UARAttributeSetCore> AttributeSetCore;

protected:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
