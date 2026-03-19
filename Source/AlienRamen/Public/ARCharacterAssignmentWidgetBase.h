/**
 * @file ARCharacterAssignmentWidgetBase.h
 * @brief Reusable character-assignment UI bridge keyed by controller identity + canonical character tags.
 */
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Blueprint/UserWidget.h"
#include "ARCharacterAssignmentWidgetBase.generated.h"

class AARGameStateBase;
class AARPlayerStateBase;
class APlayerController;

/**
 * Controller-owned view row used by character-select and pause-menu reassignment widgets.
 * ControllerId is the runtime controller/profile id (AARPlayerStateBase::PlayerSlotId),
 * while CharacterTag is the canonical character identity currently owned by that controller.
 */
USTRUCT(BlueprintType)
struct FARControllerCharacterAssignment
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|UI|Character Assignment")
	int32 ControllerId = 0;

	/** Effective character shown in UI (pending selection when present, otherwise committed runtime tag). */
	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|UI|Character Assignment")
	FGameplayTag CharacterTag;

	/** Last committed replicated character tag from PlayerState. */
	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|UI|Character Assignment")
	FGameplayTag CommittedCharacterTag;

	/** Pending local selection awaiting confirmation. */
	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|UI|Character Assignment")
	FGameplayTag PendingCharacterTag;

	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|UI|Character Assignment")
	bool bHasPendingCharacterSelection = false;

	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|UI|Character Assignment")
	FString DisplayName;

	/** Player confirmation/ready state used for lock-in flows. */
	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|UI|Character Assignment")
	bool bIsReady = false;

	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|UI|Character Assignment")
	bool bIsOwningLocalController = false;

	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|UI|Character Assignment")
	TObjectPtr<AARPlayerStateBase> PlayerState = nullptr;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAROnControllerCharacterAssignmentsChangedSignature, const TArray<FARControllerCharacterAssignment>&, Assignments);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAROnAllTrackedControllersReadyChangedSignature, bool, bAllReady);

