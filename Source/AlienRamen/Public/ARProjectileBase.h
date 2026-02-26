#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ARProjectileBase.generated.h"

UCLASS(Blueprintable)
class ALIENRAMEN_API AARProjectileBase : public AActor
{
	GENERATED_BODY()

public:
	AARProjectileBase();

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Projectile|Lifecycle")
	void EvaluateOffscreenRelease();

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Projectile|Lifecycle")
	bool IsOutsideGameplayBounds() const;

	// Final lifecycle release step for projectile cleanup. Override for pooling.
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Alien Ramen|Projectile|Lifecycle")
	void ReleaseProjectile();

	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|Projectile|Lifecycle")
	void BP_OnProjectilePreRelease();

protected:
	virtual void BeginPlay() override;

	void ReleaseProjectile_Implementation();

protected:
	// If true, this projectile releases itself the moment it is outside gameplay bounds.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Projectile|Culling")
	bool bReleaseWhenOutsideGameplayBounds = true;

	// If true, OffscreenReleaseDelay is pulled from project settings at BeginPlay.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Projectile|Culling")
	bool bUseProjectSettingsOffscreenCullSeconds = true;

	// Seconds a projectile can stay offscreen before being released.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Projectile|Culling", meta = (ClampMin = "0.0"))
	float OffscreenReleaseDelay = 0.1f;

	// Additional XY margin around gameplay bounds before offscreen release triggers.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Projectile|Culling", meta = (ClampMin = "0.0"))
	float OffscreenReleaseMargin = 0.f;

	// If true, only authority performs release checks (recommended for replicated projectiles).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Projectile|Culling")
	bool bOffscreenCheckAuthorityOnly = true;

private:
	void EvaluateOffscreenReleaseInternal(float DeltaSeconds);

	bool bReleased = false;
	float OffscreenSeconds = 0.f;
};
