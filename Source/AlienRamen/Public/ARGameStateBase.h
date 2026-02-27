#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "StructSerializable.h"
#include "ARGameStateBase.generated.h"

class AARPlayerStateBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAROnTrackedPlayersChangedSignature);

UCLASS()
class ALIENRAMEN_API AARGameStateBase : public AGameStateBase, public IStructSerializable
{
	GENERATED_BODY()

public:
	AARGameStateBase();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "State Serialization")
	TObjectPtr<UScriptStruct> ClassStateStruct;

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Players", meta = (BlueprintAuthorityOnly))
	bool AddTrackedPlayer(AARPlayerStateBase* Player);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Players", meta = (BlueprintAuthorityOnly))
	bool RemoveTrackedPlayer(AARPlayerStateBase* Player);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Players")
	bool ContainsTrackedPlayer(const AARPlayerStateBase* Player) const;

	const TArray<TObjectPtr<AARPlayerStateBase>>& GetTrackedPlayers() const { return Players; }

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Players")
	FAROnTrackedPlayersChangedSignature OnTrackedPlayersChanged;

	virtual bool ApplyStateFromStruct_Implementation(const FInstancedStruct& SavedState) override;

	UFUNCTION(Server, Reliable)
	void ServerApplyStateFromStruct(const FInstancedStruct& SavedState);

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_Players();

	UPROPERTY(ReplicatedUsing = OnRep_Players, BlueprintReadOnly, Category = "Alien Ramen|Players")
	TArray<TObjectPtr<AARPlayerStateBase>> Players;
};
