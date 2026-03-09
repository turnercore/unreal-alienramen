#include "ARNPCCharacterBase.h"

#include "ARDialogueSubsystem.h"
#include "ARLog.h"
#include "ARNPCSubsystem.h"
#include "ARPlayerController.h"
#include "ARPlayerStateBase.h"
#include "Net/UnrealNetwork.h"

namespace
{
	static uint8 GetTalkableMaskForSlot(const EARPlayerSlot Slot)
	{
		switch (Slot)
		{
		case EARPlayerSlot::P1:
			return 1 << 0;
		case EARPlayerSlot::P2:
			return 1 << 1;
		default:
			return 0;
		}
	}
}

AARNPCCharacterBase::AARNPCCharacterBase()
{
	bReplicates = true;
}

void AARNPCCharacterBase::BeginPlay()
{
	Super::BeginPlay();
	RefreshTalkableFromSubsystem();

	if (HasAuthority())
	{
		if (UARNPCSubsystem* NpcSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UARNPCSubsystem>() : nullptr)
		{
			NpcSubsystem->OnNpcTalkableChanged.AddDynamic(this, &AARNPCCharacterBase::HandleNpcTalkableChanged);
		}
	}
}

void AARNPCCharacterBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		if (UARNPCSubsystem* NpcSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UARNPCSubsystem>() : nullptr)
		{
			NpcSubsystem->OnNpcTalkableChanged.RemoveDynamic(this, &AARNPCCharacterBase::HandleNpcTalkableChanged);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void AARNPCCharacterBase::InteractByController(AARPlayerController* InteractingController)
{
	if (!HasAuthority() || !InteractingController)
	{
		return;
	}

	if (!NpcTag.IsValid())
	{
		UE_LOG(ARLog, Warning, TEXT("[NPC] Interact ignored: '%s' has no NpcTag."), *GetNameSafe(this));
		return;
	}

	if (!bNpcLocalStateAllowsDialogue)
	{
		UE_LOG(ARLog, Verbose, TEXT("[NPC] Interact ignored for '%s': local state blocks dialogue."), *GetNameSafe(this));
		return;
	}

	if (UARDialogueSubsystem* DialogueSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UARDialogueSubsystem>() : nullptr)
	{
		if (!DialogueSubsystem->TryStartDialogueWithNpc(InteractingController, NpcTag))
		{
			UE_LOG(ARLog, Verbose, TEXT("[NPC] TryStartDialogueWithNpc returned false for '%s' with NPC '%s'."), *GetNameSafe(InteractingController), *NpcTag.ToString());
		}
	}
}

void AARNPCCharacterBase::SetNpcLocalStateAllowsDialogue(const bool bEnabled)
{
	if (!HasAuthority())
	{
		return;
	}

	if (bNpcLocalStateAllowsDialogue == bEnabled)
	{
		return;
	}

	bNpcLocalStateAllowsDialogue = bEnabled;
	RefreshTalkableFromSubsystem();
	ForceNetUpdate();
}

void AARNPCCharacterBase::RefreshTalkableFromSubsystem()
{
	if (!HasAuthority() || !NpcTag.IsValid())
	{
		return;
	}

	uint8 NewTalkableMask = 0;
	if (UARDialogueSubsystem* DialogueSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UARDialogueSubsystem>() : nullptr)
	{
		if (bNpcLocalStateAllowsDialogue)
		{
			if (DialogueSubsystem->HasUnlockedDialogueForNpcForSlot(NpcTag, EARPlayerSlot::P1))
			{
				NewTalkableMask |= GetTalkableMaskForSlot(EARPlayerSlot::P1);
			}

			if (DialogueSubsystem->HasUnlockedDialogueForNpcForSlot(NpcTag, EARPlayerSlot::P2))
			{
				NewTalkableMask |= GetTalkableMaskForSlot(EARPlayerSlot::P2);
			}
		}
	}

	const uint8 OldMask = TalkablePlayerSlotMask;
	const bool bMaskChanged = OldMask != NewTalkableMask;
	TalkablePlayerSlotMask = NewTalkableMask;
	if (bMaskChanged)
	{
		OnRep_TalkablePlayerSlotMask(OldMask);
	}

	const bool bNewTalkable = TalkablePlayerSlotMask != 0;
	const bool bTalkableChanged = bIsTalkable != bNewTalkable;
	if (bTalkableChanged)
	{
		const bool bOld = bIsTalkable;
		bIsTalkable = bNewTalkable;
		OnRep_IsTalkable(bOld);
	}

	if (bMaskChanged || bTalkableChanged)
	{
		ForceNetUpdate();
	}
}

void AARNPCCharacterBase::HandleNpcTalkableChanged(FGameplayTag ChangedNpcTag, bool bNewTalkable)
{
	(void)bNewTalkable;
	if (!HasAuthority() || !ChangedNpcTag.MatchesTagExact(NpcTag))
	{
		return;
	}

	RefreshTalkableFromSubsystem();
}

void AARNPCCharacterBase::OnRep_IsTalkable(bool bOldTalkable)
{
	if (bIsTalkable != bOldTalkable)
	{
		OnNpcTalkableStateChanged.Broadcast(bIsTalkable);
	}
}

void AARNPCCharacterBase::OnRep_TalkablePlayerSlotMask(uint8 bOldTalkablePlayerSlotMask)
{
	(void)bOldTalkablePlayerSlotMask;
}

bool AARNPCCharacterBase::IsTalkableForPlayerSlot(const EARPlayerSlot PlayerSlot) const
{
	const uint8 SlotMask = GetTalkableMaskForSlot(PlayerSlot);
	return SlotMask != 0 && (TalkablePlayerSlotMask & SlotMask) != 0;
}

bool AARNPCCharacterBase::IsTalkableForController(const AARPlayerController* QueryController) const
{
	if (!QueryController)
	{
		return false;
	}

	const AARPlayerStateBase* QueryPlayerState = QueryController->GetPlayerState<AARPlayerStateBase>();
	if (!QueryPlayerState)
	{
		return false;
	}

	return IsTalkableForPlayerSlot(QueryPlayerState->GetPlayerSlot());
}

void AARNPCCharacterBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AARNPCCharacterBase, bIsTalkable);
	DOREPLIFETIME(AARNPCCharacterBase, TalkablePlayerSlotMask);
}
