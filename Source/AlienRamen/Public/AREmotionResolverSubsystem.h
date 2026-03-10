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
class UDataTable;
class IConsoleObject;

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
	void LogCacheStats() const;

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
	void RegisterDebugConsoleCommands();
	void UnregisterDebugConsoleCommands();
	void HandleEmotionDataTableChanged();
	void BindToConfiguredDataTable();
	void UnbindDataTableChangedDelegate();
	bool HasConfigInputsChanged() const;
	bool EnsureCacheBuilt();
	bool BuildCache();

	UPROPERTY(Transient)
	TMap<FGameplayTag, TSoftObjectPtr<UTexture2D>> IconByEmotionTag;

	// Caches previous request outcomes (including misses) for frequent lookups.
	TMap<FGameplayTag, FGameplayTag> RequestToResolvedTagCache;
	TMap<FGameplayTag, TSoftObjectPtr<UTexture2D>> RequestToResolvedIconCache;
	TSet<FGameplayTag> RequestMissCache;

	TWeakObjectPtr<class UDataTable> BoundDataTable;
	FDelegateHandle DataTableChangedHandle;
	FSoftObjectPath CachedEmotionDataTablePath;
	FGameplayTag CachedGenericRootTag;

	IConsoleObject* CmdLogCacheStats = nullptr;
	IConsoleObject* CmdRebuildCache = nullptr;
	uint64 CacheBuildCount = 0;
	uint64 LookupCount = 0;
	uint64 CacheHitCount = 0;
	uint64 CacheMissCount = 0;
	uint64 CacheInvalidationCount = 0;

	bool bCacheBuilt = false;
};
