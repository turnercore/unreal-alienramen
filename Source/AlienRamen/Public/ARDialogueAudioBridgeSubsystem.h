/**
 * @file ARDialogueAudioBridgeSubsystem.h
 * @brief Local dialogue-audio bridge for native and FMOD-backed signal playback.
 */
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ParleyDialogueTypes.h"
#include "ARDialogueAudioBridgeSubsystem.generated.h"

class APlayerController;
class UDataTable;

UCLASS()
class ALIENRAMEN_API UARDialogueAudioBridgeSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Local client entrypoint for dialogue audio requests dispatched by controller RPC bridge.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|Audio", meta = (ToolTip = "Handles local dialogue audio requests and routes to native sound or FMOD playback based on Parley dialogue audio mode."))
	bool HandleLocalDialogueAudioRequest(APlayerController* SourceController, const FDialogueAudioRequest& Request);

private:
	bool IsDuplicateLocalLine(const FDialogueAudioRequest& Request, double NowSeconds) const;
	void RecordDeliveredLine(const FDialogueAudioRequest& Request, double NowSeconds);
	void PruneDeliveredLineCache(double NowSeconds);
	void HandleCueTableChanged();
	void BindToCueTable(UDataTable* DataTable);
	void UnbindCueTableChangedDelegate();
	bool RebuildCueMapIfNeeded();
	bool ResolveSignalEventAssetForCue(const FGameplayTag& CueTag, UObject*& OutEventAsset);
	bool PlayFMODEvent2DByReflection(UObject* WorldContextObject, UObject* EventAsset) const;

	mutable TMap<FString, double> DeliveredLineTimeByKey;
	TWeakObjectPtr<UDataTable> BoundCueTable;
	FDelegateHandle CueTableChangedHandle;
	FSoftObjectPath CachedCueTablePath;
	TMap<FGameplayTag, TSoftObjectPtr<UObject>> CueToSignalEventAsset;
	TMap<FGameplayTag, TWeakObjectPtr<UObject>> ResolvedSignalEventByCue;
	bool bCueMapInitialized = false;
};
