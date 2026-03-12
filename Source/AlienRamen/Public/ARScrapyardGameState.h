/**
 * @file ARScrapyardGameState.h
 * @brief ARScrapyardGameState header for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARGameStateBase.h"
#include "ARRunBuffTypes.h"
#include "ARScrapyardTypes.h"
#include "ARScrapyardGameState.generated.h"

class AARScrapyardCarryItemBase;
class AARScrapyardExitZoneActor;
class UARRunBuffSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAROnScrapyardExtractionSummaryChangedSignature, const FARScrapyardExtractionSummary&, Summary);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAROnScrapyardRunTimerChangedSignature, float, RemainingSeconds);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAROnScrapyardRunActiveChangedSignature, bool, bIsRunActive);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAROnScrapyardRunTimerPausedChangedSignature, bool, bIsPaused);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAROnScrapyardRunBuffSnapshotChangedSignature, const FARRunBuffStateSnapshot&, Snapshot);

UCLASS()
class ALIENRAMEN_API AARScrapyardGameState : public AARGameStateBase
{
	GENERATED_BODY()

public:
	AARScrapyardGameState();

	virtual UScriptStruct* GetStateStruct_Implementation() const override;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Scrapyard")
	const FARScrapyardExtractionSummary& GetExtractionSummary() const { return ExtractionSummary; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Scrapyard")
	bool IsScrapyardRunActive() const { return bScrapyardRunActive; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Scrapyard")
	int32 GetScrapyardRunSeed() const { return ScrapyardRunSeed; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Scrapyard")
	float GetScrapyardRunDurationSeconds() const { return ScrapyardRunDurationSeconds; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Scrapyard")
	float GetScrapyardRunRemainingSeconds() const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Scrapyard")
	bool IsScrapyardRunTimerPaused() const { return bScrapyardRunTimerPaused; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Scrapyard")
	const FARRunBuffStateSnapshot& GetRunBuffStateSnapshot() const { return RunBuffStateSnapshot; }

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Scrapyard", meta = (BlueprintAuthorityOnly))
	void StartScrapyardRun(float RunDurationSeconds, int32 InRunSeed = 0);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Scrapyard", meta = (BlueprintAuthorityOnly))
	void AddScrapyardTime(float AddedSeconds);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Scrapyard", meta = (BlueprintAuthorityOnly))
	void SetScrapyardRunTimerPaused(bool bPaused);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Scrapyard", meta = (BlueprintAuthorityOnly))
	bool FinalizeScrapyardRun();

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Scrapyard", meta = (BlueprintAuthorityOnly))
	bool FinalizeScrapyardRunAndTravelToShop(const FString& InShopTravelURL);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Scrapyard", meta = (BlueprintAuthorityOnly))
	bool ReserveScrapForItem(AARScrapyardCarryItemBase* ItemActor, int32& OutReservedCost);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Scrapyard", meta = (BlueprintAuthorityOnly))
	bool RefundScrapForItem(AARScrapyardCarryItemBase* ItemActor, int32& OutRefundCost);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Scrapyard")
	bool ResolveItemDefinitionForActor(AARScrapyardCarryItemBase* ItemActor, FARScrapyardItemDefRow& OutDef) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Scrapyard")
	int32 ResolveItemCostForActor(AARScrapyardCarryItemBase* ItemActor) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Scrapyard")
	bool IsItemReservedForExtraction(AARScrapyardCarryItemBase* ItemActor) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Scrapyard")
	bool TryGetReservedScrapForItem(AARScrapyardCarryItemBase* ItemActor, int32& OutReservedCost) const;

	void RegisterExitZone(AARScrapyardExitZoneActor* ExitZone);
	void UnregisterExitZone(AARScrapyardExitZoneActor* ExitZone);

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Scrapyard")
	FAROnScrapyardExtractionSummaryChangedSignature OnScrapyardExtractionSummaryChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Scrapyard")
	FAROnScrapyardRunTimerChangedSignature OnScrapyardRunTimerChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Scrapyard")
	FAROnScrapyardRunActiveChangedSignature OnScrapyardRunActiveChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Scrapyard")
	FAROnScrapyardRunTimerPausedChangedSignature OnScrapyardRunTimerPausedChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Scrapyard")
	FAROnScrapyardRunBuffSnapshotChangedSignature OnScrapyardRunBuffSnapshotChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_ScrapyardRunActive(bool bOldRunActive);

	UFUNCTION()
	void OnRep_ExtractionSummary(const FARScrapyardExtractionSummary& OldSummary);

	UFUNCTION()
	void OnRep_ScrapyardRunTimerPaused(bool bOldPaused);

	UFUNCTION()
	void OnRep_RunBuffStateSnapshot(const FARRunBuffStateSnapshot& OldSnapshot);

private:
	struct FScrapyardExtractionCandidate
	{
		TWeakObjectPtr<AARScrapyardCarryItemBase> ItemActor;
		FGameplayTag ItemTag;
		int32 ScrapCost = 0;
	};

	void SetScrapyardSharedScrap(int32 NewScrapValue);
	void RefreshExtractionSummary(bool bBroadcast);
	void PruneInvalidReservedItems();
	void BroadcastTimerIfNeeded(bool bForceBroadcast = false);
	void BuildExtractionCandidates(TArray<FScrapyardExtractionCandidate>& OutCandidates) const;
	bool ResolveItemDefinitionForTag(FGameplayTag ItemTag, FARScrapyardItemDefRow& OutDef) const;
	bool GrantRewardForCandidate(const FScrapyardExtractionCandidate& Candidate, FARScrapyardRewardGrant& OutGrantedReward);
	void CleanupCandidateActor(const FScrapyardExtractionCandidate& Candidate);
	void BindRunBuffSubsystem();
	void UnbindRunBuffSubsystem();
	void RefreshRunBuffStateSnapshot(bool bBroadcast);
	float ResolveScrapyardRunElapsedSeconds() const;

	UFUNCTION()
	void HandleRunBuffStateChanged(const FARRunBuffStateSnapshot& Snapshot);

	UPROPERTY(ReplicatedUsing = OnRep_ScrapyardRunActive, BlueprintReadOnly, Category = "Alien Ramen|Scrapyard", meta = (AllowPrivateAccess = "true"))
	bool bScrapyardRunActive = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Alien Ramen|Scrapyard", meta = (AllowPrivateAccess = "true"))
	float ScrapyardRunStartServerTime = 0.0f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Alien Ramen|Scrapyard", meta = (AllowPrivateAccess = "true"))
	float ScrapyardRunDurationSeconds = 0.0f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Alien Ramen|Scrapyard", meta = (AllowPrivateAccess = "true"))
	int32 ScrapyardRunSeed = 0;

	UPROPERTY(ReplicatedUsing = OnRep_ScrapyardRunTimerPaused, BlueprintReadOnly, Category = "Alien Ramen|Scrapyard", meta = (AllowPrivateAccess = "true"))
	bool bScrapyardRunTimerPaused = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Alien Ramen|Scrapyard", meta = (AllowPrivateAccess = "true"))
	float ScrapyardRunPauseStartServerTime = 0.0f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Alien Ramen|Scrapyard", meta = (AllowPrivateAccess = "true"))
	float ScrapyardRunAccumulatedPauseSeconds = 0.0f;

	UPROPERTY(ReplicatedUsing = OnRep_ExtractionSummary, BlueprintReadOnly, Category = "Alien Ramen|Scrapyard", meta = (AllowPrivateAccess = "true"))
	FARScrapyardExtractionSummary ExtractionSummary;

	UPROPERTY(ReplicatedUsing = OnRep_RunBuffStateSnapshot, BlueprintReadOnly, Category = "Alien Ramen|Scrapyard", meta = (AllowPrivateAccess = "true"))
	FARRunBuffStateSnapshot RunBuffStateSnapshot;

	// Map URL used when finalizing and returning to shop.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Scrapyard", meta = (AllowPrivateAccess = "true"))
	FString DefaultShopTravelURL = TEXT("/Game/Maps/Lvl_RamenShop");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Scrapyard", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float DefaultRunDurationSeconds = 180.0f;

	UPROPERTY(Transient)
	TMap<TWeakObjectPtr<AARScrapyardCarryItemBase>, int32> ReservedCostByItem;

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AARScrapyardExitZoneActor>> RegisteredExitZones;

	UPROPERTY(Transient)
	TWeakObjectPtr<UARRunBuffSubsystem> BoundRunBuffSubsystem;

	UPROPERTY(Transient)
	int32 LastBroadcastWholeRemainingSeconds = INDEX_NONE;
};
