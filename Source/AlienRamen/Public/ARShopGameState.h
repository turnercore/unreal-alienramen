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

	/** Runtime-visible base bowl payout used for customer serve economy UI. */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Shop|Economy")
	int32 GetBaseBowlPayout() const { return BaseBowlPayout; }

	/** Authority-only setter for runtime base bowl payout mirror. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop|Economy", meta = (BlueprintAuthorityOnly))
	void SetBaseBowlPayout(int32 NewBaseBowlPayout);

	/**
	 * Finalize shop-mode exit and request travel to invader gameplay via mode travel routing.
	 * Uses InInvaderTravelURL when provided; otherwise falls back to DefaultInvaderTravelURL.
	 * Expected destination is the gameplay map (for example /Game/Maps/Lvl_Invader), not the transition map.
	 */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop", meta = (BlueprintAuthorityOnly))
	bool FinalizeShopRunAndTravelToInvader(const FString& InInvaderTravelURL);

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_BaseBowlPayout(int32 OldBaseBowlPayout);

	// Map URL used when finalizing shop mode and launching an invader run.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Shop", meta = (AllowPrivateAccess = "true"))
	FString DefaultInvaderTravelURL = TEXT("/Game/Maps/Lvl_Invader");

	/** Replicated runtime base payout per served bowl. Defaults to 10 unless overridden by game mode. */
	UPROPERTY(ReplicatedUsing = OnRep_BaseBowlPayout, EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Shop|Economy", meta = (AllowPrivateAccess = "true", ClampMin = "0", UIMin = "0"))
	int32 BaseBowlPayout = 10;
};
