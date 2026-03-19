#include "ARInvaderSpicyTrackHUDWidgetBase.h"

#include "ARInvaderGameState.h"
#include "ARInvaderHUD.h"
#include "ARPlayerStateBase.h"
#include "ARPlayerTypes.h"
#include "GameFramework/PlayerController.h"

namespace
{
	static FGameplayTag NormalizeCharacterTag(const FGameplayTag CharacterTag)
	{
		return ARPlayer::NormalizeCharacterTag(CharacterTag);
	}

	static bool DoesSnapshotMatchCharacterTag(const FARInvaderSpicyTrackCharacterState& Snapshot, const FGameplayTag CharacterTag)
	{
		const FGameplayTag NormalizedSnapshotTag = NormalizeCharacterTag(Snapshot.SourceCharacterTag);
		const FGameplayTag NormalizedTargetTag = NormalizeCharacterTag(CharacterTag);
		return NormalizedSnapshotTag.IsValid() && NormalizedSnapshotTag.MatchesTagExact(NormalizedTargetTag);
	}
}

void UARInvaderSpicyTrackHUDWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();

	if (bAutoBindOwningInvaderHUDOnConstruct && !BoundInvaderHUD.IsValid())
	{
		TryBindOwningInvaderHUD();
	}
}

void UARInvaderSpicyTrackHUDWidgetBase::NativeDestruct()
{
	DeinitializeInvaderSpicyTrackHUDWidget();
	Super::NativeDestruct();
}

void UARInvaderSpicyTrackHUDWidgetBase::InitializeInvaderSpicyTrackHUDWidget(AARInvaderHUD* InInvaderHUD)
{
	if (!InInvaderHUD)
	{
		DeinitializeInvaderSpicyTrackHUDWidget();
		return;
	}

	if (BoundInvaderHUD.Get() == InInvaderHUD && BoundInvaderGameState.IsValid())
	{
		RefreshInvaderSpicyTrackSnapshot(true);
		return;
	}

	DeinitializeInvaderSpicyTrackHUDWidget();

	BoundInvaderHUD = InInvaderHUD;
	BoundInvaderGameState = InInvaderHUD->GetWorld() ? InInvaderHUD->GetWorld()->GetGameState<AARInvaderGameState>() : nullptr;

	BindInvaderGameStateDelegates();
	RebindTrackedPlayerStateDelegates();
	RefreshInvaderSpicyTrackSnapshot(false);
	bHasInvaderSpicyTrackHUDWidgetInitialized = true;
	BP_OnInvaderSpicyTrackHUDWidgetInitialized(BoundInvaderHUD.Get(), BoundInvaderGameState.Get());
	RefreshInvaderSpicyTrackSnapshot(true);
}

void UARInvaderSpicyTrackHUDWidgetBase::DeinitializeInvaderSpicyTrackHUDWidget()
{
	const bool bHadBindingOrState = BoundInvaderHUD.IsValid()
		|| BoundInvaderGameState.IsValid()
		|| TrackedPlayerStates.Num() > 0
		|| CachedCharacterStates.Num() > 0
		|| bHasSharedTrackSnapshot;

	UnbindTrackedPlayerStateDelegates();
	UnbindInvaderGameStateDelegates();
	BoundInvaderHUD.Reset();
	BoundInvaderGameState.Reset();
	CachedCharacterStates.Reset();
	CachedSharedTrackSlots.Reset();
	bHasSharedTrackSnapshot = false;
	bHasInvaderSpicyTrackHUDWidgetInitialized = false;

	if (bHadBindingOrState)
	{
		BP_OnInvaderSpicyTrackHUDWidgetDeinitialized();
	}
}

bool UARInvaderSpicyTrackHUDWidgetBase::TryBindOwningInvaderHUD()
{
	APlayerController* OwningController = GetOwningPlayer();
	AARInvaderHUD* OwningInvaderHUD = OwningController ? Cast<AARInvaderHUD>(OwningController->GetHUD()) : nullptr;
	if (!OwningInvaderHUD)
	{
		return false;
	}

	InitializeInvaderSpicyTrackHUDWidget(OwningInvaderHUD);
	return BoundInvaderHUD.Get() == OwningInvaderHUD;
}

