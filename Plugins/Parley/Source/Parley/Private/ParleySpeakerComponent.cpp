#include "ParleySpeakerComponent.h"

#include "ParleyDialogueSubsystem.h"
#include "ParleyDialogueSettings.h"
#include "ParleyLog.h"
#include "ParleyPlayerControllerInterface.h"
#include "ParleySpeakerSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "UObject/UnrealType.h"

namespace
{
	static FGameplayTag ReadCharacterTagProperty(const UObject* Object, const FName PropertyName)
	{
		if (!Object)
		{
			return FGameplayTag();
		}

		const FStructProperty* Property = FindFProperty<FStructProperty>(Object->GetClass(), PropertyName);
		if (!Property || Property->Struct != TBaseStructure<FGameplayTag>::Get())
		{
			return FGameplayTag();
		}

		return *Property->ContainerPtrToValuePtr<FGameplayTag>(Object);
	}

	static bool AreCharacterTagContainersEquivalent(const FGameplayTagContainer& Left, const FGameplayTagContainer& Right)
	{
		return Left.Num() == Right.Num() && Left.HasAllExact(Right) && Right.HasAllExact(Left);
	}

	static void GatherControlledCharacterTags(const UWorld* World, TArray<FGameplayTag>& OutCharacterTags)
	{
		OutCharacterTags.Reset();
		if (!World)
		{
			return;
		}

		const AGameStateBase* GameState = World->GetGameState<AGameStateBase>();
		if (!GameState)
		{
			return;
		}

		for (APlayerState* PlayerState : GameState->PlayerArray)
		{
			FGameplayTag CharacterTag;
			if (const APlayerController* OwnerController = PlayerState ? Cast<APlayerController>(PlayerState->GetOwner()) : nullptr)
			{
				if (const APawn* Pawn = OwnerController->GetPawn())
				{
					if (const UParleySpeakerComponent* SpeakerComponent = Pawn->FindComponentByClass<UParleySpeakerComponent>())
					{
						CharacterTag = SpeakerComponent->GetSpeakerTag();
					}
				}
			}
			if (!CharacterTag.IsValid())
			{
				CharacterTag = ReadCharacterTagProperty(PlayerState, TEXT("CurrentCharacterTag"));
			}
			if (CharacterTag.IsValid())
			{
				OutCharacterTags.AddUnique(CharacterTag);
			}
		}
	}

