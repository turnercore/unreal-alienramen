#include "ARInvaderPlayerController.h"

#include "ARInvaderGameState.h"
#include "ARPlayerStateBase.h"

AARInvaderPlayerController::AARInvaderPlayerController()
{
}

AARPlayerStateBase* AARInvaderPlayerController::GetInvaderPlayerState() const
{
	return GetPlayerState<AARPlayerStateBase>();
}

void AARInvaderPlayerController::RequestActivateFullBlast()
{
	if (HasAuthority())
	{
		ServerRequestActivateFullBlast_Implementation();
		return;
	}

	ServerRequestActivateFullBlast();
}

void AARInvaderPlayerController::ServerRequestActivateFullBlast_Implementation()
{
	if (AARInvaderGameState* InvaderGameState = GetWorld() ? GetWorld()->GetGameState<AARInvaderGameState>() : nullptr)
	{
		InvaderGameState->RequestActivateFullBlast(GetInvaderPlayerState());
	}
}

void AARInvaderPlayerController::RequestResolveFullBlastSelection(const FGameplayTag SelectedUpgradeTag, const int32 DesiredDestinationSlot)
{
	if (HasAuthority())
	{
		ServerRequestResolveFullBlastSelection_Implementation(SelectedUpgradeTag, DesiredDestinationSlot);
		return;
	}

	ServerRequestResolveFullBlastSelection(SelectedUpgradeTag, DesiredDestinationSlot);
}

void AARInvaderPlayerController::ServerRequestResolveFullBlastSelection_Implementation(const FGameplayTag SelectedUpgradeTag, const int32 DesiredDestinationSlot)
{
	if (AARInvaderGameState* InvaderGameState = GetWorld() ? GetWorld()->GetGameState<AARInvaderGameState>() : nullptr)
	{
		InvaderGameState->ResolveFullBlastSelection(GetInvaderPlayerState(), SelectedUpgradeTag, DesiredDestinationSlot);
	}
}

void AARInvaderPlayerController::RequestResolveFullBlastSkip()
{
	if (HasAuthority())
	{
		ServerRequestResolveFullBlastSkip_Implementation();
		return;
	}

	ServerRequestResolveFullBlastSkip();
}

void AARInvaderPlayerController::ServerRequestResolveFullBlastSkip_Implementation()
{
	if (AARInvaderGameState* InvaderGameState = GetWorld() ? GetWorld()->GetGameState<AARInvaderGameState>() : nullptr)
	{
		InvaderGameState->ResolveFullBlastSkip(GetInvaderPlayerState());
	}
}

void AARInvaderPlayerController::RequestActivateTrackUpgrade(const int32 SlotIndex)
{
	if (HasAuthority())
	{
		ServerRequestActivateTrackUpgrade_Implementation(SlotIndex);
		return;
	}

	ServerRequestActivateTrackUpgrade(SlotIndex);
}

void AARInvaderPlayerController::ServerRequestActivateTrackUpgrade_Implementation(const int32 SlotIndex)
{
	if (AARInvaderGameState* InvaderGameState = GetWorld() ? GetWorld()->GetGameState<AARInvaderGameState>() : nullptr)
	{
		InvaderGameState->ActivateTrackUpgrade(GetInvaderPlayerState(), SlotIndex);
	}
}

void AARInvaderPlayerController::HandleSpiceTrackDeltaInput(const float AxisValue)
{
	if (AARPlayerStateBase* InvaderPlayerState = GetInvaderPlayerState())
	{
		int32 DeltaTier = 0;
		if (AxisValue > 0.5f)
		{
			DeltaTier = 1;
		}
		else if (AxisValue < -0.5f)
		{
			DeltaTier = -1;
		}

		if (DeltaTier != 0)
		{
			InvaderPlayerState->AdjustSpicyTrackCursorTier(DeltaTier);
		}
	}
}

void AARInvaderPlayerController::HandleSpiceTrackActivateFromCursor()
{
	AARPlayerStateBase* InvaderPlayerState = GetInvaderPlayerState();
	if (!InvaderPlayerState)
	{
		return;
	}

	const int32 CursorTier = InvaderPlayerState->GetEffectiveSpicyTrackCursorTier();
	if (CursorTier <= 0)
	{
		RequestActivateFullBlast();
		return;
	}

	RequestActivateTrackUpgrade(CursorTier);
}

void AARInvaderPlayerController::RequestStartSharingSpice()
{
	if (HasAuthority())
	{
		ServerRequestStartSharingSpice_Implementation();
		return;
	}

	ServerRequestStartSharingSpice();
}

void AARInvaderPlayerController::ServerRequestStartSharingSpice_Implementation()
{
	if (AARInvaderGameState* InvaderGameState = GetWorld() ? GetWorld()->GetGameState<AARInvaderGameState>() : nullptr)
	{
		InvaderGameState->StartSharingSpice(GetInvaderPlayerState());
	}
}

void AARInvaderPlayerController::RequestStopSharingSpice()
{
	if (HasAuthority())
	{
		ServerRequestStopSharingSpice_Implementation();
		return;
	}

	ServerRequestStopSharingSpice();
}

void AARInvaderPlayerController::ServerRequestStopSharingSpice_Implementation()
{
	if (AARInvaderGameState* InvaderGameState = GetWorld() ? GetWorld()->GetGameState<AARInvaderGameState>() : nullptr)
	{
		InvaderGameState->StopSharingSpice(GetInvaderPlayerState());
	}
}

void AARInvaderPlayerController::RequestSetOfferPresence(
	const FGameplayTag HoveredUpgradeTag,
	const int32 HoveredDestinationSlot,
	const FVector2D CursorNormalized,
	const bool bHasCursor)
{
	if (HasAuthority())
	{
		ServerRequestSetOfferPresence_Implementation(HoveredUpgradeTag, HoveredDestinationSlot, CursorNormalized, bHasCursor);
		return;
	}

	ServerRequestSetOfferPresence(HoveredUpgradeTag, HoveredDestinationSlot, CursorNormalized, bHasCursor);
}

void AARInvaderPlayerController::ServerRequestSetOfferPresence_Implementation(
	const FGameplayTag HoveredUpgradeTag,
	const int32 HoveredDestinationSlot,
	const FVector2D CursorNormalized,
	const bool bHasCursor)
{
	if (AARInvaderGameState* InvaderGameState = GetWorld() ? GetWorld()->GetGameState<AARInvaderGameState>() : nullptr)
	{
		InvaderGameState->SetOfferPresence(GetInvaderPlayerState(), HoveredUpgradeTag, HoveredDestinationSlot, CursorNormalized, bHasCursor);
	}
}

void AARInvaderPlayerController::RequestClearOfferPresence()
{
	if (HasAuthority())
	{
		ServerRequestClearOfferPresence_Implementation();
		return;
	}

	ServerRequestClearOfferPresence();
}

void AARInvaderPlayerController::ServerRequestClearOfferPresence_Implementation()
{
	if (AARInvaderGameState* InvaderGameState = GetWorld() ? GetWorld()->GetGameState<AARInvaderGameState>() : nullptr)
	{
		InvaderGameState->ClearOfferPresence(GetInvaderPlayerState());
	}
}
