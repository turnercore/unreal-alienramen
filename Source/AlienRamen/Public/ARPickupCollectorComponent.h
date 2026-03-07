/**
 * @file ARPickupCollectorComponent.h
 * @brief Player-side invader pickup detection and collection requests.
 */
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ARPickupCollectorComponent.generated.h"

class AARInvaderDropBase;
class AARPlayerCharacterInvader;
class UAbilitySystemComponent;
class UPrimitiveComponent;
class USphereComponent;
struct FHitResult;
struct FOnAttributeChangeData;

/**
 * Lightweight collector attached to invader player pawns.
 * - Uses an overlap sphere detector for nearby drops.
 * - Subscribes to owner ASC PickupRadius attribute changes and updates detector radius immediately.
 * - Locally-controlled client performs predicted visual collection and sends server request.
 * - Server validates range and starts authoritative collection/reward flow.
 */
UCLASS(ClassGroup = (AlienRamen), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class ALIENRAMEN_API UARPickupCollectorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UARPickupCollectorComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	AARPlayerCharacterInvader* GetOwnerInvader() const;
	void SetupDetectorComponent();
	void EnsurePickupRadiusBinding();
	void UnbindPickupRadiusDelegate();
	float ResolvePickupRadius() const;
	void RefreshDetectorRadius();
	void StartRetryTimer();
	void StopRetryTimer();
	void HandleRetryTimerTick();
	void ProcessAllOverlappingDrops();
	void TryProcessDrop(AARInvaderDropBase* Drop);
	void PrunePendingRequests();
	bool ShouldSendServerRequestForDrop(const AARInvaderDropBase* Drop, float WorldTimeSeconds) const;

	void HandlePickupRadiusAttributeChanged(const FOnAttributeChangeData& ChangeData);

	UFUNCTION()
	void HandleDetectorBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleDetectorEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

private:
	// If true, owning client triggers immediate local visual collect while waiting for server authority.
	UPROPERTY(EditAnywhere, Category = "Alien Ramen|Invader|Drop")
	bool bEnableClientPrediction = true;

	// Minimum seconds between repeated server pickup requests for the same drop.
	UPROPERTY(EditAnywhere, Category = "Alien Ramen|Invader|Drop", meta = (ClampMin = "0.01"))
	float ServerRequestRetrySeconds = 0.10f;

	// Fallback range if ASC radius is unavailable/non-positive.
	UPROPERTY(EditAnywhere, Category = "Alien Ramen|Invader|Drop", meta = (ClampMin = "0.0"))
	float FallbackPickupRadius = 150.0f;

	UPROPERTY(Transient)
	TObjectPtr<USphereComponent> PickupDetector = nullptr;

	float CachedPickupRadius = 0.0f;
	FTimerHandle RetryTimerHandle;
	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
	FDelegateHandle PickupRadiusChangedDelegateHandle;
	TSet<TWeakObjectPtr<AARInvaderDropBase>> OverlappingDrops;
	TMap<TWeakObjectPtr<const AARInvaderDropBase>, float> LastServerRequestByDrop;
};
