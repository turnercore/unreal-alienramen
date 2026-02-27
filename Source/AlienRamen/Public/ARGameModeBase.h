#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ARPlayerStateBase.h"
#include "ARGameModeBase.generated.h"

class AARGameStateBase;

UCLASS()
class ALIENRAMEN_API AARGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

protected:
	UFUNCTION(BlueprintNativeEvent, Category = "Alien Ramen|Players")
	void BP_OnPlayerJoined(AARPlayerStateBase* JoinedPlayerState);
	virtual void BP_OnPlayerJoined_Implementation(AARPlayerStateBase* JoinedPlayerState);

	UFUNCTION(BlueprintNativeEvent, Category = "Alien Ramen|Players")
	void BP_OnPlayerLeft(AARPlayerStateBase* LeftPlayerState);
	virtual void BP_OnPlayerLeft_Implementation(AARPlayerStateBase* LeftPlayerState);

private:
	static EARPlayerSlot DetermineNextPlayerSlot(const AARGameStateBase* GameState);
};
