#pragma once

#include "CoreMinimal.h"
#include "ARPlayerStateBase.h"
#include "GameFramework/GameStateBase.h"
#include "StructSerializable.h"
#include "ARGameStateBase.generated.h"

class AARPlayerStateBase;
class APlayerController;
class APawn;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAROnTrackedPlayersChangedSignature);

UCLASS()
class ALIENRAMEN_API AARGameStateBase : public AGameStateBase, public IStructSerializable
{
	GENERATED_BODY()

public:
	AARGameStateBase();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "State Serialization")
	TObjectPtr<UScriptStruct> ClassStateStruct;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Players")
	AARPlayerStateBase* GetPlayerBySlot(EARPlayerSlot Slot) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Players")
	AARPlayerStateBase* GetOtherPlayerStateFromPlayerState(const AARPlayerStateBase* CurrentPlayerState) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Players")
	AARPlayerStateBase* GetOtherPlayerStateFromController(const APlayerController* CurrentPlayerController) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Players")
	AARPlayerStateBase* GetOtherPlayerStateFromPawn(const APawn* CurrentPlayerPawn) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Players")
	AARPlayerStateBase* GetOtherPlayerStateFromContext(const UObject* CurrentPlayerContext) const;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Players")
	FAROnTrackedPlayersChangedSignature OnTrackedPlayersChanged;

	virtual bool ApplyStateFromStruct_Implementation(const FInstancedStruct& SavedState) override;

	UFUNCTION(Server, Reliable)
	void ServerApplyStateFromStruct(const FInstancedStruct& SavedState);

protected:
	virtual void BeginPlay() override;
	virtual void AddPlayerState(APlayerState* PlayerState) override;
	virtual void RemovePlayerState(APlayerState* PlayerState) override;
};
