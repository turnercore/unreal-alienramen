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
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAROnGameStateHydratedSignature);

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

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Save")
	FAROnGameStateHydratedSignature OnHydratedFromSave;

	// Non-persistent UI mirror of save-owned cycles; authority-only setter.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Save", meta = (BlueprintAuthorityOnly))
	void SyncCyclesFromSave(int32 NewCycles);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Save")
	int32 GetCyclesForUI() const { return CyclesForUI; }

	virtual bool ApplyStateFromStruct_Implementation(const FInstancedStruct& SavedState) override;

	UFUNCTION(Server, Reliable)
	void ServerApplyStateFromStruct(const FInstancedStruct& SavedState);

	// Called by SaveSubsystem after hydration completes to inform UI.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Save")
	void NotifyHydratedFromSave();

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void AddPlayerState(APlayerState* PlayerState) override;
	virtual void RemovePlayerState(APlayerState* PlayerState) override;

	UFUNCTION()
	void OnRep_Players();

	UFUNCTION()
	void OnRep_CyclesForUI();

	UPROPERTY(ReplicatedUsing = OnRep_Players, BlueprintReadOnly, Category = "Alien Ramen|Players")
	TArray<TObjectPtr<AARPlayerStateBase>> Players;

	UPROPERTY(ReplicatedUsing = OnRep_CyclesForUI, BlueprintReadOnly, Category = "Alien Ramen|Save")
	int32 CyclesForUI = 0;
};
