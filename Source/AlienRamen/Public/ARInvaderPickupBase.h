/**
 * @file ARInvaderPickupBase.h
 * @brief Invader-only pickup base for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ARInvaderPickupBase.generated.h"

UCLASS(Blueprintable)
class ALIENRAMEN_API AARInvaderPickupBase : public AActor
{
	GENERATED_BODY()

public:
	AARInvaderPickupBase();

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader|Pickup|Lifecycle")
	void EvaluateOffscreenRelease(float DeltaSeconds);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Invader|Pickup|Lifecycle")
	bool IsOutsideGameplayBounds() const;

	// Final lifecycle release step for pickup cleanup. Override for pooling.
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Alien Ramen|Invader|Pickup|Lifecycle")
	void ReleasePickup();

	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|Invader|Pickup|Lifecycle")
	void BP_OnPickupPreRelease();

protected:
	virtual void BeginPlay() override;

	void ReleasePickup_Implementation();

protected:
	// If true, this pickup releases itself after spending OffscreenReleaseDelay outside gameplay bounds.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Invader|Pickup|Culling")
	bool bReleaseWhenOutsideGameplayBounds = true;

	// If true, OffscreenReleaseDelay is pulled from project settings at BeginPlay.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Invader|Pickup|Culling")
	bool bUseProjectSettingsOffscreenCullSeconds = true;

	// Seconds a pickup can stay offscreen before being released.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Invader|Pickup|Culling", meta = (ClampMin = "0.0"))
	float OffscreenReleaseDelay = 0.1f;

	// Additional XY margin around gameplay bounds before offscreen timing starts.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Invader|Pickup|Culling", meta = (ClampMin = "0.0"))
	float OffscreenReleaseMargin = 0.f;

	// If true, only authority performs release checks (recommended for replicated pickups).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Invader|Pickup|Culling")
	bool bOffscreenCheckAuthorityOnly = true;

private:
	bool bReleased = false;
	float OffscreenSeconds = 0.f;
};
