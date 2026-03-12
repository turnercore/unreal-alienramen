/**
 * @file ARRamenMeatActor.h
 * @brief Replicated shop meat pickup used by station loading and storage boxes.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARColorTypes.h"
#include "ARShopCarryItemBase.h"
#include "ARRamenMeatActor.generated.h"

class UPrimitiveComponent;
struct FHitResult;

UCLASS(Blueprintable)
class ALIENRAMEN_API AARRamenMeatActor : public AARShopCarryItemBase
{
	GENERATED_BODY()

public:
	AARRamenMeatActor();

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop|Meat", meta = (BlueprintAuthorityOnly))
	void SetMeatData(EARAffinityColor NewColor, int32 NewAmount);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Shop|Meat")
	EARAffinityColor GetMeatColor() const { return MeatColor; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Shop|Meat")
	int32 GetMeatAmount() const { return MeatAmount; }

	// True once this meat has moved at least RequiredDistance units away from spawn.
	// Used by storage auto-return gating to avoid instant re-store at spawn time.
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Shop|Meat")
	bool HasMovedAwayForStorageReturn(float RequiredDistance) const;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void NotifyHit(
		UPrimitiveComponent* MyComp,
		AActor* Other,
		UPrimitiveComponent* OtherComp,
		bool bSelfMoved,
		FVector HitLocation,
		FVector HitNormal,
		FVector NormalImpulse,
		const FHitResult& Hit) override;
	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "Alien Ramen|Shop|Meat")
	EARAffinityColor MeatColor = EARAffinityColor::Red;

	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "Alien Ramen|Shop|Meat", meta = (ClampMin = "1", UIMin = "1"))
	int32 MeatAmount = 1;

private:
	void TryAutoStoreWithActor(AActor* OtherActor);

	FVector SpawnLocationForStorageReturn = FVector::ZeroVector;
	float MaxDistanceFromSpawnSq = 0.0f;
};
