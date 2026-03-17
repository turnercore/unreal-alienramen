#include "ARCharacterAssignmentWidgetBase.h"

#include "ARGameStateBase.h"
#include "ARPlayerStateBase.h"
#include "ARPlayerTypes.h"
#include "GameFramework/PlayerController.h"

namespace
{
	static bool AreAssignmentsEquivalent(const TArray<FARControllerCharacterAssignment>& Left, const TArray<FARControllerCharacterAssignment>& Right)
	{
		if (Left.Num() != Right.Num())
		{
			return false;
		}

		for (int32 Index = 0; Index < Left.Num(); ++Index)
		{
			const FARControllerCharacterAssignment& A = Left[Index];
			const FARControllerCharacterAssignment& B = Right[Index];
			if (A.ControllerId != B.ControllerId
				|| !ARPlayer::NormalizeCharacterTag(A.CharacterTag).MatchesTagExact(ARPlayer::NormalizeCharacterTag(B.CharacterTag))
				|| !ARPlayer::NormalizeCharacterTag(A.CommittedCharacterTag).MatchesTagExact(ARPlayer::NormalizeCharacterTag(B.CommittedCharacterTag))
				|| !ARPlayer::NormalizeCharacterTag(A.PendingCharacterTag).MatchesTagExact(ARPlayer::NormalizeCharacterTag(B.PendingCharacterTag))
				|| A.bHasPendingCharacterSelection != B.bHasPendingCharacterSelection
				|| A.DisplayName != B.DisplayName
				|| A.bIsReady != B.bIsReady
				|| A.bIsOwningLocalController != B.bIsOwningLocalController
				|| A.PlayerState != B.PlayerState)
			{
				return false;
			}
		}

		return true;
	}
}

void UARCharacterAssignmentWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();

	if (bAutoBindOwningPlayerOnConstruct && !BoundOwningController)
	{
		TryBindOwningPlayerContext();
	}
}

void UARCharacterAssignmentWidgetBase::NativeDestruct()
{
	DeinitializeCharacterAssignmentWidget();
	Super::NativeDestruct();
}

void UARCharacterAssignmentWidgetBase::InitializeCharacterAssignmentWidget(APlayerController* InOwningController)
{
	if (BoundOwningController == InOwningController && BoundGameState.IsValid())
	{
		RebindTrackedPlayerStateDelegates();
		RebuildAssignmentsCache(true);
		return;
	}

	DeinitializeCharacterAssignmentWidget();
	BoundOwningController = InOwningController;
	if (!BoundOwningController)
	{
		return;
	}

	BoundGameState = BoundOwningController->GetWorld() ? BoundOwningController->GetWorld()->GetGameState<AARGameStateBase>() : nullptr;
	BindGameStateDelegates();
	RebindTrackedPlayerStateDelegates();
	RebuildAssignmentsCache(true);
	BP_OnCharacterAssignmentWidgetInitialized(BoundOwningController, BoundGameState.Get());
}

void UARCharacterAssignmentWidgetBase::DeinitializeCharacterAssignmentWidget()
{
	const bool bHadState = BoundOwningController != nullptr
		|| BoundGameState.IsValid()
		|| CachedAssignments.Num() > 0
		|| PendingCharacterSelectionsByControllerId.Num() > 0
		|| TrackedPlayerStates.Num() > 0;

	UnbindTrackedPlayerStateDelegates();
	UnbindGameStateDelegates();
	BoundOwningController = nullptr;
	BoundGameState.Reset();
	CachedAssignments.Reset();
	PendingCharacterSelectionsByControllerId.Reset();
	TrackedPlayerStates.Reset();
	bCachedAllTrackedControllersReady = false;

	if (bHadState)
	{
		BP_OnCharacterAssignmentWidgetDeinitialized();
	}
}

bool UARCharacterAssignmentWidgetBase::TryBindOwningPlayerContext()
{
	APlayerController* OwningController = GetOwningPlayer();
	if (!OwningController)
	{
		return false;
	}

	InitializeCharacterAssignmentWidget(OwningController);
	return BoundOwningController == OwningController;
}

void UARCharacterAssignmentWidgetBase::RefreshCharacterAssignmentSnapshot()
{
	RebuildAssignmentsCache(true);
}

bool UARCharacterAssignmentWidgetBase::GetAssignmentByControllerId(const int32 ControllerId, FARControllerCharacterAssignment& OutAssignment) const
{
	for (const FARControllerCharacterAssignment& Assignment : CachedAssignments)
	{
		if (Assignment.ControllerId == ControllerId)
		{
			OutAssignment = Assignment;
			return true;
		}
	}

	OutAssignment = FARControllerCharacterAssignment();
	return false;
}

