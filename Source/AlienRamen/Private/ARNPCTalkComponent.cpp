#include "ARNPCTalkComponent.h"

#include "AREmotionComponent.h"
#include "ARDialogueSubsystem.h"
#include "ARLog.h"
#include "ARNPCSubsystem.h"
#include "ARPlayerController.h"
#include "ARPlayerStateBase.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
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

UARNPCTalkComponent::UARNPCTalkComponent()
{
	SetIsReplicatedByDefault(true);
}

void UARNPCTalkComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!IsAuthorityOwner())
	{
		return;
	}

	RefreshTalkableFromSubsystem();

	if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		if (UARNPCSubsystem* NpcSubsystem = GameInstance->GetSubsystem<UARNPCSubsystem>())
		{
			NpcSubsystem->OnNpcTalkableChanged.AddDynamic(this, &UARNPCTalkComponent::HandleNpcTalkableChanged);
		}
	}
}

void UARNPCTalkComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsAuthorityOwner())
	{
		if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
		{
			if (UARNPCSubsystem* NpcSubsystem = GameInstance->GetSubsystem<UARNPCSubsystem>())
			{
				NpcSubsystem->OnNpcTalkableChanged.RemoveDynamic(this, &UARNPCTalkComponent::HandleNpcTalkableChanged);
			}
		}
	}

	Super::EndPlay(EndPlayReason);
}

void UARNPCTalkComponent::InteractByController(AARPlayerController* InteractingController)
{
	if (!IsAuthorityOwner() || !InteractingController)
	{
		return;
	}

	if (!NpcTag.IsValid())
	{
		UE_LOG(ARLog, Warning, TEXT("[NPC] Interact ignored: '%s' has no NpcTag."), *GetNameSafe(GetOwner()));
		return;
	}

	if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		if (UARDialogueSubsystem* DialogueSubsystem = GameInstance->GetSubsystem<UARDialogueSubsystem>())
		{
			if (!DialogueSubsystem->TryStartDialogueWithNpc(InteractingController, NpcTag))
			{
				UE_LOG(
					ARLog,
					Verbose,
					TEXT("[NPC] TryStartDialogueWithNpc returned false for '%s' with NPC '%s'."),
					*GetNameSafe(InteractingController),
					*NpcTag.ToString());
			}
		}
	}
}

void UARNPCTalkComponent::SetNpcTag(const FGameplayTag NewNpcTag)
{
	if (NpcTag.MatchesTagExact(NewNpcTag))
	{
		return;
	}

	NpcTag = NewNpcTag;
	if (IsAuthorityOwner())
	{
		if (AActor* OwnerActor = GetOwner())
		{
			if (UAREmotionComponent* EmotionComponent = OwnerActor->FindComponentByClass<UAREmotionComponent>())
			{
				EmotionComponent->SetRegisteredSpeakerTag(NpcTag);
			}
		}

		RefreshTalkableFromSubsystem();
	}
}

void UARNPCTalkComponent::RefreshTalkableFromSubsystem()
{
	if (!IsAuthorityOwner() || !NpcTag.IsValid())
	{
		return;
	}

	uint8 NewTalkableMask = 0;
	if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		if (UARDialogueSubsystem* DialogueSubsystem = GameInstance->GetSubsystem<UARDialogueSubsystem>())
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
		const bool bOldTalkable = bIsTalkable;
		bIsTalkable = bNewTalkable;
		OnRep_IsTalkable(bOldTalkable);
	}

	if (bMaskChanged || bTalkableChanged)
	{
		ForceOwnerNetUpdate();
	}
}

void UARNPCTalkComponent::HandleNpcTalkableChanged(const FGameplayTag ChangedNpcTag, const bool bNewTalkable)
{
	(void)bNewTalkable;
	if (!IsAuthorityOwner() || !ChangedNpcTag.MatchesTagExact(NpcTag))
	{
		return;
	}

	RefreshTalkableFromSubsystem();
}

void UARNPCTalkComponent::OnRep_IsTalkable(const bool bOldTalkable)
{
	if (bIsTalkable != bOldTalkable)
	{
		OnNpcTalkableStateChanged.Broadcast(bIsTalkable);
	}
}

void UARNPCTalkComponent::OnRep_TalkablePlayerSlotMask(const uint8 bOldTalkablePlayerSlotMask)
{
	if (bOldTalkablePlayerSlotMask == TalkablePlayerSlotMask)
	{
		return;
	}

	// Always broadcast on slot-mask changes so listeners refresh per-slot indicators even
	// if bIsTalkable replication is delayed or unchanged.
	OnNpcTalkableStateChanged.Broadcast(TalkablePlayerSlotMask != 0);
}

bool UARNPCTalkComponent::IsTalkableForPlayerSlot(const EARPlayerSlot PlayerSlot) const
{
	const uint8 SlotMask = GetTalkableMaskForSlot(PlayerSlot);
	return SlotMask != 0 && (TalkablePlayerSlotMask & SlotMask) != 0;
}

bool UARNPCTalkComponent::IsTalkableForController(const AARPlayerController* QueryController) const
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

bool UARNPCTalkComponent::IsAuthorityOwner() const
{
	const AActor* OwnerActor = GetOwner();
	return OwnerActor && OwnerActor->HasAuthority();
}

void UARNPCTalkComponent::ForceOwnerNetUpdate() const
{
	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->ForceNetUpdate();
	}
}

void UARNPCTalkComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UARNPCTalkComponent, bIsTalkable);
	DOREPLIFETIME(UARNPCTalkComponent, TalkablePlayerSlotMask);
}
