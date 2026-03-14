/**
 * @file EmoResolverSubsystem.h
 * @brief Shared emotion icon resolver/cache backed by a configured DataTable.
 */
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/SoftObjectPtr.h"
#include "EmoResolverSubsystem.generated.h"

class UTexture2D;
class UDataTable;
class IConsoleObject;
class UEmoComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEmoOnAnyEmotionChanged, UEmoComponent*, Component, FGameplayTag, NewEmotionTag);

UCLASS()
class EMO_API UEmoResolverSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Rebuilds icon cache from configured DataTable and clears request caches. Call after changing emotion data. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|Emotion", meta = (ToolTip = "Executes an emotion-system operation."))
	void RebuildCache();

	/** Writes cache stats to log (routes through console command too). Useful for debugging missing icons. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|Emotion", meta = (ToolTip = "Executes an emotion-system operation."))
	void LogCacheStats() const;

	/**
	 * Resolve an emotion tag to an icon (soft texture) and the final resolved tag (after fallback).
	 * Returns false when no icon is found; OutResolvedEmotionTag may still return fallback tag.
	 */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|Emotion", meta = (ToolTip = "Executes an emotion-system operation."))
	bool TryResolveEmotionIcon(
		FGameplayTag RequestedEmotionTag,
		TSoftObjectPtr<UTexture2D>& OutIconTexture,
		FGameplayTag& OutResolvedEmotionTag);

	/** Broadcast when any observed UEmoComponent changes its effective displayed emotion tag. */
	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Dialogue|Emotion", meta = (ToolTip = "Broadcast when any UEmoComponent reports an effective displayed emotion tag change. Params: Component, NewEmotionTag."))
	FEmoOnAnyEmotionChanged OnAnyEmotionChanged;

	// Fallback helper for contexts without a game instance subsystem (for example some editor preview paths).
	static bool TryResolveEmotionIconFromConfiguredData(
		FGameplayTag RequestedEmotionTag,
		TSoftObjectPtr<UTexture2D>& OutIconTexture,
		FGameplayTag& OutResolvedEmotionTag);

	void NotifyComponentEmotionChanged(UEmoComponent* Component, FGameplayTag NewEmotionTag);

private:
	void RegisterDebugConsoleCommands();
	void UnregisterDebugConsoleCommands();
	void HandleEmotionDataTableChanged();
	void BindToConfiguredDataTable(class UDataTable* DataTable);
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
	FGameplayTag CachedResolverRootTag;
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
