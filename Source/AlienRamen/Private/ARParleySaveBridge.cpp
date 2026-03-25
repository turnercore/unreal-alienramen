#include "ARParleySaveBridge.h"

#include "ARCharacterStateRuntime.h"
#include "ARCharacterSubsystem.h"
#include "ARGameStateBase.h"
#include "ARGameModeBase.h"
#include "ARLog.h"
#include "ARPlayerTypes.h"
#include "ARSaveGame.h"
#include "ARSaveSubsystem.h"
#include "ParleyDialogueSubsystem.h"
#include "ParleyFactionSubsystem.h"

namespace
{
	static FGameplayTag GetRequesterPlaceholderSpeakerTag()
	{
		return FGameplayTag::RequestGameplayTag(TEXT("Parley.Speaker.Requester"), false);
	}

	static FGameplayTag GetOwnerPlaceholderSpeakerTag()
	{
		return FGameplayTag::RequestGameplayTag(TEXT("Parley.Speaker.Owner"), false);
	}

	static EARCharacterChoice GetCharacterChoiceFromSpeakerTag(const FGameplayTag& SpeakerTag)
	{
		if (!SpeakerTag.IsValid())
		{
			return EARCharacterChoice::None;
		}

		const FGameplayTag BrotherSpeakerTag = ARPlayer::GetBrotherParleySpeakerTag();
		if (BrotherSpeakerTag.IsValid() && SpeakerTag.MatchesTagExact(BrotherSpeakerTag))
		{
			return EARCharacterChoice::Brother;
		}

		const FGameplayTag SisterSpeakerTag = ARPlayer::GetSisterParleySpeakerTag();
		if (SisterSpeakerTag.IsValid() && SpeakerTag.MatchesTagExact(SisterSpeakerTag))
		{
			return EARCharacterChoice::Sister;
		}

		return ARPlayer::GetCharacterChoiceForTag(SpeakerTag);
	}

	static bool IsPlayerCharacterSpeakerTag(const FGameplayTag& SpeakerTag)
	{
		return GetCharacterChoiceFromSpeakerTag(SpeakerTag) != EARCharacterChoice::None
			|| SpeakerTag.MatchesTagExact(GetRequesterPlaceholderSpeakerTag())
			|| SpeakerTag.MatchesTagExact(GetOwnerPlaceholderSpeakerTag());
	}

	static void AddMirrorVariants(const FGameplayTag& SpeakerTag, TArray<FGameplayTag>& OutVariants)
	{
		if (!SpeakerTag.IsValid())
		{
			return;
		}

		OutVariants.AddUnique(SpeakerTag);
		const EARCharacterChoice Choice = GetCharacterChoiceFromSpeakerTag(SpeakerTag);
		if (Choice == EARCharacterChoice::Brother)
		{
			OutVariants.AddUnique(ARPlayer::GetSisterParleySpeakerTag());
		}
		else if (Choice == EARCharacterChoice::Sister)
		{
			OutVariants.AddUnique(ARPlayer::GetBrotherParleySpeakerTag());
		}
		else if (SpeakerTag.MatchesTagExact(GetRequesterPlaceholderSpeakerTag())
			|| SpeakerTag.MatchesTagExact(GetOwnerPlaceholderSpeakerTag()))
		{
			OutVariants.AddUnique(ARPlayer::GetBrotherParleySpeakerTag());
			OutVariants.AddUnique(ARPlayer::GetSisterParleySpeakerTag());
		}
	}

