/**
 * @file ARInvaderDropTypes.h
 * @brief Shared invader drop type surface.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARInvaderDropTypes.generated.h"

/** Runtime currency category produced by invader enemy drops. */
UENUM(BlueprintType)
enum class EARInvaderDropType : uint8
{
	None = 0 UMETA(DisplayName = "None"),
	Scrap UMETA(DisplayName = "Scrap"),
	Meat UMETA(DisplayName = "Meat")
};

/** Pawn collision behavior for invader drop physics drift. */
UENUM(BlueprintType)
enum class EARInvaderDropPawnCollisionMode : uint8
{
	CollideWithPawns = 0 UMETA(DisplayName = "Collide With Pawns"),
	IgnoreAllPawns UMETA(DisplayName = "Ignore All Pawns")
};
