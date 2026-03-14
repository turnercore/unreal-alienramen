#include "ARParleySaveBridge.h"

#include "ARGameModeBase.h"
#include "ARLog.h"
#include "ARPlayerTypes.h"
#include "ARSaveGame.h"
#include "ARSaveSubsystem.h"
#include "ParleyDialogueSubsystem.h"
#include "ParleyFactionSubsystem.h"
#include "ParleyPlayerSlotHelpers.h"

void UARParleySaveBridge::Initialize(UARSaveSubsystem* InSaveSubsystem, UParleyDialogueSubsystem* InParleySubsystem, UParleyFactionSubsystem* InFactionSubsystem)
{
	Shutdown();

	SaveSubsystem = InSaveSubsystem;
	ParleySubsystem = InParleySubsystem;
	FactionSubsystem = InFactionSubsystem;

	if (SaveSubsystem)
	{
		SaveSubsystem->OnGameLoaded.AddDynamic(this, &UARParleySaveBridge::HandleSaveLoaded);
	}

	if (ParleySubsystem)
	{
		ParleySubsystem->OnParleyConversationCompleted.AddDynamic(this, &UARParleySaveBridge::HandleConversationCompleted);
		ParleySubsystem->OnRelationshipChanged.AddDynamic(this, &UARParleySaveBridge::HandleRelationshipChanged);
		ParleySubsystem->OnProgressionTagMutated.AddDynamic(this, &UARParleySaveBridge::HandleProgressionTagMutated);
		ParleySubsystem->OnQueryConversationCompleted.BindUObject(this, &UARParleySaveBridge::IsConversationCompletedForPlayer);
		ParleySubsystem->OnQueryCurrentModeTag.BindUObject(this, &UARParleySaveBridge::ResolveCurrentModeTag);
	}

	if (FactionSubsystem)
	{
		FactionSubsystem->OnFactionPopularityChanged.AddDynamic(this, &UARParleySaveBridge::HandleFactionPopularityChanged);
		FactionSubsystem->OnFactionSpeakerReputationChanged.AddDynamic(this, &UARParleySaveBridge::HandleFactionSpeakerReputationChanged);
	}

	InjectAllFromCurrentSave();
}

void UARParleySaveBridge::Shutdown()
{
	if (SaveSubsystem)
	{
		SaveSubsystem->OnGameLoaded.RemoveDynamic(this, &UARParleySaveBridge::HandleSaveLoaded);
	}

	if (ParleySubsystem)
	{
		ParleySubsystem->OnParleyConversationCompleted.RemoveDynamic(this, &UARParleySaveBridge::HandleConversationCompleted);
		ParleySubsystem->OnRelationshipChanged.RemoveDynamic(this, &UARParleySaveBridge::HandleRelationshipChanged);
		ParleySubsystem->OnProgressionTagMutated.RemoveDynamic(this, &UARParleySaveBridge::HandleProgressionTagMutated);
		ParleySubsystem->OnQueryConversationCompleted.Unbind();
		ParleySubsystem->OnQueryCurrentModeTag.Unbind();
	}

	if (FactionSubsystem)
	{
		FactionSubsystem->OnFactionPopularityChanged.RemoveDynamic(this, &UARParleySaveBridge::HandleFactionPopularityChanged);
		FactionSubsystem->OnFactionSpeakerReputationChanged.RemoveDynamic(this, &UARParleySaveBridge::HandleFactionSpeakerReputationChanged);
	}
}

void UARParleySaveBridge::HandleSaveLoaded()
{
	InjectAllFromCurrentSave();
}

void UARParleySaveBridge::HandleConversationCompleted(FGameplayTag ConversationTag, FGameplayTag PlayerSlotTag, FGameplayTag CharacterTag)
{
	UARSaveGame* SaveGame = GetCurrentSave();
	if (!SaveGame || !ConversationTag.IsValid())
	{
		return;
	}

	SaveGame->DialogueCompletedConversationTagsByGame.AddTag(ConversationTag);

	const EARPlayerSlot PlayerSlot = ARPlayer::GetPlayerSlotForTag(PlayerSlotTag);
	const FGameplayTag EffectiveCharacterTag = CharacterTag.IsValid()
		? CharacterTag
		: ResolveCharacterTagForSlot(SaveGame, PlayerSlot);
	if (EffectiveCharacterTag.IsValid())
	{
		FARCharacterSaveData& CharacterState = SaveGame->FindOrAddCharacterStateData(EffectiveCharacterTag);
		CharacterState.DialogueState.CompletedConversationTags.AddTag(ConversationTag);
	}

	SaveSubsystem->MarkSaveDirty();
}

