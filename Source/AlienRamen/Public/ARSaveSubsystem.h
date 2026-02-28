#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ARSaveTypes.h"
#include "ARSaveSubsystem.generated.h"

class UARSaveGame;
class UARSaveIndexGame;
class AARGameStateBase;
class AARPlayerStateBase;
class AARPlayerController;
class UWorld;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAROnSaveOperationCompleted, const FARSaveResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAROnSaveOperationFailed, const FARSaveResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAROnGameLoaded);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAROnSaveOperationStarted);

UCLASS()
class ALIENRAMEN_API UARSaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static constexpr int32 DefaultUserIndex = 0;

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Save")
	bool CreateNewSave(FName DesiredSlotBase, FARSaveSlotDescriptor& OutSlot, FARSaveResult& OutResult);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Save")
	bool SaveCurrentGame(FName SlotBaseName, bool bCreateNewRevision, FARSaveResult& OutResult);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Save")
	bool LoadGame(FName SlotBaseName, int32 RevisionOrLatest, FARSaveResult& OutResult);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Save")
	bool ListSaves(TArray<FARSaveSlotDescriptor>& OutSlots, FARSaveResult& OutResult) const;

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Save")
	bool ListDebugSaves(TArray<FARSaveSlotDescriptor>& OutSlots, FARSaveResult& OutResult) const;

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Save")
	bool DeleteSave(FName SlotBaseName, FARSaveResult& OutResult);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Save")
	UARSaveGame* GetCurrentSaveGame() const { return CurrentSaveGame; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Save")
	bool HasCurrentSave() const { return CurrentSaveGame != nullptr; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Save")
	FName GetCurrentSlotBaseName() const { return CurrentSlotBaseName; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Save")
	int32 GetCurrentSlotRevision() const;

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Save")
	FName GenerateRandomSlotBaseName(bool bEnsureUnique = true);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Save")
	int32 GetMaxBackupRevisions() const;

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Save")
	void SetMaxBackupRevisions(int32 NewMaxBackups);

	// Returns true and fills OutElapsed when a save exists; false when no current save/timestamp.
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Save")
	bool GetTimeSinceLastSave(FTimespan& OutElapsed) const;

	// Returns true if we have a timestamp and outputs it (UTC).
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Save")
	bool GetLastSaveTimestamp(FDateTime& OutTimestampUtc) const;

	// Returns localized text for elapsed time since last save; false if no save.
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Save")
	bool FormatTimeSinceLastSave(FText& OutText) const;

	// Whether a save is currently running (authority only, sync path).
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Save")
	bool IsSaveInProgress() const { return bSaveInProgress; }

	// Marks save dirty; autosave can later persist.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Save")
	void MarkSaveDirty();

	// Attempts an autosave only if dirty; returns true if a save was executed.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Save", meta = (BlueprintAuthorityOnly))
	bool RequestAutosaveIfDirty(bool bCreateNewRevision, FARSaveResult& OutResult);

	// Increments the canonical save's cycle counter. Authority only; can optionally persist immediately.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Save", meta = (BlueprintAuthorityOnly))
	bool IncrementSaveCycles(int32 Delta, bool bSaveAfterIncrement, FARSaveResult& OutResult);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Save")
	void RequestGameStateHydration(AARGameStateBase* Requester);

	/**
	 * Authority-only travel helper used by UI/Blueprints.
	 *
	 * Flow: optional readiness gate -> capture GameState travel snapshot -> save (create if missing) -> ServerTravel with enforced listen option.
	 */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Travel", meta = (BlueprintAuthorityOnly))
	bool RequestServerTravel(const FString& URL, bool bSkipReadyChecks = false, bool bAbsolute = false, bool bSkipGameNotify = false);

	/**
	 * Authority-only non-networked level open. Enforces listen option so host remains authoritative.
	 */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Travel", meta = (BlueprintAuthorityOnly))
	bool RequestOpenLevel(const FString& LevelName, bool bSkipReadyChecks = false, bool bAbsolute = false);

	// Sets travel-transient GameState data to be applied first on next RequestGameStateHydration call.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Save")
	void SetPendingTravelGameStateData(const FInstancedStruct& PendingStateData);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Save")
	void ClearPendingTravelGameStateData();

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Save")
	bool HasPendingTravelGameStateData() const { return PendingTravelGameStateData.IsValid(); }

	// Applies player-specific save payload onto Requester if identity (or optional slot fallback) is found in CurrentSaveGame.
	// Returns true when a matching player row was found and applied.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Save")
	bool TryHydratePlayerStateFromCurrentSave(AARPlayerStateBase* Requester, bool bAllowSlotFallback = true);

	// Server-authoritative helper: sends current canonical snapshot to a specific player controller.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Save")
	bool PushCurrentSaveToPlayer(AARPlayerController* TargetPlayerController, FARSaveResult& OutResult);

	// Allows server to replicate canonical snapshot bytes to local client storage endpoints.
	bool PersistCanonicalSaveFromBytes(const TArray<uint8>& SaveBytes, FName SlotBaseName, int32 SlotNumber, FARSaveResult& OutResult);

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Save")
	FAROnSaveOperationCompleted OnSaveCompleted;

	// Fires when a save starts (before disk write).
	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Save")
	FAROnSaveOperationStarted OnSaveStarted;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Save")
	FAROnSaveOperationCompleted OnLoadCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Save")
	FAROnSaveOperationFailed OnSaveFailed;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Save")
	FAROnSaveOperationFailed OnLoadFailed;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Save")
	FAROnGameLoaded OnGameLoaded;

protected:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	bool ArePlayersReadyForTravel(bool bSkipReadyChecks, FString& OutError) const;
	bool CaptureGameStateForTravel(UWorld* World);
	static FString EnsureListenOption(const FString& InURLOrOptions);

	static FName NormalizeSlotBaseName(FName SlotBaseName);
	static FName BuildRevisionSlotName(FName SlotBaseName, int32 SlotNumber);
	static bool TrySplitRevisionSlotName(const FString& InSlotName, FString& OutBaseSlotName, int32& OutSlotNumber);

	bool LoadOrCreateIndexForSlot(UARSaveIndexGame*& OutIndex, FARSaveResult& OutResult, const TCHAR* IndexSlotName) const;
	bool SaveIndexForSlot(UARSaveIndexGame* IndexObj, FARSaveResult& OutResult, const TCHAR* IndexSlotName) const;
	bool LoadOrCreateIndex(UARSaveIndexGame*& OutIndex, FARSaveResult& OutResult) const;
	bool SaveIndex(UARSaveIndexGame* IndexObj, FARSaveResult& OutResult) const;
	bool SaveSaveObject(UARSaveGame* SaveObject, FName SlotBaseName, int32 SlotNumber, FARSaveResult& OutResult) const;
	UARSaveGame* LoadSaveObjectWithRollback(FName SlotBaseName, int32 RevisionOrLatest, int32& OutResolvedSlotNumber, FARSaveResult& OutResult) const;
	void PruneOldRevisions(FName SlotBaseName, int32 LatestRevision) const;
	void GatherRuntimeData(UARSaveGame* SaveObject);
	void BroadcastSaveFailure(const FARSaveResult& Result);
	void BroadcastLoadFailure(const FARSaveResult& Result);
	void ApplyLoadedSave(UARSaveGame* LoadedSave, const FARSaveResult& LoadResult);
	void QueuePendingCanonicalSyncRequest(AARPlayerController* TargetPlayerController);
	void FlushPendingCanonicalSyncRequests();

	int32 UpsertIndexEntry(UARSaveIndexGame* IndexObj, const FARSaveSlotDescriptor& Descriptor) const;
	bool RemoveIndexEntry(UARSaveIndexGame* IndexObj, FName SlotBaseName) const;

	UPROPERTY(Transient)
	TObjectPtr<UARSaveGame> CurrentSaveGame;

	UPROPERTY(Transient)
	FName CurrentSlotBaseName = NAME_None;

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AARPlayerController>> PendingCanonicalSyncRequests;

	UPROPERTY(Transient)
	FInstancedStruct PendingTravelGameStateData;

	UPROPERTY(Transient)
	bool bSaveInProgress = false;

	UPROPERTY(Transient)
	bool bSaveDirty = false;

	UPROPERTY(Transient)
	FDateTime LastSaveTimestampUtc;

	// Minimum interval between saves (seconds). Clamp to >0 to enable throttling; 0 disables throttle.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Save", meta = (AllowPrivateAccess = "true"))
	float MinSaveIntervalSeconds = 1.0f;

	// When true, successful saves emit an Info log with slot/revision/time (useful for audit during debug).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Save", meta = (AllowPrivateAccess = "true"))
	bool bLogSaveSuccess = false;
};