void UARInvaderSpicyTrackHUDWidgetBase::RefreshInvaderSpicyTrackSnapshot(const bool bBroadcastSnapshotEvents)
{
	AARInvaderHUD* InvaderHUD = BoundInvaderHUD.Get();
	AARInvaderGameState* ResolvedInvaderGameState = InvaderHUD && InvaderHUD->GetWorld()
		? InvaderHUD->GetWorld()->GetGameState<AARInvaderGameState>()
		: nullptr;
	if (ResolvedInvaderGameState != BoundInvaderGameState.Get())
	{
		UnbindTrackedPlayerStateDelegates();
		UnbindInvaderGameStateDelegates();
		BoundInvaderGameState = ResolvedInvaderGameState;
		BindInvaderGameStateDelegates();
		RebindTrackedPlayerStateDelegates();
	}

	RefreshCachedCharacterStates();
	RefreshSharedTrackSnapshot();

	if (!bBroadcastSnapshotEvents || !bHasInvaderSpicyTrackHUDWidgetInitialized)
	{
		return;
	}

	for (const FARInvaderSpicyTrackCharacterState& Snapshot : CachedCharacterStates)
	{
		OnInvaderWidgetCharacterSpiceTrackChanged.Broadcast(
			Snapshot.SourcePlayerState,
			Snapshot.SourceCharacterTag,
			Snapshot.CurrentSpiceValue,
			Snapshot.CurrentSpiceValue);
		BP_OnInvaderWidgetCharacterSpiceTrackChanged(
			Snapshot.SourcePlayerState,
			Snapshot.SourceCharacterTag,
			Snapshot.CurrentSpiceValue,
			Snapshot.CurrentSpiceValue);

		OnInvaderWidgetCharacterMaxSpiceTrackChanged.Broadcast(
			Snapshot.SourcePlayerState,
			Snapshot.SourceCharacterTag,
			Snapshot.MaxSpiceValue,
			Snapshot.MaxSpiceValue);
		BP_OnInvaderWidgetCharacterMaxSpiceTrackChanged(
			Snapshot.SourcePlayerState,
			Snapshot.SourceCharacterTag,
			Snapshot.MaxSpiceValue,
			Snapshot.MaxSpiceValue);

		OnInvaderWidgetCharacterCursorChanged.Broadcast(
			Snapshot.SourcePlayerState,
			Snapshot.SourceCharacterTag,
			Snapshot.CurrentCursorTier,
			Snapshot.CurrentCursorTier);
		BP_OnInvaderWidgetCharacterCursorChanged(
			Snapshot.SourcePlayerState,
			Snapshot.SourceCharacterTag,
			Snapshot.CurrentCursorTier,
			Snapshot.CurrentCursorTier);
	}

	if (bHasSharedTrackSnapshot)
	{
		OnInvaderWidgetSharedTrackChanged.Broadcast(CachedSharedTrackSlots);
		BP_OnInvaderWidgetSharedTrackChanged(CachedSharedTrackSlots);
	}
}

bool UARInvaderSpicyTrackHUDWidgetBase::GetCharacterStateByTag(
	const FGameplayTag CharacterTag,
	FARInvaderSpicyTrackCharacterState& OutState) const
{
	const FGameplayTag NormalizedCharacterTag = NormalizeCharacterTag(CharacterTag);
	if (!NormalizedCharacterTag.IsValid())
	{
		OutState = FARInvaderSpicyTrackCharacterState();
		return false;
	}

	for (const FARInvaderSpicyTrackCharacterState& Snapshot : CachedCharacterStates)
	{
		if (DoesSnapshotMatchCharacterTag(Snapshot, NormalizedCharacterTag))
		{
			OutState = Snapshot;
			return true;
		}
	}

	OutState = FARInvaderSpicyTrackCharacterState();
	return false;
}

bool UARInvaderSpicyTrackHUDWidgetBase::GetSharedTrackSlotDisplayStates(TArray<FARInvaderTrackSlotDisplayState>& OutSharedTrackSlots) const
{
	OutSharedTrackSlots = bHasSharedTrackSnapshot ? CachedSharedTrackSlots : TArray<FARInvaderTrackSlotDisplayState>();
	return bHasSharedTrackSnapshot;
}

void UARInvaderSpicyTrackHUDWidgetBase::BindInvaderGameStateDelegates()
{
	AARInvaderGameState* InvaderGameState = BoundInvaderGameState.Get();
	if (!InvaderGameState)
	{
		return;
	}

	InvaderGameState->OnTrackedPlayersChanged.AddUniqueDynamic(this, &UARInvaderSpicyTrackHUDWidgetBase::HandleTrackedPlayersChanged);
	InvaderGameState->OnInvaderSharedTrackChanged.AddUniqueDynamic(this, &UARInvaderSpicyTrackHUDWidgetBase::HandleSharedTrackChanged);
}