bool UARCharacterAssignmentWidgetBase::GetAssignmentByCharacterTag(const FGameplayTag CharacterTag, FARControllerCharacterAssignment& OutAssignment) const
{
	const FGameplayTag CanonicalTag = ARPlayer::NormalizeCharacterTag(CharacterTag);
	if (!CanonicalTag.IsValid())
	{
		OutAssignment = FARControllerCharacterAssignment();
		return false;
	}

	for (const FARControllerCharacterAssignment& Assignment : CachedAssignments)
	{
		if (ARPlayer::NormalizeCharacterTag(Assignment.CharacterTag).MatchesTagExact(CanonicalTag))
		{
			OutAssignment = Assignment;
			return true;
		}
	}

	OutAssignment = FARControllerCharacterAssignment();
	return false;
}

bool UARCharacterAssignmentWidgetBase::IsCharacterControlled(const FGameplayTag CharacterTag) const
{
	FARControllerCharacterAssignment Assignment;
	return GetAssignmentByCharacterTag(CharacterTag, Assignment);
}

int32 UARCharacterAssignmentWidgetBase::GetBoundOwningControllerId() const
{
	const AARPlayerStateBase* OwningPlayerState = GetBoundOwningPlayerState();
	return OwningPlayerState ? OwningPlayerState->GetPlayerSlotId() : 0;
}

FGameplayTag UARCharacterAssignmentWidgetBase::GetBoundOwningCharacterTag() const
{
	const AARPlayerStateBase* OwningPlayerState = GetBoundOwningPlayerState();
	return OwningPlayerState ? ARPlayer::NormalizeCharacterTag(OwningPlayerState->GetCurrentCharacterTag()) : FGameplayTag();
}

bool UARCharacterAssignmentWidgetBase::RequestAssignControllerToCharacter(const int32 ControllerId, const FGameplayTag CharacterTag)
{
	const FGameplayTag CanonicalCharacterTag = ARPlayer::NormalizeCharacterTag(CharacterTag);
	if (!CanonicalCharacterTag.IsValid() || !CanMutateControllerId(ControllerId))
	{
		return false;
	}

	if (bLockSelectionWhileReady && IsControllerReady(ControllerId))
	{
		return false;
	}

	PendingCharacterSelectionsByControllerId.FindOrAdd(ControllerId) = CanonicalCharacterTag;

	if (!bDeferCharacterAssignmentUntilConfirm)
	{
		if (!ApplyCharacterAssignmentToController(ControllerId, CanonicalCharacterTag))
		{
			return false;
		}

		PendingCharacterSelectionsByControllerId.Remove(ControllerId);
	}

	RebuildAssignmentsCache(true);
	return true;
}

bool UARCharacterAssignmentWidgetBase::RequestAssignOwningControllerToCharacter(const FGameplayTag CharacterTag)
{
	const int32 OwningControllerId = GetBoundOwningControllerId();
	return OwningControllerId > 0 && RequestAssignControllerToCharacter(OwningControllerId, CharacterTag);
}

bool UARCharacterAssignmentWidgetBase::ConfirmControllerSelection(const int32 ControllerId, const bool bSetReady)
{
	if (!CanMutateControllerId(ControllerId))
	{
		return false;
	}

	FGameplayTag PendingCharacterTag;
	if (GetPendingSelectionByControllerId(ControllerId, PendingCharacterTag))
	{
		if (!ApplyCharacterAssignmentToController(ControllerId, PendingCharacterTag))
		{
			return false;
		}
	}

	PendingCharacterSelectionsByControllerId.Remove(ControllerId);
	if (bSetReady && !SetControllerReadyState(ControllerId, true))
	{
		return false;
	}

	RebuildAssignmentsCache(true);
	return true;
}

bool UARCharacterAssignmentWidgetBase::ConfirmOwningControllerSelection(const bool bSetReady)
{
	const int32 OwningControllerId = GetBoundOwningControllerId();
	return OwningControllerId > 0 && ConfirmControllerSelection(OwningControllerId, bSetReady);
}

bool UARCharacterAssignmentWidgetBase::CancelControllerConfirmation(const int32 ControllerId, const bool bSetNotReady)
{
	if (!CanMutateControllerId(ControllerId))
	{
		return false;
	}

	PendingCharacterSelectionsByControllerId.Remove(ControllerId);
	if (bSetNotReady && !SetControllerReadyState(ControllerId, false))
	{
		return false;
	}

	RebuildAssignmentsCache(true);
	return true;
}

