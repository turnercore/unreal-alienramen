/**
 * @file ARPlayerCharacterBase.h
 * @brief ARPlayerCharacterBase header for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "ARPlayerCharacterBase.generated.h"

class UAbilitySystemComponent;
class UEmoComponent;

/** Root player pawn base; implements ASC interface stub for mode-specific subclasses. */
UCLASS()
class ALIENRAMEN_API AARPlayerCharacterBase : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AARPlayerCharacterBase();

	// Default base returns null; mode-specific character classes can provide an ASC source.
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Player")
	UEmoComponent* GetEmotionComponent() const { return EmotionComponent; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Player")
	TObjectPtr<UEmoComponent> EmotionComponent;
};
