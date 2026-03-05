#pragma once

#include "CoreMinimal.h"
#include "ARGameStateBase.h"
#include "ARScrapyardGameState.generated.h"

UCLASS()
class ALIENRAMEN_API AARScrapyardGameState : public AARGameStateBase
{
	GENERATED_BODY()

public:
	AARScrapyardGameState();

	virtual UScriptStruct* GetStateStruct_Implementation() const override;
};
