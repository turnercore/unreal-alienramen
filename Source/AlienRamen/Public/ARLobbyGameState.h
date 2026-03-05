#pragma once

#include "CoreMinimal.h"
#include "ARGameStateBase.h"
#include "ARLobbyGameState.generated.h"

UCLASS()
class ALIENRAMEN_API AARLobbyGameState : public AARGameStateBase
{
	GENERATED_BODY()

public:
	AARLobbyGameState();

	virtual UScriptStruct* GetStateStruct_Implementation() const override;
};