bool UARCharacterAssignmentWidgetBase::CancelOwningControllerConfirmation(const bool bSetNotReady)
{
	const int32 OwningControllerId = GetBoundOwningControllerId();
	return OwningControllerId > 0 && CancelControllerConfirmation(OwningControllerId, bSetNotReady);
}

bool UARCharacterAssignmentWidgetBase::SetControllerReadyState(const int32 ControllerId, const bool bReady)
{
	if (!CanMutateControllerId(ControllerId))
	{
		return false;
	}

	AARPlayerStateBase* TargetPlayerState = FindTrackedPlayerStateByControllerId(ControllerId);
	if (!TargetPlayerState)
	{
		return false;
	}

	TargetPlayerState->SetReadyForRun(bReady);
	return true;
}

bool UARCharacterAssignmentWidgetBase::SetOwningControllerReadyState(const bool bReady)
{
	const int32 OwningControllerId = GetBoundOwningControllerId();
	return OwningControllerId > 0 && SetControllerReadyState(OwningControllerId, bReady);
}

bool UARCharacterAssignmentWidgetBase::GetPendingSelectionByControllerId(const int32 ControllerId, FGameplayTag& OutPendingCharacterTag) const
{
	const FGameplayTag* PendingTag = PendingCharacterSelectionsByControllerId.Find(ControllerId);
	if (!PendingTag)
	{
		OutPendingCharacterTag = FGameplayTag();
		return false;
	}

	OutPendingCharacterTag = ARPlayer::NormalizeCharacterTag(*PendingTag);
	return OutPendingCharacterTag.IsValid();
}

bool UARCharacterAssignmentWidgetBase::IsControllerReady(const int32 ControllerId) const
{
	for (const FARControllerCharacterAssignment& Assignment : CachedAssignments)
	{
		if (Assignment.ControllerId == ControllerId)
		{
			return Assignment.bIsReady;
		}
	}

	return false;
}

bool UARCharacterAssignmentWidgetBase::CanControllerAssignToCharacter(const int32 ControllerId, const FGameplayTag CharacterTag) const
{
	if (!CanMutateControllerId(ControllerId) || !ARPlayer::NormalizeCharacterTag(CharacterTag).IsValid())
	{
		return false;
	}

	if (bLockSelectionWhileReady && IsControllerReady(ControllerId))
	{
		return false;
	}

	FARControllerCharacterAssignment Assignment;
	return GetAssignmentByControllerId(ControllerId, Assignment);
}

bool UARCharacterAssignmentWidgetBase::CanMutateControllerId(const int32 ControllerId) const
{
	if (ControllerId <= 0)
	{
		return false;
	}

	APlayerController* OwningController = BoundOwningController;
	if (!OwningController)
	{
		return false;
	}

	if (OwningController->HasAuthority())
	{
		return true;
	}

	const AARPlayerStateBase* OwningPlayerState = GetBoundOwningPlayerState();
	return OwningPlayerState && OwningPlayerState->GetPlayerSlotId() == ControllerId;
}

AARPlayerStateBase* UARCharacterAssignmentWidgetBase::FindTrackedPlayerStateByControllerId(const int32 ControllerId) const
{
	if (ControllerId <= 0)
	{
		return nullptr;
	}

	const AARGameStateBase* GameState = BoundGameState.Get();
	if (!GameState)
	{
		return nullptr;
	}

	const TArray<AARPlayerStateBase*> PlayerStates = GameState->GetPlayerStates();
	for (AARPlayerStateBase* PlayerState : PlayerStates)
	{
		if (PlayerState && PlayerState->GetPlayerSlotId() == ControllerId)
		{
			return PlayerState;
		}
	}

	return nullptr;
}

bool UARCharacterAssignmentWidgetBase::ApplyCharacterAssignmentToController(const int32 ControllerId, const FGameplayTag CharacterTag)
{
	AARPlayerStateBase* TargetPlayerState = FindTrackedPlayerStateByControllerId(ControllerId);
	const FGameplayTag CanonicalCharacterTag = ARPlayer::NormalizeCharacterTag(CharacterTag);
	if (!TargetPlayerState || !CanonicalCharacterTag.IsValid())
	{
		return false;
	}

	TargetPlayerState->SetCurrentCharacterTag(CanonicalCharacterTag);
	return true;
}

