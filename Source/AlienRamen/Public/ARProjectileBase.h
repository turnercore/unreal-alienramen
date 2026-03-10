/**
 * @file ARProjectileBase.h
 * @brief ARProjectileBase header for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StructUtils/InstancedStruct.h"
#include "ARProjectileBase.generated.h"

UCLASS(Blueprintable)
class ALIENRAMEN_API AARProjectileBase : public AActor
{
	GENERATED_BODY()

public:
	AARProjectileBase();

	virtual void Tick(float DeltaSeconds) override;

	// Applies projectile init data by property-name matching to this actor and its projectile-movement components.
	// Intended to be called from lifecycle init flows that provide FInstancedStruct payloads.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Projectile|Init")
	bool InitializeProjectileFromData(const FInstancedStruct& InitData);

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

	// If true, BeginPlay auto-wires runtime component setup:
	// picks a collision primitive, uses it as root when root is missing, and assigns projectile movement UpdatedComponent.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Projectile|Collision")
	bool bAutoWireCollisionAndMovement = true;

	// If true, BeginPlay applies invader-safe default collision responses:
	// projectile ignores other projectiles and drops. Disable in BP to fully own collision setup.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Projectile|Collision")
	bool bApplyDefaultInvaderCollisionResponses = true;

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
