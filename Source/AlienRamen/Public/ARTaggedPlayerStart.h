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
	/** Identity tag this start point serves (for example Player.Slot.P1 or Parley.Speaker.Brother). */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Alien Ramen|Spawn", meta = (ToolTip = "Identity tag this start point serves (for example Player.Slot.P1 or Parley.Speaker.Brother)."))
	FGameplayTag SpawnIdentityTag;

	/** When true, only exact tag matches are accepted for this start point. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Alien Ramen|Spawn", meta = (ToolTip = "When enabled, this start accepts only exact tag matches; otherwise parent/child matches are allowed."))
	bool bExactTagMatchOnly = false;

	bool MatchesSpawnIdentityTag(const FGameplayTag& QueryTag, const bool bRequireExact) const;
};

