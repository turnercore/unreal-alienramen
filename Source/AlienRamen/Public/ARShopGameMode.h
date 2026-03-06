/**
 * @file ARShopGameMode.h
 * @brief ARShopGameMode header for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARGameModeBase.h"
#include "ARShopGameMode.generated.h"

UCLASS()
class ALIENRAMEN_API AARShopGameMode : public AARGameModeBase
{
	GENERATED_BODY()

public:
	AARShopGameMode();

protected:
	virtual bool PreStartTravel(const FString& URL, const FString& Options, bool bSkipReadyChecks) override;
};