	static FGameplayTag ResolveSourceSpeakerTagFromController(const APlayerController* InteractingController)
	{
		if (!InteractingController)
		{
			return FGameplayTag();
		}

		if (const APawn* InteractingPawn = InteractingController->GetPawn())
		{
			if (const UParleySpeakerComponent* SourceSpeakerComponent = InteractingPawn->FindComponentByClass<UParleySpeakerComponent>())
			{
				return SourceSpeakerComponent->GetSpeakerTag();
			}
		}

		return FGameplayTag();
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
			const FGameplayTag SourceSpeakerTag = ResolveSourceSpeakerTagFromController(InteractingController);
			if (!SourceSpeakerTag.IsValid())
			{
				UE_LOG(
					ParleyLog,
					Verbose,
					TEXT("[Speaker] Interact ignored for '%s': controller '%s' has no possessed pawn speaker component."),
					*GetNameSafe(GetOwner()),
					*GetNameSafe(InteractingController));
				return;
			}

			const bool bStarted = DialogueSubsystem->TryStartDialogueBetweenSpeakers(InteractingController, SourceSpeakerTag, SpeakerTag);
			if (!bStarted)
			{
				UE_LOG(
					ParleyLog,
					Verbose,
					TEXT("[Speaker] Dialogue start returned false for '%s' source='%s' target='%s'."),
					*GetNameSafe(InteractingController),
					*SourceSpeakerTag.ToString(),
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
		ForceOwnerNetUpdate();
	}
}

void UParleySpeakerComponent::RefreshTalkableFromSubsystem()
{
	if (!IsAuthorityOwner())
	{
		UE_LOG(
			ParleyLog,
			Verbose,
			TEXT("[Speaker] Component refresh skipped for '%s': authority required."),
			*GetNameSafe(GetOwner()));
		return;
	}

	const FGameplayTagContainer OldTalkableCharacterTags = TalkableCharacterTags;
	FGameplayTagContainer NewTalkableCharacterTags;

	if (!SpeakerTag.IsValid())
	{
		UE_LOG(ParleyLog, Verbose, TEXT("[Speaker] Component refresh '%s': invalid speaker tag; clearing talkable state."), *GetNameSafe(GetOwner()));
	}
	else if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		if (UParleyDialogueSubsystem* DialogueSubsystem = GameInstance->GetSubsystem<UParleyDialogueSubsystem>())
		{
			TArray<FGameplayTag> ControlledCharacterTags;
			GatherControlledCharacterTags(GetWorld(), ControlledCharacterTags);
			for (const FGameplayTag CharacterTag : ControlledCharacterTags)
			{
				if (DialogueSubsystem->HasUnlockedDialogueForSpeakerForCharacter(SpeakerTag, CharacterTag))
				{
					NewTalkableCharacterTags.AddTag(CharacterTag);
				}
			}

			UE_LOG(
				ParleyLog,
				Verbose,
				TEXT("[Speaker] Component eval '%s' (%s): TalkableCharacters=%d"),
				*GetNameSafe(GetOwner()),
				*SpeakerTag.ToString(),
				NewTalkableCharacterTags.Num());
		}
		else
		{
			UE_LOG(ParleyLog, Verbose, TEXT("[Speaker] Component refresh '%s': dialogue subsystem unavailable; clearing talkable state."), *GetNameSafe(GetOwner()));
		}
	}
	else
	{
		UE_LOG(ParleyLog, Verbose, TEXT("[Speaker] Component refresh '%s': game instance unavailable; clearing talkable state."), *GetNameSafe(GetOwner()));
	}

	const bool bCharacterTagsChanged = !AreCharacterTagContainersEquivalent(OldTalkableCharacterTags, NewTalkableCharacterTags);
	TalkableCharacterTags = NewTalkableCharacterTags;
	if (bCharacterTagsChanged)
	{
		OnRep_TalkableCharacterTags(OldTalkableCharacterTags);
	}

	const bool bNewTalkable = TalkableCharacterTags.Num() > 0;
	const bool bTalkableChanged = bIsTalkable != bNewTalkable;
	if (bTalkableChanged)
	{
		const bool bOldTalkable = bIsTalkable;
		bIsTalkable = bNewTalkable;
		OnRep_IsTalkable(bOldTalkable);
	}

	if (bCharacterTagsChanged || bTalkableChanged)
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

void UParleySpeakerComponent::OnRep_TalkableCharacterTags(FGameplayTagContainer OldTalkableCharacterTags)
{
	if (AreCharacterTagContainersEquivalent(OldTalkableCharacterTags, TalkableCharacterTags))
	{
		return;
	}

	// Always broadcast on per-character changes so listeners refresh per-character indicators even
	// if bIsTalkable replication is delayed or unchanged.
	OnSpeakerTalkableStateChanged.Broadcast(TalkableCharacterTags.Num() > 0);
}

bool UParleySpeakerComponent::IsTalkableForCharacterTag(const FGameplayTag CharacterTag) const
{
	return CharacterTag.IsValid() && TalkableCharacterTags.HasTagExact(CharacterTag);
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

	const FGameplayTag CharacterTag = ControllerInterface->GetCharacterTag();
	return IsTalkableForCharacterTag(CharacterTag);
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
	DOREPLIFETIME(UParleySpeakerComponent, SpeakerTag);
	DOREPLIFETIME(UParleySpeakerComponent, bIsTalkable);
	DOREPLIFETIME(UParleySpeakerComponent, TalkableCharacterTags);
}
