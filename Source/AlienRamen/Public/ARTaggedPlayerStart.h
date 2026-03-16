/**
 * @file ARTaggedPlayerStart.h
 * @brief Tagged player-start actor for slot/character-aware spawn routing.
 */
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameFramework/PlayerStart.h"
#include "ARTaggedPlayerStart.generated.h"

UCLASS()
class ALIENRAMEN_API AARTaggedPlayerStart : public APlayerStart
{
	GENERATED_BODY()

public:
	/**
	 * Gameplay-tag identity this start point serves (for example `Player.Slot.P1` or `Shop.Character.Brother`).
	 * Runtime spawn routing reads this gameplay-tag field.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Spawn", meta = (DisplayName = "Player Start Tag (Gameplay)", Categories = "Shop.Character,Player.Slot,Parley.Speaker,Shop.Customer", ToolTip = "Gameplay-tag identity for this start point. Runtime spawn matching uses this field."))
	FGameplayTag SpawnIdentityTag;

	/** When true, only exact tag matches are accepted for this start point. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Alien Ramen|Spawn", meta = (ToolTip = "When enabled, this start accepts only exact tag matches; otherwise parent/child matches are allowed."))
	bool bExactTagMatchOnly = false;

	/** Checks whether this start point should satisfy a spawn query tag. */
	bool MatchesSpawnIdentityTag(const FGameplayTag& QueryTag, const bool bRequireExact) const;

#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* InProperty) const override;
#endif
};