void UARInvaderSpicyTrackHUDWidgetBase::UnbindInvaderGameStateDelegates()
{
	AARInvaderGameState* InvaderGameState = BoundInvaderGameState.Get();
	if (!InvaderGameState)
	{
		return;
	}

	InvaderGameState->OnTrackedPlayersChanged.RemoveDynamic(this, &UARInvaderSpicyTrackHUDWidgetBase::HandleTrackedPlayersChanged);
	InvaderGameState->OnInvaderSharedTrackChanged.RemoveDynamic(this, &UARInvaderSpicyTrackHUDWidgetBase::HandleSharedTrackChanged);
}

void UARInvaderSpicyTrackHUDWidgetBase::RebindTrackedPlayerStateDelegates()
{
	UnbindTrackedPlayerStateDelegates();

	AARInvaderGameState* InvaderGameState = BoundInvaderGameState.Get();
	if (!InvaderGameState)
	{
		return;
	}

	for (AARPlayerStateBase* PlayerState : InvaderGameState->GetPlayerStates())
	{
		if (!PlayerState)
		{
			continue;
		}

		TrackedPlayerStates.Add(PlayerState);
		PlayerState->OnCurrentCharacterTagChanged.AddUniqueDynamic(this, &UARInvaderSpicyTrackHUDWidgetBase::HandleTrackedPlayerCurrentCharacterTagChanged);
		PlayerState->OnSpiceChanged.AddUniqueDynamic(this, &UARInvaderSpicyTrackHUDWidgetBase::HandleTrackedPlayerSpiceTrackChanged);
		PlayerState->OnMaxSpiceChanged.AddUniqueDynamic(this, &UARInvaderSpicyTrackHUDWidgetBase::HandleTrackedPlayerMaxSpiceTrackChanged);
		PlayerState->OnSpicyTrackCursorChanged.AddUniqueDynamic(this, &UARInvaderSpicyTrackHUDWidgetBase::HandleTrackedPlayerCursorChanged);
	}
}

void UARInvaderSpicyTrackHUDWidgetBase::UnbindTrackedPlayerStateDelegates()
{
	for (const TWeakObjectPtr<AARPlayerStateBase>& TrackedPlayerState : TrackedPlayerStates)
	{
		AARPlayerStateBase* PlayerState = TrackedPlayerState.Get();
		if (!PlayerState)
		{
			continue;
		}

		PlayerState->OnCurrentCharacterTagChanged.RemoveDynamic(this, &UARInvaderSpicyTrackHUDWidgetBase::HandleTrackedPlayerCurrentCharacterTagChanged);
		PlayerState->OnSpiceChanged.RemoveDynamic(this, &UARInvaderSpicyTrackHUDWidgetBase::HandleTrackedPlayerSpiceTrackChanged);
		PlayerState->OnMaxSpiceChanged.RemoveDynamic(this, &UARInvaderSpicyTrackHUDWidgetBase::HandleTrackedPlayerMaxSpiceTrackChanged);
		PlayerState->OnSpicyTrackCursorChanged.RemoveDynamic(this, &UARInvaderSpicyTrackHUDWidgetBase::HandleTrackedPlayerCursorChanged);
	}

	TrackedPlayerStates.Reset();
}

void UARInvaderSpicyTrackHUDWidgetBase::RefreshCachedCharacterStates()
{
	CachedCharacterStates.Reset();

	for (const TWeakObjectPtr<AARPlayerStateBase>& TrackedPlayerState : TrackedPlayerStates)
	{
		AARPlayerStateBase* PlayerState = TrackedPlayerState.Get();
		if (!PlayerState)
		{
			continue;
		}

		FARInvaderSpicyTrackCharacterState Snapshot;
		BuildCharacterStateSnapshot(PlayerState, Snapshot);
		CachedCharacterStates.Add(MoveTemp(Snapshot));
	}

	CachedCharacterStates.Sort([](const FARInvaderSpicyTrackCharacterState& Left, const FARInvaderSpicyTrackCharacterState& Right)
		{
			return Left.SourceCharacterTag.ToString() < Right.SourceCharacterTag.ToString();
		});
}

