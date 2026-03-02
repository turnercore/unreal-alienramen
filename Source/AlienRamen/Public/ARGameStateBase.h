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
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FAROnPlayerReadyChangedSignature,
	AARPlayerStateBase*,
	SourcePlayerState,
	EARPlayerSlot,
	SourcePlayerSlot,
	bool,
	bNewReady,
	bool,
	bOldReady);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FAROnAllPlayersTravelReadyChangedSignature,
	bool,
	bNewAllPlayersTravelReady,
	bool,
	bOldAllPlayersTravelReady);

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
	TArray<AARPlayerStateBase*> GetPlayerStates() const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Players")
	bool AreAllPlayersTravelReady() const;

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

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Players")
	FAROnPlayerReadyChangedSignature OnPlayerReadyChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Players")
	FAROnAllPlayersTravelReadyChangedSignature OnAllPlayersTravelReadyChanged;

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
	void HandlePlayerReadyStatusChanged(AARPlayerStateBase* SourcePlayerState, EARPlayerSlot SourcePlayerSlot, bool bNewReady, bool bOldReady);

	UFUNCTION()
	void HandlePlayerSlotChanged(EARPlayerSlot NewSlot, EARPlayerSlot OldSlot);

	UFUNCTION()
	void HandlePlayerCharacterPickedChanged(AARPlayerStateBase* SourcePlayerState, EARPlayerSlot SourcePlayerSlot, EARCharacterChoice NewCharacter, EARCharacterChoice OldCharacter);

	UFUNCTION()
	void OnRep_AllPlayersTravelReady(bool bOldAllPlayersTravelReady);

	UFUNCTION()
	void OnRep_CyclesForUI();

	void BindPlayerStateSignals(AARPlayerStateBase* PlayerState);
	void UnbindPlayerStateSignals(AARPlayerStateBase* PlayerState);
	bool ComputeAllPlayersTravelReady() const;
	void RefreshAllPlayersTravelReady();

	UPROPERTY(ReplicatedUsing = OnRep_AllPlayersTravelReady, BlueprintReadOnly, Category = "Alien Ramen|Players")
	bool bAllPlayersTravelReady = false;

	UPROPERTY(ReplicatedUsing = OnRep_CyclesForUI, BlueprintReadOnly, Category = "Alien Ramen|Save")
	int32 CyclesForUI = 0;
};
