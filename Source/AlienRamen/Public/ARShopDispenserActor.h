/**
 * @file ARShopDispenserActor.h
 * @brief Generic server-authoritative shop dispenser actor for spawning carryable outputs.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARColorTypes.h"
#include "GameplayTagContainer.h"
#include "GameFramework/Actor.h"
#include "ARShopDispenserActor.generated.h"

class AARPlayerController;
class AActor;
class USceneComponent;

UENUM(BlueprintType)
enum class EARShopDispenserSourceType : uint8
{
	Unlimited = 0,
	GameStateMeatReserve,
	Custom
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARShopDispenseDefinition
{
	GENERATED_BODY()

	// Optional lookup key for this output. If Request uses an invalid tag, first entry is used.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	FGameplayTag ItemTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	TSubclassOf<AActor> SpawnActorClass;

	// When true and SpawnActorClass is unset, resolve spawn actor class from shared item definition table.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	bool bResolveSpawnActorClassFromItemDefinition = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ClampMin = "1", UIMin = "1"))
	int32 AmountPerDispense = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	EARShopDispenserSourceType SourceType = EARShopDispenserSourceType::Unlimited;

	// Source color for reserve-backed outputs (for example meat buckets).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	EARAffinityColor SourceColor = EARAffinityColor::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	bool bAutoPlaceIntoCarry = true;
};

UCLASS(Blueprintable)
class ALIENRAMEN_API AARShopDispenserActor : public AActor
{
	GENERATED_BODY()

public:
	AARShopDispenserActor();

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop|Dispenser", meta = (BlueprintAuthorityOnly))
	bool TryDispenseToController(AARPlayerController* RequestingController, FGameplayTag RequestedItemTag);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Shop|Dispenser")
	bool HasDispenseDefinition(FGameplayTag RequestedItemTag) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Shop|Dispenser")
	bool GetDispenseDefinition(FGameplayTag RequestedItemTag, FARShopDispenseDefinition& OutDefinition) const;

protected:
	// Custom source consumption hook for non-standard inventories/currencies.
	UFUNCTION(BlueprintNativeEvent, Category = "Alien Ramen|Shop|Dispenser")
	bool ConsumeCustomSource(const FARShopDispenseDefinition& Definition, int32 RequestedAmount, int32& OutGrantedAmount);
	virtual bool ConsumeCustomSource_Implementation(const FARShopDispenseDefinition& Definition, int32 RequestedAmount, int32& OutGrantedAmount);

	// Best-effort compensation hook when source was consumed but spawn failed.
	UFUNCTION(BlueprintNativeEvent, Category = "Alien Ramen|Shop|Dispenser")
	void RollbackCustomSource(const FARShopDispenseDefinition& Definition, int32 GrantedAmount);
	virtual void RollbackCustomSource_Implementation(const FARShopDispenseDefinition& Definition, int32 GrantedAmount);

	// Optional post-spawn initialization for BP-only output behaviors.
	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|Shop|Dispenser")
	void BP_OnDispensedActorSpawned(AActor* SpawnedActor, const FARShopDispenseDefinition& Definition, int32 DispensedAmount, AARPlayerController* RequestingController);

	// Native post-spawn initialization for known output types.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop|Dispenser")
	virtual void InitializeSpawnedActorFromDefinition(AActor* SpawnedActor, const FARShopDispenseDefinition& Definition, int32 DispensedAmount);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Shop|Dispenser")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Shop|Dispenser")
	TObjectPtr<USceneComponent> SpawnAnchor;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Shop|Dispenser")
	TArray<FARShopDispenseDefinition> DispenseDefinitions;

private:
	bool ConsumeSource(
		const FARShopDispenseDefinition& Definition,
		int32& InOutDispenseAmount,
		bool& bOutUsedMeatReserve,
		struct FARMeatState& OutPreConsumeMeatState);
	void RollbackSource(
		const FARShopDispenseDefinition& Definition,
		int32 DispenseAmount,
		bool bUsedMeatReserve,
		const struct FARMeatState& PreConsumeMeatState);
	const FARShopDispenseDefinition* ResolveDispenseDefinition(FGameplayTag RequestedItemTag) const;
	static class UARShopCarryComponent* ResolveCarryComponentFromController(AARPlayerController* Controller);
	static int32* ResolveMeatBucket(struct FARMeatState& MeatState, EARAffinityColor SourceColor);
};
