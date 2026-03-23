/**
 * @file ARModeMapPathUtility.h
 * @brief Shared mode-tag to default map-path resolver for travel/save systems.
 */
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

namespace ARModeMapPath
{
	/** Returns the canonical default map asset path for a mode gameplay tag. */
	ALIENRAMEN_API FString ResolveDefaultMapPathForModeTag(const FGameplayTag& ModeTag);
}
