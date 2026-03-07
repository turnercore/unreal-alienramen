/**
 * @file ARInvaderDropBase.h
 * @brief Replicated base actor for invader currency drops.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARColorTypes.h"
#include "ARInvaderDropTypes.h"
#include "ARPickupBase.h"
#include "GameplayTagContainer.h"
#include "ARInvaderDropBase.generated.h"

class AARPlayerCharacterInvader;
class USphereComponent;

/**
 * Shared invader drop actor used for scrap/meat currency pickups.
 * - Authority owns collection and reward application.
 * - Actor stays in XY plane using physics until collection starts.
 * - Collection transitions to short homing lerp and then destroys.
 */
UCLASS(Blueprintable)
class ALIENRAMEN_API AARInvaderDropBase : public AARPickupBase
{
	GENERATED_BODY()

public:
	AARInvaderDropBase();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Authority-only runtime setup called by invader kill/drop flow. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader|Drop", meta = (BlueprintAuthorityOnly))
	void InitializeDrop(EARInvaderDropType InDropType, int32 InDropAmount, EARAffinityColor InDropColor);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Invader|Drop")
	EARInvaderDropType GetDropType() const { return DropType; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Invader|Drop")
	int32 GetDropAmount() const { return DropAmount; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Invader|Drop")
	EARAffinityColor GetDropColor() const { return DropColor; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Invader|Drop")
	bool IsAvailableForCollection() const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Invader|Drop")
	bool IsPlayerWithinPickupRange2D(const AARPlayerCharacterInvader* Player) const;

	/** Authority-only collection path. Returns true if collection started. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader|Drop", meta = (BlueprintAuthorityOnly))
	bool TryCollectByPlayer(AARPlayerCharacterInvader* InCollectingPlayer);

	/** Local cosmetic-only prediction path used by owning clients before server confirmation. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader|Drop")
	void BeginPredictedCollectionLocal(AARPlayerCharacterInvader* InCollectingPlayer);

	// Enables/disables custom earth-directed gravity force on authority.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader|Drop", meta = (BlueprintAuthorityOnly))
	void SetEarthGravityEnabled(bool bEnabled);

	/** Final authority reward apply hook before the pickup is released. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Alien Ramen|Invader|Drop", meta = (BlueprintAuthorityOnly))
	void ApplyDropReward();

	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|Invader|Drop")
	void BP_OnCollectionStarted(AARPlayerCharacterInvader* CollectingPlayer);

	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|Invader|Drop")
	void BP_OnRewardApplied(AARPlayerCharacterInvader* CollectingPlayer);

protected:
	virtual void BeginPlay() override;
	UFUNCTION()
	void OnRep_IsCollecting();

	void ApplyDropReward_Implementation();
	void DisablePhysicsAndCollisionForCollection();
	void RestorePostPredictionVisualState();
	void ResolveInvaderGravityFrameFromSettings();
	void ApplyEarthGravityForce(float DeltaSeconds);

private:
	float ResolvePickupRadiusForPlayer(const AARPlayerCharacterInvader* Player) const;
	void BeginCollection(AARPlayerCharacterInvader* InCollectingPlayer);
	void TickCollection(float DeltaSeconds);
	void FinalizeCollection();
	void ApplyCollectionGameplayCue(AARPlayerCharacterInvader* InCollectingPlayer) const;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Invader|Drop", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USphereComponent> PhysicsSphere;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Invader|Drop", meta = (AllowPrivateAccess = "true"))
	EARInvaderDropType DropType = EARInvaderDropType::None;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Invader|Drop", meta = (AllowPrivateAccess = "true"))
	int32 DropAmount = 0;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Invader|Drop", meta = (AllowPrivateAccess = "true"))
	EARAffinityColor DropColor = EARAffinityColor::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Invader|Drop|Pickup", meta = (AllowPrivateAccess = "true", ClampMin = "0.01"))
	float CollectionDuration = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Invader|Drop|Pickup", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float FallbackPickupRadius = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Invader|Drop|Pickup", meta = (AllowPrivateAccess = "true"))
	FGameplayTag CollectionGameplayCueTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Invader|Drop|Pickup", meta = (AllowPrivateAccess = "true"))
	float CollectionTargetZOffset = 0.0f;

	UPROPERTY(ReplicatedUsing = OnRep_IsCollecting, VisibleAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Invader|Drop", meta = (AllowPrivateAccess = "true"))
	bool bIsCollecting = false;

	bool bRewardApplied = false;
	float CollectionElapsed = 0.0f;
	FVector CollectionStartLocation = FVector::ZeroVector;
	TWeakObjectPtr<AARPlayerCharacterInvader> CollectingPlayerPtr;

	bool bPredictedLocalCollectionVisual = false;
	float PredictedLocalCollectionElapsed = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Invader|Drop|Pickup", meta = (AllowPrivateAccess = "true", ClampMin = "0.05"))
	float PredictedCollectionRollbackSeconds = 0.35f;

	bool bEarthGravityEnabled = false;
	FVector EarthGravityDirection = FVector(-1.0f, 0.0f, 0.0f);
	float EarthGravityAcceleration = 980.0f;
};