void UARParleySaveBridge::HandleRelationshipChanged(FGameplayTag SpeakerTag, FGameplayTag PlayerSlotTag, float Delta, float NewTotal)
{
	(void)PlayerSlotTag;
	(void)Delta;
	UARSaveGame* SaveGame = GetCurrentSave();
	if (!SaveGame || !SpeakerTag.IsValid())
	{
		return;
	}

	for (FDialogueRelationshipState& State : SaveGame->DialogueRelationshipStates)
	{
		if (State.SpeakerTag.MatchesTagExact(SpeakerTag))
		{
			State.RelationshipPoints = NewTotal;
			SaveSubsystem->MarkSaveDirty();
			return;
		}
	}

	FDialogueRelationshipState& Added = SaveGame->DialogueRelationshipStates.AddDefaulted_GetRef();
	Added.SpeakerTag = SpeakerTag;
	Added.RelationshipPoints = NewTotal;
	SaveSubsystem->MarkSaveDirty();
}

void UARParleySaveBridge::HandleProgressionTagMutated(FGameplayTag ProgressionTag, bool bAdded, FGameplayTag PlayerSlotTag)
{
	UARSaveGame* SaveGame = GetCurrentSave();
	if (!SaveGame || !ProgressionTag.IsValid())
	{
		return;
	}

	const EARPlayerSlot PlayerSlot = ARPlayer::GetPlayerSlotForTag(PlayerSlotTag);
	if (PlayerSlot == EARPlayerSlot::Unknown)
	{
		if (bAdded)
		{
			SaveGame->ProgressionTags.AddTag(ProgressionTag);
		}
		else
		{
			SaveGame->ProgressionTags.RemoveTag(ProgressionTag);
		}
		SaveSubsystem->MarkSaveDirty();
		return;
	}

	const FGameplayTag CharacterTag = ResolveCharacterTagForSlot(SaveGame, PlayerSlot);
	if (!CharacterTag.IsValid())
	{
		return;
	}

	FARCharacterSaveData& CharacterState = SaveGame->FindOrAddCharacterStateData(CharacterTag);
	if (bAdded)
	{
		CharacterState.DialogueState.ProgressionTags.AddTag(ProgressionTag);
	}
	else
	{
		CharacterState.DialogueState.ProgressionTags.RemoveTag(ProgressionTag);
	}
	SaveSubsystem->MarkSaveDirty();
}

void UARParleySaveBridge::HandleFactionPopularityChanged(FGameplayTag FactionTag, float Delta, float NewTotal)
{
	(void)Delta;
	UARSaveGame* SaveGame = GetCurrentSave();
	if (!SaveGame || !FactionTag.IsValid())
	{
		return;
	}

	for (FARFactionRuntimeState& State : SaveGame->FactionPopularityStates)
	{
		if (State.FactionTag.MatchesTagExact(FactionTag))
		{
			State.Popularity = NewTotal;
			SaveSubsystem->MarkSaveDirty();
			return;
		}
	}

	FARFactionRuntimeState& Added = SaveGame->FactionPopularityStates.AddDefaulted_GetRef();
	Added.FactionTag = FactionTag;
	Added.Popularity = NewTotal;
	SaveSubsystem->MarkSaveDirty();
}

void UARParleySaveBridge::HandleFactionSpeakerReputationChanged(FGameplayTag FactionTag, FGameplayTag SpeakerTag, float Delta, float NewTotal)
{
	(void)Delta;
	UARSaveGame* SaveGame = GetCurrentSave();
	if (!SaveGame || !FactionTag.IsValid() || !SpeakerTag.IsValid())
	{
		return;
	}

	for (FParleyFactionSpeakerReputationState& State : SaveGame->FactionSpeakerReputationStates)
	{
		if (State.FactionTag.MatchesTagExact(FactionTag) && State.SpeakerTag.MatchesTagExact(SpeakerTag))
		{
			State.Reputation = NewTotal;
			SaveSubsystem->MarkSaveDirty();
			return;
		}
	}

	FParleyFactionSpeakerReputationState& Added = SaveGame->FactionSpeakerReputationStates.AddDefaulted_GetRef();
	Added.FactionTag = FactionTag;
	Added.SpeakerTag = SpeakerTag;
	Added.Reputation = NewTotal;
	SaveSubsystem->MarkSaveDirty();
}

bool UARParleySaveBridge::IsConversationCompletedForPlayer(FGameplayTag ConversationTag, FGameplayTag PlayerSlotTag) const
{
	const UARSaveGame* SaveGame = GetCurrentSave();
	if (!SaveGame || !ConversationTag.IsValid())
	{
		return false;
	}

	const EARPlayerSlot PlayerSlot = ARPlayer::GetPlayerSlotForTag(PlayerSlotTag);
	if (PlayerSlot == EARPlayerSlot::Unknown)
	{
		return false;
	}

	const FGameplayTag CharacterTag = ResolveCharacterTagForSlot(const_cast<UARSaveGame*>(SaveGame), PlayerSlot);
	if (!CharacterTag.IsValid())
	{
		return false;
	}

	FARCharacterSaveData CharacterState;
	int32 CharacterIndex = INDEX_NONE;
	if (!SaveGame->FindCharacterStateDataByTag(CharacterTag, CharacterState, CharacterIndex))
	{
		return false;
	}

	return CharacterState.DialogueState.CompletedConversationTags.HasTagExact(ConversationTag);
}

