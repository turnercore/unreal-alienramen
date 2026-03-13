#include "ARSpeakerComponent.h"

#include "ARCustomerComponent.h"
#include "AREmotionComponent.h"
#include "ARDialogueSubsystem.h"
#include "ARLog.h"
#include "ARSpeakerSubsystem.h"
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

UARSpeakerComponent::UARSpeakerComponent()
{
	SetIsReplicatedByDefault(true);
}

void UARSpeakerComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!IsAuthorityOwner())
	{
		return;
	}

	RefreshTalkableFromSubsystem();

	if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		if (UARSpeakerSubsystem* SpeakerSubsystem = GameInstance->GetSubsystem<UARSpeakerSubsystem>())
		{
			SpeakerSubsystem->OnSpeakerTalkableChanged.AddDynamic(this, &UARSpeakerComponent::HandleSpeakerTalkableChanged);
		}
	}
}

void UARSpeakerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsAuthorityOwner())
	{
		if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
		{
			if (UARSpeakerSubsystem* SpeakerSubsystem = GameInstance->GetSubsystem<UARSpeakerSubsystem>())
			{
				SpeakerSubsystem->OnSpeakerTalkableChanged.RemoveDynamic(this, &UARSpeakerComponent::HandleSpeakerTalkableChanged);
			}
		}
	}

	Super::EndPlay(EndPlayReason);
}

void UARSpeakerComponent::InteractByController(AARPlayerController* InteractingController)
{
	if (!IsAuthorityOwner() || !InteractingController)
	{
		return;
	}

	if (!SpeakerTag.IsValid())
	{
		UE_LOG(ARLog, Warning, TEXT("[Speaker] Interact ignored: '%s' has no SpeakerTag."), *GetNameSafe(GetOwner()));
		return;
	}

	if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		if (UARDialogueSubsystem* DialogueSubsystem = GameInstance->GetSubsystem<UARDialogueSubsystem>())
		{
			if (!DialogueSubsystem->TryStartDialogueWithSpeaker(InteractingController, SpeakerTag))
			{
				UE_LOG(
					ARLog,
					Verbose,
					TEXT("[Speaker] TryStartDialogueWithSpeaker returned false for '%s' with speaker '%s'."),
					*GetNameSafe(InteractingController),
					*SpeakerTag.ToString());
			}
		}
	}
}

void UARSpeakerComponent::SetSpeakerTag(const FGameplayTag NewSpeakerTag)
{
	if (SpeakerTag.MatchesTagExact(NewSpeakerTag))
	{
		return;
	}

	SpeakerTag = NewSpeakerTag;
	if (IsAuthorityOwner())
	{
		if (AActor* OwnerActor = GetOwner())
		{
			if (UAREmotionComponent* EmotionComponent = OwnerActor->FindComponentByClass<UAREmotionComponent>())
			{
				EmotionComponent->SetRegisteredSpeakerTag(SpeakerTag);
			}
		}

		RefreshTalkableFromSubsystem();
	}
}

void UARSpeakerComponent::RefreshTalkableFromSubsystem()
{
	if (!IsAuthorityOwner() || !SpeakerTag.IsValid())
	{
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[Speaker] Component refresh skipped for '%s': Authority=%s SpeakerTagValid=%s"),
			*GetNameSafe(GetOwner()),
			IsAuthorityOwner() ? TEXT("true") : TEXT("false"),
			SpeakerTag.IsValid() ? TEXT("true") : TEXT("false"));
		return;
	}

	const uint8 OldMask = TalkablePlayerSlotMask;
	uint8 NewTalkableMask = 0;
	if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		if (UARDialogueSubsystem* DialogueSubsystem = GameInstance->GetSubsystem<UARDialogueSubsystem>())
		{
			const bool bP1Talkable = DialogueSubsystem->HasUnlockedDialogueForSpeakerForSlot(SpeakerTag, EARPlayerSlot::P1);
			const bool bP2Talkable = DialogueSubsystem->HasUnlockedDialogueForSpeakerForSlot(SpeakerTag, EARPlayerSlot::P2);
			if (bP1Talkable)
			{
				NewTalkableMask |= GetTalkableMaskForSlot(EARPlayerSlot::P1);
			}

			if (bP2Talkable)
			{
				NewTalkableMask |= GetTalkableMaskForSlot(EARPlayerSlot::P2);
			}

			UE_LOG(
				ARLog,
				Verbose,
				TEXT("[Speaker] Component eval '%s' (%s): P1=%s P2=%s Mask=0x%02x->0x%02x"),
				*GetNameSafe(GetOwner()),
				*SpeakerTag.ToString(),
				bP1Talkable ? TEXT("true") : TEXT("false"),
				bP2Talkable ? TEXT("true") : TEXT("false"),
				OldMask,
				NewTalkableMask);
		}
		else
		{
			UE_LOG(ARLog, Verbose, TEXT("[Speaker] Component refresh '%s': dialogue subsystem unavailable."), *GetNameSafe(GetOwner()));
		}
	}
	else
	{
		UE_LOG(ARLog, Verbose, TEXT("[Speaker] Component refresh '%s': game instance unavailable."), *GetNameSafe(GetOwner()));
	}

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

void UARSpeakerComponent::HandleSpeakerTalkableChanged(const FGameplayTag ChangedSpeakerTag, const bool bNewTalkable)
{
	(void)bNewTalkable;
	if (!IsAuthorityOwner() || !ChangedSpeakerTag.MatchesTagExact(SpeakerTag))
	{
		return;
	}

	RefreshTalkableFromSubsystem();
}

void UARSpeakerComponent::OnRep_IsTalkable(const bool bOldTalkable)
{
	if (bIsTalkable != bOldTalkable)
	{
		OnSpeakerTalkableStateChanged.Broadcast(bIsTalkable);
	}
}

void UARSpeakerComponent::OnRep_TalkablePlayerSlotMask(const uint8 bOldTalkablePlayerSlotMask)
{
	if (bOldTalkablePlayerSlotMask == TalkablePlayerSlotMask)
	{
		return;
	}

	// Always broadcast on slot-mask changes so listeners refresh per-slot indicators even
	// if bIsTalkable replication is delayed or unchanged.
	OnSpeakerTalkableStateChanged.Broadcast(TalkablePlayerSlotMask != 0);
}

bool UARSpeakerComponent::IsTalkableForPlayerSlot(const EARPlayerSlot PlayerSlot) const
{
	const uint8 SlotMask = GetTalkableMaskForSlot(PlayerSlot);
	return SlotMask != 0 && (TalkablePlayerSlotMask & SlotMask) != 0;
}

bool UARSpeakerComponent::IsTalkableForController(const AARPlayerController* QueryController) const
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

bool UARSpeakerComponent::HasSomethingToSay() const
{
	if (bIsTalkable)
	{
		return true;
	}

	const AActor* OwnerActor = GetOwner();
	const UARCustomerComponent* CustomerComponent = OwnerActor ? OwnerActor->FindComponentByClass<UARCustomerComponent>() : nullptr;
	return CustomerComponent && CustomerComponent->HasOrderForInteraction();
}

bool UARSpeakerComponent::IsAuthorityOwner() const
{
	const AActor* OwnerActor = GetOwner();
	return OwnerActor && OwnerActor->HasAuthority();
}

void UARSpeakerComponent::ForceOwnerNetUpdate() const
{
	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->ForceNetUpdate();
	}
}

void UARSpeakerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UARSpeakerComponent, bIsTalkable);
	DOREPLIFETIME(UARSpeakerComponent, TalkablePlayerSlotMask);
}