UCLASS(Abstract, Blueprintable)
class ALIENRAMEN_API UARCharacterAssignmentWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Binds the widget to a controller context and starts tracking replicated controller<->character ownership. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|UI|Character Assignment")
	void InitializeCharacterAssignmentWidget(APlayerController* InOwningController);

	/** Unbinds all runtime delegates and clears cached assignment rows. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|UI|Character Assignment")
	void DeinitializeCharacterAssignmentWidget();

	/** Convenience auto-bind helper for widgets created with a valid owning player. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|UI|Character Assignment")
	bool TryBindOwningPlayerContext();

	/** Forces a full snapshot rebuild from current replicated state. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|UI|Character Assignment")
	void RefreshCharacterAssignmentSnapshot();

	/** Cached assignment rows sorted by ControllerId. */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|UI|Character Assignment")
	TArray<FARControllerCharacterAssignment> GetCurrentAssignments() const { return CachedAssignments; }

	/** Returns cached assignment for a specific runtime controller id. */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|UI|Character Assignment")
	bool GetAssignmentByControllerId(int32 ControllerId, FARControllerCharacterAssignment& OutAssignment) const;

	/** Returns cached assignment for a specific canonical character tag. */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|UI|Character Assignment")
	bool GetAssignmentByCharacterTag(FGameplayTag CharacterTag, FARControllerCharacterAssignment& OutAssignment) const;

	/** True when any tracked controller currently owns the provided canonical character tag. */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|UI|Character Assignment")
	bool IsCharacterControlled(FGameplayTag CharacterTag) const;

	/** Runtime controller/profile id for this widget's owning player context. */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|UI|Character Assignment")
	int32 GetBoundOwningControllerId() const;

	/** Canonical character tag currently owned by this widget's owning player context. */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|UI|Character Assignment")
	FGameplayTag GetBoundOwningCharacterTag() const;

	/** Queues a pending character selection for this controller id (does not commit when deferred-confirm is enabled). */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|UI|Character Assignment")
	bool RequestAssignControllerToCharacter(int32 ControllerId, FGameplayTag CharacterTag);

	/** Convenience wrapper for RequestAssignControllerToCharacter using this widget's owning controller id. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|UI|Character Assignment")
	bool RequestAssignOwningControllerToCharacter(FGameplayTag CharacterTag);

	/** Commits this controller's pending selection (if present) and optionally marks ready. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|UI|Character Assignment")
	bool ConfirmControllerSelection(int32 ControllerId, bool bSetReady = true);

	/** Commits owning controller pending selection and optionally marks ready. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|UI|Character Assignment")
	bool ConfirmOwningControllerSelection(bool bSetReady = true);

	/** Clears pending selection and optionally marks not-ready so selection can change again. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|UI|Character Assignment")
	bool CancelControllerConfirmation(int32 ControllerId, bool bSetNotReady = true);

	/** Clears owning pending selection and optionally marks not-ready. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|UI|Character Assignment")
	bool CancelOwningControllerConfirmation(bool bSetNotReady = true);

	/** Explicit ready toggle by runtime controller id. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|UI|Character Assignment")
	bool SetControllerReadyState(int32 ControllerId, bool bReady);

	/** Explicit ready toggle for owning controller. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|UI|Character Assignment")
	bool SetOwningControllerReadyState(bool bReady);

	/** Returns pending tag for controller when one exists. */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|UI|Character Assignment")
	bool GetPendingSelectionByControllerId(int32 ControllerId, FGameplayTag& OutPendingCharacterTag) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|UI|Character Assignment")
	bool IsControllerReady(int32 ControllerId) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|UI|Character Assignment")
	bool AreAllTrackedControllersReady() const { return bCachedAllTrackedControllersReady; }

	/** Lightweight validity guard for assignment attempts (controller exists + tag resolves canonical). */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|UI|Character Assignment")
	bool CanControllerAssignToCharacter(int32 ControllerId, FGameplayTag CharacterTag) const;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|UI|Character Assignment")
	FAROnControllerCharacterAssignmentsChangedSignature OnControllerCharacterAssignmentsChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|UI|Character Assignment")
	FAROnAllTrackedControllersReadyChangedSignature OnAllTrackedControllersReadyChanged;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|UI|Character Assignment")
	void BP_OnCharacterAssignmentWidgetInitialized(APlayerController* InOwningController, AARGameStateBase* InGameState);

	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|UI|Character Assignment")
	void BP_OnCharacterAssignmentWidgetDeinitialized();

	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|UI|Character Assignment")
	void BP_OnControllerCharacterAssignmentsChanged(const TArray<FARControllerCharacterAssignment>& Assignments);

	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|UI|Character Assignment")
	void BP_OnAllTrackedControllersReadyChanged(bool bAllReady);

	/** Auto-bind this widget to GetOwningPlayer() on construct. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|UI|Character Assignment")
	bool bAutoBindOwningPlayerOnConstruct = true;

	/** When true, character requests are staged locally and only sent on Confirm* calls. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|UI|Character Assignment")
	bool bDeferCharacterAssignmentUntilConfirm = true;

	/** When true, ready controllers cannot change pending selection until unready/cancel. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|UI|Character Assignment")
	bool bLockSelectionWhileReady = true;

private:
	bool CanMutateControllerId(int32 ControllerId) const;
	AARPlayerStateBase* FindTrackedPlayerStateByControllerId(int32 ControllerId) const;
	bool ApplyCharacterAssignmentToController(int32 ControllerId, FGameplayTag CharacterTag);
	bool ComputeAllTrackedControllersReady() const;
	void BindGameStateDelegates();
	void UnbindGameStateDelegates();
	void RebindTrackedPlayerStateDelegates();
	void UnbindTrackedPlayerStateDelegates();
	void RebuildAssignmentsCache(bool bForceBroadcast);
	void BuildAssignmentsSnapshot(TArray<FARControllerCharacterAssignment>& OutAssignments) const;
	AARPlayerStateBase* GetBoundOwningPlayerState() const;

	UFUNCTION()
	void HandleTrackedPlayersChanged();

	UFUNCTION()
	void HandleTrackedPlayerCurrentCharacterTagChanged(FGameplayTag NewCharacterTag, FGameplayTag OldCharacterTag);

	UFUNCTION()
	void HandleTrackedPlayerSlotIdChanged(AARPlayerStateBase* SourcePlayerState, int32 NewPlayerSlotId, int32 OldPlayerSlotId);

	UFUNCTION()
	void HandleTrackedPlayerReadyStatusChanged(AARPlayerStateBase* SourcePlayerState, FGameplayTag SourceCharacterTag, bool bNewReady, bool bOldReady);

	UFUNCTION()
	void HandleTrackedPlayerDisplayNameChanged(AARPlayerStateBase* SourcePlayerState, FGameplayTag SourceCharacterTag, const FString& NewDisplayName, const FString& OldDisplayName);

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Alien Ramen|UI|Character Assignment", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<APlayerController> BoundOwningController = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Alien Ramen|UI|Character Assignment", meta = (AllowPrivateAccess = "true"))
	TWeakObjectPtr<AARGameStateBase> BoundGameState;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Alien Ramen|UI|Character Assignment", meta = (AllowPrivateAccess = "true"))
	TArray<FARControllerCharacterAssignment> CachedAssignments;

	UPROPERTY(Transient)
	TMap<int32, FGameplayTag> PendingCharacterSelectionsByControllerId;

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AARPlayerStateBase>> TrackedPlayerStates;

	UPROPERTY(Transient)
	bool bCachedAllTrackedControllersReady = false;
};
