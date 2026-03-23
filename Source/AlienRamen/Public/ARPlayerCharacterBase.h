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
class AARCharacterStateRuntime;
class UEmoComponent;
class UParleySpeakerComponent;

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

	/** Returns the pawn-owned Parley speaker component used as dialogue requester identity. */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Player|Dialogue")
	UParleySpeakerComponent* GetParleySpeakerComponent() const { return ParleySpeakerComponent; }

	/** Returns the character runtime currently bound to this pawn, even while unpossessed. */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Player|Character Runtime")
	AARCharacterStateRuntime* GetRepresentedCharacterRuntime() const;

	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Player")
	TObjectPtr<UEmoComponent> EmotionComponent;

	/** Last local 2D movement input vector received by this pawn. Blueprints should update this from movement input handlers. */
	UPROPERTY(Transient, BlueprintReadWrite, Category = "Alien Ramen|Player|Input", meta = (ToolTip = "Last local 2D movement input vector received by this pawn. Blueprints should update this from movement input handlers."))
	FVector2D CurrentMovementVector = FVector2D::ZeroVector;

	/** Pawn-side dialogue identity source. Dialogue start paths read requester/owner tags from this component. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Player|Dialogue")
	TObjectPtr<UParleySpeakerComponent> ParleySpeakerComponent;

private:
	void RefreshParleySpeakerFromPlayerState();
};