bool UARCharacterAssignmentWidgetBase::ComputeAllTrackedControllersReady() const
{
	bool bFoundAnyController = false;
	for (const FARControllerCharacterAssignment& Assignment : CachedAssignments)
	{
		if (Assignment.ControllerId <= 0)
		{
			continue;
		}

		bFoundAnyController = true;
		if (!Assignment.bIsReady)
		{
			return false;
		}
	}

	return bFoundAnyController;
}

void UARCharacterAssignmentWidgetBase::BindGameStateDelegates()
{
	AARGameStateBase* GameState = BoundGameState.Get();
	if (!GameState)
	{
		return;
	}

	GameState->OnTrackedPlayersChanged.AddUniqueDynamic(this, &UARCharacterAssignmentWidgetBase::HandleTrackedPlayersChanged);
}

void UARCharacterAssignmentWidgetBase::UnbindGameStateDelegates()
{
	AARGameStateBase* GameState = BoundGameState.Get();
	if (!GameState)
	{
		return;
	}

	GameState->OnTrackedPlayersChanged.RemoveDynamic(this, &UARCharacterAssignmentWidgetBase::HandleTrackedPlayersChanged);
}

void UARCharacterAssignmentWidgetBase::RebindTrackedPlayerStateDelegates()
{
	UnbindTrackedPlayerStateDelegates();

	AARGameStateBase* GameState = BoundGameState.Get();
	if (!GameState)
	{
		return;
	}

	const TArray<AARPlayerStateBase*> PlayerStates = GameState->GetPlayerStates();
	for (AARPlayerStateBase* PlayerState : PlayerStates)
	{
		if (!PlayerState)
		{
			continue;
		}

		TrackedPlayerStates.Add(PlayerState);
		PlayerState->OnCurrentCharacterTagChanged.AddUniqueDynamic(this, &UARCharacterAssignmentWidgetBase::HandleTrackedPlayerCurrentCharacterTagChanged);
		PlayerState->OnPlayerSlotIdChanged.AddUniqueDynamic(this, &UARCharacterAssignmentWidgetBase::HandleTrackedPlayerSlotIdChanged);
		PlayerState->OnReadyStatusChanged.AddUniqueDynamic(this, &UARCharacterAssignmentWidgetBase::HandleTrackedPlayerReadyStatusChanged);
		PlayerState->OnDisplayNameChanged.AddUniqueDynamic(this, &UARCharacterAssignmentWidgetBase::HandleTrackedPlayerDisplayNameChanged);
	}
}

void UARCharacterAssignmentWidgetBase::UnbindTrackedPlayerStateDelegates()
{
	for (const TWeakObjectPtr<AARPlayerStateBase>& TrackedState : TrackedPlayerStates)
	{
		AARPlayerStateBase* PlayerState = TrackedState.Get();
		if (!PlayerState)
		{
			continue;
		}

		PlayerState->OnCurrentCharacterTagChanged.RemoveDynamic(this, &UARCharacterAssignmentWidgetBase::HandleTrackedPlayerCurrentCharacterTagChanged);
		PlayerState->OnPlayerSlotIdChanged.RemoveDynamic(this, &UARCharacterAssignmentWidgetBase::HandleTrackedPlayerSlotIdChanged);
		PlayerState->OnReadyStatusChanged.RemoveDynamic(this, &UARCharacterAssignmentWidgetBase::HandleTrackedPlayerReadyStatusChanged);
		PlayerState->OnDisplayNameChanged.RemoveDynamic(this, &UARCharacterAssignmentWidgetBase::HandleTrackedPlayerDisplayNameChanged);
	}

	TrackedPlayerStates.Reset();
}

void UARCharacterAssignmentWidgetBase::RebuildAssignmentsCache(const bool bForceBroadcast)
{
	TArray<FARControllerCharacterAssignment> NextAssignments;
	BuildAssignmentsSnapshot(NextAssignments);

	if (!bForceBroadcast && AreAssignmentsEquivalent(CachedAssignments, NextAssignments))
	{
		return;
	}

	CachedAssignments = MoveTemp(NextAssignments);
	OnControllerCharacterAssignmentsChanged.Broadcast(CachedAssignments);
	BP_OnControllerCharacterAssignmentsChanged(CachedAssignments);

	const bool bOldAllReady = bCachedAllTrackedControllersReady;
	const bool bNewAllReady = ComputeAllTrackedControllersReady();
	bCachedAllTrackedControllersReady = bNewAllReady;
	if (bOldAllReady != bNewAllReady || bForceBroadcast)
	{
		OnAllTrackedControllersReadyChanged.Broadcast(bNewAllReady);
		BP_OnAllTrackedControllersReadyChanged(bNewAllReady);
	}
}