	static bool UpsertSpeakerRelationshipState(UARSaveGame* SaveGame, const FGameplayTag SourceSpeakerTag, const FGameplayTag TargetSpeakerTag, const float NewTotal)
	{
		if (!SaveGame || !SourceSpeakerTag.IsValid() || !TargetSpeakerTag.IsValid())
		{
			return false;
		}

		for (FDialogueSpeakerRelationshipState& State : SaveGame->DialogueSpeakerRelationshipStates)
		{
			if (State.SourceSpeakerTag.MatchesTagExact(SourceSpeakerTag)
				&& State.TargetSpeakerTag.MatchesTagExact(TargetSpeakerTag))
			{
				if (!FMath::IsNearlyEqual(State.RelationshipPoints, NewTotal))
				{
					State.RelationshipPoints = NewTotal;
					return true;
				}
				return false;
			}
		}

		FDialogueSpeakerRelationshipState& Added = SaveGame->DialogueSpeakerRelationshipStates.AddDefaulted_GetRef();
		Added.SourceSpeakerTag = SourceSpeakerTag;
		Added.TargetSpeakerTag = TargetSpeakerTag;
		Added.RelationshipPoints = NewTotal;
		return true;
	}
}

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
		ParleySubsystem->OnSpeakerRelationshipChanged.AddDynamic(this, &UARParleySaveBridge::HandleSpeakerRelationshipChanged);
		ParleySubsystem->OnProgressionTagMutated.AddDynamic(this, &UARParleySaveBridge::HandleProgressionTagMutated);
		ParleySubsystem->OnQueryConversationCompleted.BindUObject(this, &UARParleySaveBridge::IsConversationCompletedForCharacter);
		ParleySubsystem->OnQueryCurrentModeTag.BindUObject(this, &UARParleySaveBridge::ResolveCurrentModeTag);
	}

	if (FactionSubsystem)
	{
		FactionSubsystem->OnFactionPopularityChanged.AddDynamic(this, &UARParleySaveBridge::HandleFactionPopularityChanged);
		FactionSubsystem->OnFactionSpeakerReputationChanged.AddDynamic(this, &UARParleySaveBridge::HandleFactionSpeakerReputationChanged);
	}

	UE_LOG(
		ARLog,
		Log,
		TEXT("[ParleyBridge] Initialize SaveSubsystem=%s ParleySubsystem=%s FactionSubsystem=%s RuntimeWorld=%s"),
		*GetNameSafe(SaveSubsystem),
		*GetNameSafe(ParleySubsystem),
		*GetNameSafe(FactionSubsystem),
		*GetNameSafe(ResolveRuntimeWorld()));

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
		ParleySubsystem->OnSpeakerRelationshipChanged.RemoveDynamic(this, &UARParleySaveBridge::HandleSpeakerRelationshipChanged);
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

void UARParleySaveBridge::HandleConversationCompleted(FGameplayTag ConversationTag, FGameplayTag OwnerCharacterTag, FGameplayTag CharacterTag)
{
	UARSaveGame* SaveGame = GetCurrentSave();
	if (!SaveGame || !ConversationTag.IsValid())
	{
		return;
	}

	SaveGame->DialogueCompletedConversationTagsByGame.AddTag(ConversationTag);

	const FGameplayTag EffectiveCharacterTag = ARPlayer::NormalizeCharacterTag(CharacterTag).IsValid()
		? ARPlayer::NormalizeCharacterTag(CharacterTag)
		: ARPlayer::NormalizeCharacterTag(OwnerCharacterTag);
	if (EffectiveCharacterTag.IsValid())
	{
		FARCharacterSaveData& CharacterState = SaveGame->FindOrAddCharacterStateData(EffectiveCharacterTag);
		CharacterState.DialogueState.CompletedConversationTags.AddTag(ConversationTag);
	}

	SaveSubsystem->MarkSaveDirty();
}

void UARParleySaveBridge::HandleSpeakerRelationshipChanged(FGameplayTag SourceSpeakerTag, FGameplayTag TargetSpeakerTag, FGameplayTag OwnerCharacterTag, float Delta, float NewTotal)
{
	(void)OwnerCharacterTag;
	(void)Delta;
	UARSaveGame* SaveGame = GetCurrentSave();
	if (!SaveGame || !SourceSpeakerTag.IsValid() || !TargetSpeakerTag.IsValid())
	{
		return;
	}

	TArray<FGameplayTag> SourceVariants;
	TArray<FGameplayTag> TargetVariants;
	AddMirrorVariants(SourceSpeakerTag, SourceVariants);
	AddMirrorVariants(TargetSpeakerTag, TargetVariants);

	if (!IsPlayerCharacterSpeakerTag(SourceSpeakerTag) && !IsPlayerCharacterSpeakerTag(TargetSpeakerTag))
	{
		SourceVariants.Reset();
		TargetVariants.Reset();
		SourceVariants.Add(SourceSpeakerTag);
		TargetVariants.Add(TargetSpeakerTag);
	}

	bool bChanged = false;
	for (const FGameplayTag SourceVariant : SourceVariants)
	{
		for (const FGameplayTag TargetVariant : TargetVariants)
		{
			// Do not synthesize player self-edges when mirroring an asymmetric player edge (for example Brother->Sister).
			if (!SourceSpeakerTag.MatchesTagExact(TargetSpeakerTag)
				&& SourceVariant.MatchesTagExact(TargetVariant)
				&& IsPlayerCharacterSpeakerTag(SourceVariant))
			{
				continue;
			}

			bChanged |= UpsertSpeakerRelationshipState(SaveGame, SourceVariant, TargetVariant, NewTotal);
		}
	}

	if (bChanged)
	{
		SaveSubsystem->MarkSaveDirty();
	}
}

