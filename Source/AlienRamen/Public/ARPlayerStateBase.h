#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "GameplayEffect.h"
#include "StructSerializable.h"
#include "ARPlayerStateBase.generated.h"

class UAbilitySystemComponent;
class UAttributeSet;

UCLASS()
class ALIENRAMEN_API AARPlayerStateBase : public APlayerState, public IAbilitySystemInterface, public IStructSerializable
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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "State Serialization")
	TObjectPtr<UScriptStruct> ClassStateStruct;

	virtual bool ApplyStateFromStruct_Implementation(const FInstancedStruct& SavedState) override;

	UFUNCTION(Server, Reliable)
	void ServerApplyStateFromStruct(const FInstancedStruct& SavedState);

protected:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