void UARInvaderSpicyTrackHUDWidgetBase::RefreshSharedTrackSnapshot()
{
	AARInvaderGameState* InvaderGameState = BoundInvaderGameState.Get();
	if (!InvaderGameState)
	{
		CachedSharedTrackSlots.Reset();
		bHasSharedTrackSnapshot = false;
		return;
	}

	InvaderGameState->GetSharedTrackSlotDisplayStates(CachedSharedTrackSlots);
	bHasSharedTrackSnapshot = true;
}

void UARInvaderSpicyTrackHUDWidgetBase::BuildCharacterStateSnapshot(
	AARPlayerStateBase* PlayerState,
	FARInvaderSpicyTrackCharacterState& OutState) const
{
	OutState = FARInvaderSpicyTrackCharacterState();
	if (!PlayerState)
	{
		return;
	}

	OutState.SourcePlayerState = PlayerState;
	OutState.SourceCharacterTag = NormalizeCharacterTag(PlayerState->GetCurrentCharacterTag());
	OutState.CurrentSpiceValue = FMath::Max(0.0f, PlayerState->GetCoreAttributeValue(EARCoreAttributeType::Spice));
	OutState.MaxSpiceValue = FMath::Max(0.0f, PlayerState->GetCoreAttributeValue(EARCoreAttributeType::MaxSpice));
	OutState.CurrentCursorTier = FMath::Max(0, PlayerState->GetEffectiveSpicyTrackCursorTier());
}

void UARInvaderSpicyTrackHUDWidgetBase::HandleTrackedPlayersChanged()
{
	RebindTrackedPlayerStateDelegates();
	RefreshInvaderSpicyTrackSnapshot(true);
}

void UARInvaderSpicyTrackHUDWidgetBase::HandleSharedTrackChanged()
{
	RefreshSharedTrackSnapshot();
	if (!bHasSharedTrackSnapshot)
	{
		return;
	}

	OnInvaderWidgetSharedTrackChanged.Broadcast(CachedSharedTrackSlots);
	BP_OnInvaderWidgetSharedTrackChanged(CachedSharedTrackSlots);
}

void UARInvaderSpicyTrackHUDWidgetBase::HandleTrackedPlayerCurrentCharacterTagChanged(FGameplayTag NewCharacterTag, FGameplayTag OldCharacterTag)
{
	(void)NewCharacterTag;
	(void)OldCharacterTag;
	RefreshInvaderSpicyTrackSnapshot(true);
}

void UARInvaderSpicyTrackHUDWidgetBase::HandleTrackedPlayerSpiceTrackChanged(
	AARPlayerStateBase* SourcePlayerState,
	FGameplayTag SourceCharacterTag,
	const float NewSpiceValue,
	const float OldSpiceValue)
{
	bool bUpdatedSnapshot = false;
	for (FARInvaderSpicyTrackCharacterState& Snapshot : CachedCharacterStates)
	{
		if (Snapshot.SourcePlayerState == SourcePlayerState || DoesSnapshotMatchCharacterTag(Snapshot, SourceCharacterTag))
		{
			Snapshot.SourcePlayerState = SourcePlayerState;
			Snapshot.SourceCharacterTag = NormalizeCharacterTag(SourceCharacterTag);
			Snapshot.CurrentSpiceValue = FMath::Max(0.0f, NewSpiceValue);
			bUpdatedSnapshot = true;
			break;
		}
	}
	if (!bUpdatedSnapshot && SourcePlayerState)
	{
		FARInvaderSpicyTrackCharacterState Snapshot;
		BuildCharacterStateSnapshot(SourcePlayerState, Snapshot);
		Snapshot.SourceCharacterTag = NormalizeCharacterTag(SourceCharacterTag);
		Snapshot.CurrentSpiceValue = FMath::Max(0.0f, NewSpiceValue);
		CachedCharacterStates.Add(MoveTemp(Snapshot));
		CachedCharacterStates.Sort([](const FARInvaderSpicyTrackCharacterState& Left, const FARInvaderSpicyTrackCharacterState& Right)
			{
				return Left.SourceCharacterTag.ToString() < Right.SourceCharacterTag.ToString();
			});
	}

	OnInvaderWidgetCharacterSpiceTrackChanged.Broadcast(
		SourcePlayerState,
		NormalizeCharacterTag(SourceCharacterTag),
		NewSpiceValue,
		OldSpiceValue);
	BP_OnInvaderWidgetCharacterSpiceTrackChanged(
		SourcePlayerState,
		NormalizeCharacterTag(SourceCharacterTag),
		NewSpiceValue,
		OldSpiceValue);
}

