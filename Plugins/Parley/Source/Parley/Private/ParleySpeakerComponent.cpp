#include "ParleySpeakerComponent.h"

#include "ParleyDialogueSubsystem.h"
#include "ParleyDialogueSettings.h"
#include "ParleyLog.h"
#include "ParleyPlayerControllerInterface.h"
#include "ParleyPlayerSlotHelpers.h"
#include "ParleySpeakerSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

namespace
{
	static uint8 GetTalkableMaskForSlotIndex(const int32 SlotIndex)
	{
		switch (SlotIndex)
		{
		case 0:
			return 1 << 0;
		case 1:
			return 1 << 1;
		default:
			return 0;
		}
	}
}

UParleySpeakerComponent::UParleySpeakerComponent()
{
	SetIsReplicatedByDefault(true);
}

void UParleySpeakerComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!IsAuthorityOwner())
	{
		return;
	}

	RefreshTalkableFromSubsystem();

	if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		if (UParleySpeakerSubsystem* SpeakerSubsystem = GameInstance->GetSubsystem<UParleySpeakerSubsystem>())
		{
			SpeakerSubsystem->OnSpeakerTalkableChanged.AddDynamic(this, &UParleySpeakerComponent::HandleSpeakerTalkableChanged);
		}
	}
}

void UParleySpeakerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsAuthorityOwner())
	{
		if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
		{
			if (UParleySpeakerSubsystem* SpeakerSubsystem = GameInstance->GetSubsystem<UParleySpeakerSubsystem>())
			{
				SpeakerSubsystem->OnSpeakerTalkableChanged.RemoveDynamic(this, &UParleySpeakerComponent::HandleSpeakerTalkableChanged);
			}
		}
	}

	Super::EndPlay(EndPlayReason);
}

void UParleySpeakerComponent::InteractByController(APlayerController* InteractingController)
{
	if (!IsAuthorityOwner() || !InteractingController)
	{
		return;
	}

	if (!SpeakerTag.IsValid())
	{
		UE_LOG(ParleyLog, Warning, TEXT("[Speaker] Interact ignored: '%s' has no SpeakerTag."), *GetNameSafe(GetOwner()));
		return;
	}

	if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		if (UParleyDialogueSubsystem* DialogueSubsystem = GameInstance->GetSubsystem<UParleyDialogueSubsystem>())
		{
			// Prefer explicit source->target speaker identity when the interacting pawn has a speaker component.
			FGameplayTag SourceSpeakerTag;
			if (const APawn* InteractingPawn = InteractingController->GetPawn())
			{
				if (const UParleySpeakerComponent* SourceSpeakerComponent = InteractingPawn->FindComponentByClass<UParleySpeakerComponent>())
				{
					SourceSpeakerTag = SourceSpeakerComponent->GetSpeakerTag();
				}
			}

			const bool bStarted = SourceSpeakerTag.IsValid()
				? DialogueSubsystem->TryStartDialogueBetweenSpeakers(InteractingController, SourceSpeakerTag, SpeakerTag)
				: DialogueSubsystem->TryStartDialogueWithSpeaker(InteractingController, SpeakerTag);
			if (!bStarted)
			{
				UE_LOG(
					ParleyLog,
					Verbose,
					TEXT("[Speaker] TryStartDialogueWithSpeaker returned false for '%s' with speaker '%s'."),
					*GetNameSafe(InteractingController),
					*SpeakerTag.ToString());
			}
		}
	}
}

void UParleySpeakerComponent::InteractWithSpeakerByController(APlayerController* InteractingController, UParleySpeakerComponent* TargetSpeakerComponent)
{
	if (!IsAuthorityOwner() || !InteractingController || !TargetSpeakerComponent)
	{
		return;
	}

	const FGameplayTag SourceSpeakerTag = GetSpeakerTag();
	const FGameplayTag TargetSpeakerTag = TargetSpeakerComponent->GetSpeakerTag();
	if (!SourceSpeakerTag.IsValid() || !TargetSpeakerTag.IsValid())
	{
		UE_LOG(
			ParleyLog,
			Verbose,
			TEXT("[Speaker] InteractWithSpeakerByController ignored on '%s': invalid source/target speaker tags."),
			*GetNameSafe(GetOwner()));
		return;
	}

	if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		if (UParleyDialogueSubsystem* DialogueSubsystem = GameInstance->GetSubsystem<UParleyDialogueSubsystem>())
		{
			if (!DialogueSubsystem->TryStartDialogueBetweenSpeakers(InteractingController, SourceSpeakerTag, TargetSpeakerTag))
			{
				UE_LOG(
					ParleyLog,
					Verbose,
					TEXT("[Speaker] TryStartDialogueBetweenSpeakers returned false for '%s' source='%s' target='%s'."),
					*GetNameSafe(InteractingController),
					*SourceSpeakerTag.ToString(),
					*TargetSpeakerTag.ToString());
			}
		}
	}
}

