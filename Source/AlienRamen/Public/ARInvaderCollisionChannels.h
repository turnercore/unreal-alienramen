/**
 * @file ARInvaderCollisionChannels.h
 * @brief Shared collision-channel helpers for invader runtime actors.
 */
#pragma once

#include "CoreMinimal.h"

namespace ARInvaderCollisionChannels
{
	// Object channels configured in Config/DefaultEngine.ini.
	static constexpr ECollisionChannel Enemy = ECC_GameTraceChannel2;
	static constexpr ECollisionChannel Player = ECC_GameTraceChannel3;
	static constexpr ECollisionChannel Projectile = ECC_GameTraceChannel4;
	static constexpr ECollisionChannel Drop = ECC_GameTraceChannel5;
}

