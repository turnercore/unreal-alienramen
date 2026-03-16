/**
 * @file ARShopGameState.h
 * @brief ARShopGameState header for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARGameStateBase.h"
#include "ARShopGameState.generated.h"

UCLASS()
class ALIENRAMEN_API AARShopGameState : public AARGameStateBase
{
	GENERATED_BODY()

public:
	AARShopGameState();

	virtual UScriptStruct* GetStateStruct_Implementation() const override;

	/**
	 * Finalize shop-mode exit and request travel to invader gameplay via mode travel routing.
	 * Uses InInvaderTravelURL when provided; otherwise falls back to DefaultInvaderTravelURL.
	 * Expected destination is the gameplay map (for example /Game/Maps/Lvl_Invader), not the transition map.
	 */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop", meta = (BlueprintAuthorityOnly))
	bool FinalizeShopRunAndTravelToInvader(const FString& InInvaderTravelURL);

protected:
	// Map URL used when finalizing shop mode and launching an invader run.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Shop", meta = (AllowPrivateAccess = "true"))
	FString DefaultInvaderTravelURL = TEXT("/Game/Maps/Lvl_Invader");
};
