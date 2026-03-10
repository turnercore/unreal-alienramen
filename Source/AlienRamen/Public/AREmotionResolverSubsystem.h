/**
 * @file AREmotionResolverSubsystem.h
 * @brief Shared emotion icon resolver/cache backed by a configured DataTable.
 */
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/SoftObjectPtr.h"
#include "AREmotionResolverSubsystem.generated.h"

class UTexture2D;

UCLASS()
class ALIENRAMEN_API UAREmotionResolverSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Emotion")
	void RebuildCache();

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Emotion")
	bool TryResolveEmotionIcon(
		FGameplayTag RequestedEmotionTag,
		TSoftObjectPtr<UTexture2D>& OutIconTexture,
		FGameplayTag& OutResolvedEmotionTag);

	// Fallback helper for contexts without a game instance subsystem (for example some editor preview paths).
	static bool TryResolveEmotionIconFromConfiguredData(
		FGameplayTag RequestedEmotionTag,
		TSoftObjectPtr<UTexture2D>& OutIconTexture,
		FGameplayTag& OutResolvedEmotionTag);

private:
	bool EnsureCacheBuilt();
	bool BuildCache();

	UPROPERTY(Transient)
	TMap<FGameplayTag, TSoftObjectPtr<UTexture2D>> IconByEmotionTag;

	// Caches previous request outcomes (including misses) for frequent lookups.
	TMap<FGameplayTag, FGameplayTag> RequestToResolvedTagCache;
	TMap<FGameplayTag, TSoftObjectPtr<UTexture2D>> RequestToResolvedIconCache;
	TSet<FGameplayTag> RequestMissCache;

	bool bCacheBuilt = false;
};
