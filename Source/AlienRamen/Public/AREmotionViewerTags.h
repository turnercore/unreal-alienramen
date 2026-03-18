/**
 * @file AREmotionViewerTags.h
 * @brief Shared helpers for building EMO HUD viewer tag containers from Alien Ramen player context.
 */
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

class APlayerState;
class APawn;

namespace AREmotion
{
	/**
	 * Builds the gameplay tags a local EMO HUD should resolve against.
	 * Includes the canonical character tag plus the paired Parley speaker tag when resolvable,
	 * and also includes the possessed pawn speaker tag when the pawn exposes one.
	 */
	ALIENRAMEN_API FGameplayTagContainer BuildEmotionViewerTags(const APlayerState* PlayerState, const APawn* PossessedPawn);
}