void UParleySpeakerComponent::SetSpeakerTag(const FGameplayTag NewSpeakerTag)
{
	if (SpeakerTag.MatchesTagExact(NewSpeakerTag))
	{
		return;
	}

	SpeakerTag = NewSpeakerTag;
	if (IsAuthorityOwner())
	{
		RefreshTalkableFromSubsystem();
	}
}

void UParleySpeakerComponent::RefreshTalkableFromSubsystem()
{
	if (!IsAuthorityOwner() || !SpeakerTag.IsValid())
	{
		UE_LOG(
			ParleyLog,
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
		if (UParleyDialogueSubsystem* DialogueSubsystem = GameInstance->GetSubsystem<UParleyDialogueSubsystem>())
		{
			const bool bP1Talkable = DialogueSubsystem->HasUnlockedDialogueForSpeakerForSlot(SpeakerTag, ParleyPlayerSlot::GetP1Tag());
			const bool bP2Talkable = DialogueSubsystem->HasUnlockedDialogueForSpeakerForSlot(SpeakerTag, ParleyPlayerSlot::GetP2Tag());
			if (bP1Talkable)
			{
				NewTalkableMask |= GetTalkableMaskForSlotIndex(0);
			}

			if (bP2Talkable)
			{
				NewTalkableMask |= GetTalkableMaskForSlotIndex(1);
			}

			UE_LOG(
				ParleyLog,
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
			UE_LOG(ParleyLog, Verbose, TEXT("[Speaker] Component refresh '%s': dialogue subsystem unavailable."), *GetNameSafe(GetOwner()));
		}
	}
	else
	{
		UE_LOG(ParleyLog, Verbose, TEXT("[Speaker] Component refresh '%s': game instance unavailable."), *GetNameSafe(GetOwner()));
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

void UParleySpeakerComponent::HandleSpeakerTalkableChanged(const FGameplayTag ChangedSpeakerTag, const bool bNewTalkable)
{
	(void)bNewTalkable;
	if (!IsAuthorityOwner() || !ChangedSpeakerTag.MatchesTagExact(SpeakerTag))
	{
		return;
	}

	RefreshTalkableFromSubsystem();
}

void UParleySpeakerComponent::OnRep_IsTalkable(const bool bOldTalkable)
{
	if (bIsTalkable != bOldTalkable)
	{
		OnSpeakerTalkableStateChanged.Broadcast(bIsTalkable);
	}
}

void UParleySpeakerComponent::OnRep_TalkablePlayerSlotMask(const uint8 bOldTalkablePlayerSlotMask)
{
	if (bOldTalkablePlayerSlotMask == TalkablePlayerSlotMask)
	{
		return;
	}

	// Always broadcast on slot-mask changes so listeners refresh per-slot indicators even
	// if bIsTalkable replication is delayed or unchanged.
	OnSpeakerTalkableStateChanged.Broadcast(TalkablePlayerSlotMask != 0);
}

bool UParleySpeakerComponent::IsTalkableForPlayerSlotTag(const FGameplayTag PlayerSlotTag) const
{
	const uint8 SlotMask = GetTalkableMaskForSlotIndex(ParleyPlayerSlot::GetIndexForTag(PlayerSlotTag));
	return SlotMask != 0 && (TalkablePlayerSlotMask & SlotMask) != 0;
}

bool UParleySpeakerComponent::IsTalkableForController(const APlayerController* QueryController) const
{
	if (!QueryController || !QueryController->GetClass()->ImplementsInterface(UParleyPlayerControllerInterface::StaticClass()))
	{
		return false;
	}

	const IParleyPlayerControllerInterface* ControllerInterface = Cast<IParleyPlayerControllerInterface>(QueryController);
	if (!ControllerInterface)
	{
		return false;
	}

	const FGameplayTag SlotTag = ControllerInterface->GetPlayerSlotTag();
	return IsTalkableForPlayerSlotTag(SlotTag);
}

bool UParleySpeakerComponent::HasSomethingToSay() const
{
	return bIsTalkable;
}

bool UParleySpeakerComponent::IsAuthorityOwner() const
{
	const AActor* OwnerActor = GetOwner();
	return OwnerActor && OwnerActor->HasAuthority();
}

void UParleySpeakerComponent::ForceOwnerNetUpdate() const
{
	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->ForceNetUpdate();
	}
}

void UParleySpeakerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UParleySpeakerComponent, bIsTalkable);
	DOREPLIFETIME(UParleySpeakerComponent, TalkablePlayerSlotMask);
}
