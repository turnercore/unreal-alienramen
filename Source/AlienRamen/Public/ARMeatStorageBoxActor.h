/**
 * @file ARMeatStorageBoxActor.h
 * @brief Shop meat storage dispenser backed by replicated typed GameState meat tuples.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARColorTypes.h"
#include "ARShopDispenserActor.h"
#include "ARMeatStorageBoxActor.generated.h"

class AARPlayerController;
class AARRamenMeatActor;
class AActor;
class UStaticMeshComponent;
struct FARMeatDefinitionRow;
struct FARMeatTypeAmount;

UCLASS(Blueprintable)
class ALIENRAMEN_API AARMeatStorageBoxActor : public AARShopDispenserActor
{
	GENERATED_BODY()

public:
	AARMeatStorageBoxActor();

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop|MeatStorage", meta = (BlueprintAuthorityOnly))
	bool TryDispenseMeat(AARPlayerController* RequestingController);

	/** Dispenses a specific Item.Meat type from storage inventory. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop|MeatStorage", meta = (BlueprintAuthorityOnly))
	bool TryDispenseSpecificMeat(AARPlayerController* RequestingController, FGameplayTag MeatTag);

	/** Dispenses a random eligible typed-stock meat and applies this storage's MeatColor to the spawned world actor. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop|MeatStorage", meta = (BlueprintAuthorityOnly))
	bool TryDispenseRandomMeatByContainerColor(AARPlayerController* RequestingController);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop|MeatStorage", meta = (BlueprintAuthorityOnly))
	bool TryStoreHeldMeat(AARPlayerController* RequestingController);

	// Stores a loose world meat actor back into reserve storage (typically on storage collision/hit).
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop|MeatStorage", meta = (BlueprintAuthorityOnly))
	bool TryStoreWorldMeat(AARRamenMeatActor* MeatActor);

	// Smart interaction path used by controller/BP use flow:
	// held meat -> store back into reserve, empty hands -> dispense.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Interaction", meta = (DisplayName = "Forward Use To Controller", BlueprintAuthorityOnly))
	void ForwardUseToController(AActor* UsingActor);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop|MeatStorage", meta = (BlueprintAuthorityOnly))
	bool TryHandleStorageInteraction(AARPlayerController* RequestingController);

protected:
	virtual void BeginPlay() override;

	// Native visual/physics root for meat storage actors.
	// This replaces the inherited SceneRoot as the actor root component.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Shop|MeatStorage")
	TObjectPtr<UStaticMeshComponent> StorageRootMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Shop|MeatStorage")
	EARAffinityColor MeatColor = EARAffinityColor::Red;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Shop|MeatStorage", meta = (ClampMin = "1", UIMin = "1"))
	int32 MeatAmountPerDispense = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Shop|MeatStorage")
	TSubclassOf<AARRamenMeatActor> MeatActorClass;

	// Optional explicit Item.Meat tag for specific-dispense calls and authoring defaults.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Shop|MeatStorage", meta = (Categories = "Item.Meat"))
	FGameplayTag MeatItemTag;

	// World-hit auto-store is blocked until meat has moved at least this far from spawn.
	// Prevents freshly dispensed meat from instantly snapping back into storage when spawned near/inside the container.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Shop|MeatStorage", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MinWorldAutoStoreTravelDistance = 200.0f;

private:
	void SyncLegacyDefinition();
	bool TryDispenseResolvedMeat(AARPlayerController* RequestingController, const FARMeatDefinitionRow& MeatDefinition, EARAffinityColor MeatColorToDispense, EARVendingQualityTier MeatQualityToDispense);
	bool ConsumeTypedMeatFromState(FARMeatState& InOutMeatState, FGameplayTag MeatTag, EARAffinityColor MeatColor, EARVendingQualityTier MeatQualityTier, int32 AmountToConsume) const;
	void AddTypedMeatToState(FARMeatState& InOutMeatState, FGameplayTag MeatTag, EARAffinityColor MeatColor, EARVendingQualityTier MeatQualityTier, int32 AmountToAdd) const;
	bool SelectRandomEligibleMeatTupleFromTypedStock(const FARMeatState& MeatState, FARMeatTypeAmount& OutMeatEntry) const;
	bool TryStoreMeatActorInternal(AARRamenMeatActor* MeatActor, AARPlayerController* RequestingController, bool bRequireWorldReturnArmed);
};