void UARCharacterAssignmentWidgetBase::BuildAssignmentsSnapshot(TArray<FARControllerCharacterAssignment>& OutAssignments) const
{
	OutAssignments.Reset();

	const AARGameStateBase* GameState = BoundGameState.Get();
	if (!GameState)
	{
		return;
	}

	const int32 OwningControllerId = GetBoundOwningControllerId();
	TSet<int32> SeenControllerIds;
	const TArray<AARPlayerStateBase*> PlayerStates = GameState->GetPlayerStates();
	for (AARPlayerStateBase* PlayerState : PlayerStates)
	{
		if (!PlayerState)
		{
			continue;
		}

		const int32 ControllerId = PlayerState->GetPlayerSlotId();
		if (ControllerId <= 0 || SeenControllerIds.Contains(ControllerId))
		{
			continue;
		}

		SeenControllerIds.Add(ControllerId);

		const FGameplayTag CommittedCharacterTag = ARPlayer::NormalizeCharacterTag(PlayerState->GetCurrentCharacterTag());
		const FGameplayTag* PendingCharacterTagPtr = PendingCharacterSelectionsByControllerId.Find(ControllerId);
		const FGameplayTag PendingCharacterTag = PendingCharacterTagPtr ? ARPlayer::NormalizeCharacterTag(*PendingCharacterTagPtr) : FGameplayTag();
		const bool bHasPendingCharacterSelection = PendingCharacterTag.IsValid();

		FARControllerCharacterAssignment Assignment;
		Assignment.ControllerId = ControllerId;
		Assignment.CommittedCharacterTag = CommittedCharacterTag;
		Assignment.PendingCharacterTag = PendingCharacterTag;
		Assignment.bHasPendingCharacterSelection = bHasPendingCharacterSelection;
		Assignment.CharacterTag = bHasPendingCharacterSelection ? PendingCharacterTag : CommittedCharacterTag;
		Assignment.DisplayName = PlayerState->GetDisplayNameValue();
		Assignment.bIsReady = PlayerState->IsReadyForRun();
		Assignment.bIsOwningLocalController = OwningControllerId > 0 && ControllerId == OwningControllerId;
		Assignment.PlayerState = PlayerState;
		OutAssignments.Add(Assignment);
	}

	OutAssignments.Sort([](const FARControllerCharacterAssignment& A, const FARControllerCharacterAssignment& B)
	{
		return A.ControllerId < B.ControllerId;
	});
}

AARPlayerStateBase* UARCharacterAssignmentWidgetBase::GetBoundOwningPlayerState() const
{
	return BoundOwningController ? BoundOwningController->GetPlayerState<AARPlayerStateBase>() : nullptr;
}

void UARCharacterAssignmentWidgetBase::HandleTrackedPlayersChanged()
{
	RebindTrackedPlayerStateDelegates();
	RebuildAssignmentsCache(true);
}

void UARCharacterAssignmentWidgetBase::HandleTrackedPlayerCurrentCharacterTagChanged(FGameplayTag NewCharacterTag, FGameplayTag OldCharacterTag)
{
	(void)NewCharacterTag;
	(void)OldCharacterTag;
	RebuildAssignmentsCache(false);
}

void UARCharacterAssignmentWidgetBase::HandleTrackedPlayerSlotIdChanged(AARPlayerStateBase* SourcePlayerState, int32 NewPlayerSlotId, int32 OldPlayerSlotId)
{
	(void)SourcePlayerState;
	(void)NewPlayerSlotId;
	(void)OldPlayerSlotId;
	RebuildAssignmentsCache(true);
}

void UARCharacterAssignmentWidgetBase::HandleTrackedPlayerReadyStatusChanged(AARPlayerStateBase* SourcePlayerState, FGameplayTag SourceCharacterTag, bool bNewReady, bool bOldReady)
{
	(void)SourcePlayerState;
	(void)SourceCharacterTag;
	(void)bNewReady;
	(void)bOldReady;
	RebuildAssignmentsCache(false);
}

void UARCharacterAssignmentWidgetBase::HandleTrackedPlayerDisplayNameChanged(AARPlayerStateBase* SourcePlayerState, FGameplayTag SourceCharacterTag, const FString& NewDisplayName, const FString& OldDisplayName)
{
	(void)SourcePlayerState;
	(void)SourceCharacterTag;
	(void)NewDisplayName;
	(void)OldDisplayName;
	RebuildAssignmentsCache(false);
}
