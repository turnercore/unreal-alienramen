/**
 * @file ARMeatStorageBoxActor.h
 * @brief Shop meat storage dispenser backed by replicated GameState meat buckets.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARColorTypes.h"
#include "GameFramework/Actor.h"
#include "ARMeatStorageBoxActor.generated.h"

class AARPlayerController;
class AARRamenMeatActor;
class USceneComponent;

UCLASS(Blueprintable)
class ALIENRAMEN_API AARMeatStorageBoxActor : public AActor
{
	GENERATED_BODY()

public:
	AARMeatStorageBoxActor();

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop|MeatStorage", meta = (BlueprintAuthorityOnly))
	bool TryDispenseMeat(AARPlayerController* RequestingController);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Shop|MeatStorage")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Shop|MeatStorage")
	TObjectPtr<USceneComponent> SpawnAnchor;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Shop|MeatStorage")
	EARAffinityColor MeatColor = EARAffinityColor::Red;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Shop|MeatStorage", meta = (ClampMin = "1", UIMin = "1"))
	int32 MeatAmountPerDispense = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Shop|MeatStorage")
	TSubclassOf<AARRamenMeatActor> MeatActorClass;
};
