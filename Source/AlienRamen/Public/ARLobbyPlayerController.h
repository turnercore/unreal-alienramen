#pragma once
/**
 * @file ARLobbyPlayerController.h
 * @brief ARLobbyPlayerController header for Alien Ramen.
 */

#include "CoreMinimal.h"
#include "ARPlayerController.h"
#include "ARLobbyPlayerController.generated.h"

UCLASS()
class ALIENRAMEN_API AARLobbyPlayerController : public AARPlayerController
{
	GENERATED_BODY()

public:
	AARLobbyPlayerController();
};