void UARParleySaveBridge::HandleProgressionTagMutated(FGameplayTag ProgressionTag, bool bAdded, FGameplayTag OwnerCharacterTag)
{
	UARSaveGame* SaveGame = GetCurrentSave();
	if (!SaveGame || !ProgressionTag.IsValid())
	{
		UE_LOG(
			ARLog,
			Warning,
			TEXT("[ParleyBridge] Progression mutation dropped SaveGame=%s Tag=%s Added=%s OwnerCharacter=%s"),
			*GetNameSafe(SaveGame),
			*ProgressionTag.ToString(),
			bAdded ? TEXT("true") : TEXT("false"),
			*OwnerCharacterTag.ToString());
		return;
	}

	const FGameplayTag CharacterTag = ARPlayer::NormalizeCharacterTag(OwnerCharacterTag);
	UE_LOG(
		ARLog,
		Log,
		TEXT("[ParleyBridge] Progression mutation Tag=%s Added=%s OwnerCharacter=%s NormalizedCharacter=%s"),
		*ProgressionTag.ToString(),
		bAdded ? TEXT("true") : TEXT("false"),
		*OwnerCharacterTag.ToString(),
		*CharacterTag.ToString());
	if (!CharacterTag.IsValid())
	{
		const bool bSaveChanged = bAdded
			? !SaveGame->GameProgressionTags.HasTagExact(ProgressionTag)
			: SaveGame->GameProgressionTags.HasTagExact(ProgressionTag);
		if (bAdded)
		{
			SaveGame->GameProgressionTags.AddTag(ProgressionTag);
		}
		else
		{
			SaveGame->GameProgressionTags.RemoveTag(ProgressionTag);
		}

		UWorld* const RuntimeWorld = ResolveRuntimeWorld();
		if (!RuntimeWorld)
		{
			UE_LOG(
				ARLog,
				Warning,
				TEXT("[ParleyBridge] Game progression mutation Tag=%s Added=%s updated save only because no runtime world was available."),
				*ProgressionTag.ToString(),
				bAdded ? TEXT("true") : TEXT("false"));
		}
		else if (AARGameStateBase* GameState = RuntimeWorld->GetGameState<AARGameStateBase>())
		{
			const bool bRuntimeChanged = bAdded
				? GameState->AddGameProgressionTag(ProgressionTag)
				: GameState->RemoveGameProgressionTag(ProgressionTag);
			UE_LOG(
				ARLog,
				Log,
				TEXT("[ParleyBridge] Game progression mutation Tag=%s Added=%s SaveChanged=%s GameState=%s RuntimeChanged=%s UnlockMirrorNow=%s"),
				*ProgressionTag.ToString(),
				bAdded ? TEXT("true") : TEXT("false"),
				bSaveChanged ? TEXT("true") : TEXT("false"),
				*GetNameSafe(GameState),
				bRuntimeChanged ? TEXT("true") : TEXT("false"),
				GameState->HasUnlockTag(ProgressionTag) ? TEXT("true") : TEXT("false"));
		}
		else
		{
			UE_LOG(
				ARLog,
				Warning,
				TEXT("[ParleyBridge] Game progression mutation Tag=%s Added=%s updated save only because no AARGameStateBase was available in World=%s."),
				*ProgressionTag.ToString(),
				bAdded ? TEXT("true") : TEXT("false"),
				*GetNameSafe(RuntimeWorld));
		}

		SaveSubsystem->MarkSaveDirty();
		return;
	}

	FARCharacterSaveData& CharacterState = SaveGame->FindOrAddCharacterStateData(CharacterTag);
	const bool bSaveChanged = bAdded
		? !CharacterState.CharacterProgressionTags.HasTagExact(ProgressionTag)
		: CharacterState.CharacterProgressionTags.HasTagExact(ProgressionTag);
	if (bAdded)
	{
		CharacterState.CharacterProgressionTags.AddTag(ProgressionTag);
	}
	else
	{
		CharacterState.CharacterProgressionTags.RemoveTag(ProgressionTag);
	}

	UWorld* const RuntimeWorld = ResolveRuntimeWorld();
	if (!RuntimeWorld)
	{
		UE_LOG(
			ARLog,
			Warning,
			TEXT("[ParleyBridge] Character progression mutation Tag=%s Added=%s Character=%s updated save only because no runtime world was available."),
			*ProgressionTag.ToString(),
			bAdded ? TEXT("true") : TEXT("false"),
			*CharacterTag.ToString());
	}
	else
	{
		bool bAppliedToRuntime = false;
		if (UARCharacterSubsystem* CharacterSubsystem = RuntimeWorld->GetSubsystem<UARCharacterSubsystem>())
		{
			TArray<AARCharacterStateRuntime*> RegisteredRuntimes;
			CharacterSubsystem->GetRegisteredRuntimes(RegisteredRuntimes);
			for (AARCharacterStateRuntime* Runtime : RegisteredRuntimes)
			{
				if (!Runtime || !ARPlayer::NormalizeCharacterTag(Runtime->GetCharacterTag()).MatchesTagExact(CharacterTag))
				{
					continue;
				}

				if (bAdded)
				{
					Runtime->AddCharacterProgressionTag(ProgressionTag);
				}
				else
				{
					Runtime->RemoveCharacterProgressionTag(ProgressionTag);
				}
				bAppliedToRuntime = true;
				break;
			}
		}

		UE_LOG(
			ARLog,
			Log,
			TEXT("[ParleyBridge] Character progression mutation Tag=%s Added=%s Character=%s SaveChanged=%s RuntimeApplied=%s World=%s"),
			*ProgressionTag.ToString(),
			bAdded ? TEXT("true") : TEXT("false"),
			*CharacterTag.ToString(),
			bSaveChanged ? TEXT("true") : TEXT("false"),
			bAppliedToRuntime ? TEXT("true") : TEXT("false"),
			*GetNameSafe(RuntimeWorld));
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

	for (FParleyFactionRuntimeState& State : SaveGame->FactionPopularityStates)
	{
		if (State.FactionTag.MatchesTagExact(FactionTag))
		{
			State.Popularity = NewTotal;
			SaveSubsystem->MarkSaveDirty();
			return;
		}
	}

	FParleyFactionRuntimeState& Added = SaveGame->FactionPopularityStates.AddDefaulted_GetRef();
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

bool UARParleySaveBridge::IsConversationCompletedForCharacter(FGameplayTag ConversationTag, FGameplayTag CharacterTag) const
{
	const UARSaveGame* SaveGame = GetCurrentSave();
	if (!SaveGame || !ConversationTag.IsValid())
	{
		return false;
	}

	const FGameplayTag NormalizedCharacterTag = ARPlayer::NormalizeCharacterTag(CharacterTag);
	if (!NormalizedCharacterTag.IsValid())
	{
		return false;
	}

	FARCharacterSaveData CharacterState;
	int32 CharacterIndex = INDEX_NONE;
	if (!SaveGame->FindCharacterStateDataByTag(NormalizedCharacterTag, CharacterState, CharacterIndex))
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

	ParleySubsystem->SetGameProgressionTags(SaveGame->GameProgressionTags);
	ParleySubsystem->SetCompletedConversationTagsByGame(SaveGame->DialogueCompletedConversationTagsByGame);
	ParleySubsystem->SetSpeakerRelationshipStates(SaveGame->DialogueSpeakerRelationshipStates);

	for (const FGameplayTag CharacterTag : { ARPlayer::GetBrotherCharacterTag(), ARPlayer::GetSisterCharacterTag() })
	{
		FParleyProgressionState State;
		State.CharacterTag = CharacterTag;

		if (State.CharacterTag.IsValid())
		{
			FARCharacterSaveData CharacterState;
			int32 CharacterIndex = INDEX_NONE;
			if (SaveGame->FindCharacterStateDataByTag(State.CharacterTag, CharacterState, CharacterIndex))
			{
				State.ProgressionTags = CharacterState.CharacterProgressionTags;
				State.CompletedConversationTags = CharacterState.DialogueState.CompletedConversationTags;
				State.CompletedChoiceRecords = CharacterState.DialogueState.CompletedChoiceRecords;
				State.SeenConversationTagsThisCycle = CharacterState.DialogueState.SeenConversationTagsThisCycle;
				State.SkippedConversationTagsThisCycle = CharacterState.DialogueState.SkippedConversationTagsThisCycle;
				State.SpeakerOfferCountsThisCycle = CharacterState.DialogueState.SpeakerOfferCountsThisCycle;
				State.LastOfferedConversationBySpeakerThisCycle = CharacterState.DialogueState.LastOfferedConversationBySpeakerThisCycle;
			}
		}

		ParleySubsystem->SetProgressionStateForCharacter(State.CharacterTag, State);
	}

	FactionSubsystem->SetProgressionTags(SaveGame->GameProgressionTags);

	TArray<FParleyFactionState> FactionStates;
	FactionStates.Reserve(SaveGame->FactionPopularityStates.Num());
	for (const FParleyFactionRuntimeState& SavedState : SaveGame->FactionPopularityStates)
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

UWorld* UARParleySaveBridge::ResolveRuntimeWorld() const
{
	if (SaveSubsystem)
	{
		if (UWorld* const SaveWorld = SaveSubsystem->GetWorld())
		{
			return SaveWorld;
		}
	}

	if (ParleySubsystem)
	{
		if (UWorld* const DialogueWorld = ParleySubsystem->GetWorld())
		{
			return DialogueWorld;
		}
	}

	if (FactionSubsystem)
	{
		if (UWorld* const FactionWorld = FactionSubsystem->GetWorld())
		{
			return FactionWorld;
		}
	}

	return GetWorld();
}