FGameplayTag UARParleySaveBridge::ResolveCurrentModeTag() const
{
	if (!SaveSubsystem)
	{
		return FGameplayTag();
	}

	if (UWorld* World = SaveSubsystem->GetWorld())
	{
		if (const AARGameModeBase* GameMode = World->GetAuthGameMode<AARGameModeBase>())
		{
			return GameMode->GetModeTag();
		}
	}

	return FGameplayTag();
}

void UARParleySaveBridge::InjectAllFromCurrentSave()
{
	if (!ParleySubsystem || !FactionSubsystem)
	{
		return;
	}

	UARSaveGame* SaveGame = GetCurrentSave();
	if (!SaveGame)
	{
		return;
	}

	ParleySubsystem->SetGameProgressionTags(SaveGame->ProgressionTags);
	ParleySubsystem->SetCompletedConversationTagsByGame(SaveGame->DialogueCompletedConversationTagsByGame);
	ParleySubsystem->SetRelationshipStates(SaveGame->DialogueRelationshipStates);

	for (const EARPlayerSlot Slot : { EARPlayerSlot::P1, EARPlayerSlot::P2 })
	{
		FParleyProgressionState State;
		State.CharacterTag = ResolveCharacterTagForSlot(SaveGame, Slot);

		if (State.CharacterTag.IsValid())
		{
			FARCharacterSaveData CharacterState;
			int32 CharacterIndex = INDEX_NONE;
			if (SaveGame->FindCharacterStateDataByTag(State.CharacterTag, CharacterState, CharacterIndex))
			{
				State.ProgressionTags = CharacterState.DialogueState.ProgressionTags;
				State.CompletedConversationTags = CharacterState.DialogueState.CompletedConversationTags;
				State.CompletedChoiceRecords = CharacterState.DialogueState.CompletedChoiceRecords;
				State.SeenConversationTagsThisCycle = CharacterState.DialogueState.SeenConversationTagsThisCycle;
				State.SkippedConversationTagsThisCycle = CharacterState.DialogueState.SkippedConversationTagsThisCycle;
				State.SpeakerOfferCountsThisCycle = CharacterState.DialogueState.SpeakerOfferCountsThisCycle;
			}
		}

		ParleySubsystem->SetProgressionStateForPlayer(ARPlayer::GetPlayerSlotTag(Slot), State);
	}

	FactionSubsystem->SetProgressionTags(SaveGame->ProgressionTags);

	TArray<FParleyFactionState> FactionStates;
	FactionStates.Reserve(SaveGame->FactionPopularityStates.Num());
	for (const FARFactionRuntimeState& SavedState : SaveGame->FactionPopularityStates)
	{
		if (!SavedState.FactionTag.IsValid())
		{
			continue;
		}

		FParleyFactionState& Added = FactionStates.AddDefaulted_GetRef();
		Added.FactionTag = SavedState.FactionTag;
		Added.Popularity = SavedState.Popularity;
	}

	FactionSubsystem->SetFactionPopularityStates(FactionStates);

	TArray<FParleyFactionSpeakerReputationState> ReputationStates;
	ReputationStates.Reserve(SaveGame->FactionSpeakerReputationStates.Num());
	for (const FParleyFactionSpeakerReputationState& SavedState : SaveGame->FactionSpeakerReputationStates)
	{
		if (!SavedState.FactionTag.IsValid() || !SavedState.SpeakerTag.IsValid())
		{
			continue;
		}

		FParleyFactionSpeakerReputationState& Added = ReputationStates.AddDefaulted_GetRef();
		Added.FactionTag = SavedState.FactionTag;
		Added.SpeakerTag = SavedState.SpeakerTag;
		Added.Reputation = SavedState.Reputation;
	}

	FactionSubsystem->SetFactionSpeakerReputationStates(ReputationStates);
}

UARSaveGame* UARParleySaveBridge::GetCurrentSave() const
{
	return SaveSubsystem ? SaveSubsystem->GetCurrentSaveGame() : nullptr;
}

FGameplayTag UARParleySaveBridge::ResolveCharacterTagForSlot(UARSaveGame* SaveGame, EARPlayerSlot Slot) const
{
	if (!SaveGame || Slot == EARPlayerSlot::Unknown)
	{
		return FGameplayTag();
	}

	FARPlayerStateSaveData PlayerData;
	int32 PlayerIndex = INDEX_NONE;
	if (SaveGame->FindPlayerStateDataBySlot(Slot, PlayerData, PlayerIndex))
	{
		return PlayerData.ResolveCurrentCharacterTag();
	}

	return ARPlayer::GetDefaultCharacterTagForSlot(Slot);
}