void UARInvaderSpicyTrackHUDWidgetBase::HandleTrackedPlayerMaxSpiceTrackChanged(
	AARPlayerStateBase* SourcePlayerState,
	FGameplayTag SourceCharacterTag,
	const float NewMaxSpiceValue,
	const float OldMaxSpiceValue)
{
	bool bUpdatedSnapshot = false;
	for (FARInvaderSpicyTrackCharacterState& Snapshot : CachedCharacterStates)
	{
		if (Snapshot.SourcePlayerState == SourcePlayerState || DoesSnapshotMatchCharacterTag(Snapshot, SourceCharacterTag))
		{
			Snapshot.SourcePlayerState = SourcePlayerState;
			Snapshot.SourceCharacterTag = NormalizeCharacterTag(SourceCharacterTag);
			Snapshot.MaxSpiceValue = FMath::Max(0.0f, NewMaxSpiceValue);
			bUpdatedSnapshot = true;
			break;
		}
	}
	if (!bUpdatedSnapshot && SourcePlayerState)
	{
		FARInvaderSpicyTrackCharacterState Snapshot;
		BuildCharacterStateSnapshot(SourcePlayerState, Snapshot);
		Snapshot.SourceCharacterTag = NormalizeCharacterTag(SourceCharacterTag);
		Snapshot.MaxSpiceValue = FMath::Max(0.0f, NewMaxSpiceValue);
		CachedCharacterStates.Add(MoveTemp(Snapshot));
		CachedCharacterStates.Sort([](const FARInvaderSpicyTrackCharacterState& Left, const FARInvaderSpicyTrackCharacterState& Right)
			{
				return Left.SourceCharacterTag.ToString() < Right.SourceCharacterTag.ToString();
			});
	}

	OnInvaderWidgetCharacterMaxSpiceTrackChanged.Broadcast(
		SourcePlayerState,
		NormalizeCharacterTag(SourceCharacterTag),
		NewMaxSpiceValue,
		OldMaxSpiceValue);
	BP_OnInvaderWidgetCharacterMaxSpiceTrackChanged(
		SourcePlayerState,
		NormalizeCharacterTag(SourceCharacterTag),
		NewMaxSpiceValue,
		OldMaxSpiceValue);
}

void UARInvaderSpicyTrackHUDWidgetBase::HandleTrackedPlayerCursorChanged(
	AARPlayerStateBase* SourcePlayerState,
	FGameplayTag SourceCharacterTag,
	const int32 NewCursorTier,
	const int32 OldCursorTier)
{
	bool bUpdatedSnapshot = false;
	for (FARInvaderSpicyTrackCharacterState& Snapshot : CachedCharacterStates)
	{
		if (Snapshot.SourcePlayerState == SourcePlayerState || DoesSnapshotMatchCharacterTag(Snapshot, SourceCharacterTag))
		{
			Snapshot.SourcePlayerState = SourcePlayerState;
			Snapshot.SourceCharacterTag = NormalizeCharacterTag(SourceCharacterTag);
			Snapshot.CurrentCursorTier = FMath::Max(0, NewCursorTier);
			bUpdatedSnapshot = true;
			break;
		}
	}
	if (!bUpdatedSnapshot && SourcePlayerState)
	{
		FARInvaderSpicyTrackCharacterState Snapshot;
		BuildCharacterStateSnapshot(SourcePlayerState, Snapshot);
		Snapshot.SourceCharacterTag = NormalizeCharacterTag(SourceCharacterTag);
		Snapshot.CurrentCursorTier = FMath::Max(0, NewCursorTier);
		CachedCharacterStates.Add(MoveTemp(Snapshot));
		CachedCharacterStates.Sort([](const FARInvaderSpicyTrackCharacterState& Left, const FARInvaderSpicyTrackCharacterState& Right)
			{
				return Left.SourceCharacterTag.ToString() < Right.SourceCharacterTag.ToString();
			});
	}

	OnInvaderWidgetCharacterCursorChanged.Broadcast(
		SourcePlayerState,
		NormalizeCharacterTag(SourceCharacterTag),
		NewCursorTier,
		OldCursorTier);
	BP_OnInvaderWidgetCharacterCursorChanged(
		SourcePlayerState,
		NormalizeCharacterTag(SourceCharacterTag),
		NewCursorTier,
		OldCursorTier);
}
