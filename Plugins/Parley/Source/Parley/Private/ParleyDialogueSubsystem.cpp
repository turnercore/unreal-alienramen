#include "ParleyDialogueSubsystem.h"

#include "ParleyConversationAsset.h"
#include "ParleyDialogueSettings.h"
#include "ParleyFactionSubsystem.h"
#include "ParleyLog.h"
#include "ParleyPlayerControllerInterface.h"
#include "ParleySpeakerComponent.h"
#include "ParleySpeakerSubsystem.h"
#include "TagKeySubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "GameplayTagsManager.h"
#include "Engine/Texture2D.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

namespace
{
	static const TCHAR* LexToStringParleySlot(const FGameplayTag CharacterTag)
	{
		if (!CharacterTag.IsValid())
		{
			return TEXT("None");
		}

		return TEXT("Character");
	}

	static FGameplayTag NormalizeCharacterTagForDialogue(const FGameplayTag CharacterTag)
	{
		return CharacterTag;
	}

	static FGameplayTag GetDefaultCharacterTagForSlot(const FGameplayTag CharacterTag)
	{
		return CharacterTag;
	}

	struct FParleyPlayerIdentity
	{
		int32 LegacyId = 0;
		FText DisplayName;
		FGameplayTag PlayerCharacterTag = FGameplayTag();
		FString UniqueNetIdString;
		FString UniqueNetIdType;
	};

	struct FParleyPlayerStateProgressionData
	{
		FGameplayTag PlayerCharacterTag = FGameplayTag();
		FGameplayTag CurrentCharacterTag;

		FGameplayTag ResolveCurrentCharacterTag() const
		{
			return CurrentCharacterTag.IsValid()
				? CurrentCharacterTag
				: GetDefaultCharacterTagForSlot(PlayerCharacterTag);
		}
	};

	struct FParleyCharacterProgressionData
	{
		FGameplayTag CharacterTag;
		FDialoguePlayerPersistentState DialogueState;
	};

	struct FParleyProgressionStore
	{
		FGameplayTagContainer ProgressionTags;
		FGameplayTagContainer DialogueCompletedConversationTagsByGame;
		TArray<FDialogueSpeakerRelationshipState> DialogueSpeakerRelationshipStates;
		TArray<FParleyCharacterProgressionData> CharacterStates;
		TMap<FGameplayTag, FGameplayTag> CharacterTagByIdentity;

		bool FindPlayerStateDataByIdentity(const FParleyPlayerIdentity& Identity, FParleyPlayerStateProgressionData& OutData, int32& OutIndex) const
		{
			return FindPlayerStateDataByCharacterTag(Identity.PlayerCharacterTag, OutData, OutIndex);
		}

		bool FindPlayerStateDataByCharacterTag(const FGameplayTag Slot, FParleyPlayerStateProgressionData& OutData, int32& OutIndex) const
		{
			OutIndex = INDEX_NONE;
			if (!Slot.IsValid())
			{
				return false;
			}

			const FGameplayTag* CharacterTag = CharacterTagByIdentity.Find(Slot);
			if (!CharacterTag)
			{
				return false;
			}

			OutData.PlayerCharacterTag = Slot;
			OutData.CurrentCharacterTag = *CharacterTag;
			OutIndex = 0;
			return true;
		}

		bool FindCharacterStateDataByTag(const FGameplayTag& CharacterTag, FParleyCharacterProgressionData& OutData, int32& OutIndex) const
		{
			OutIndex = INDEX_NONE;
			if (!CharacterTag.IsValid())
			{
				return false;
			}

			for (int32 Index = 0; Index < CharacterStates.Num(); ++Index)
			{
				if (CharacterStates[Index].CharacterTag.MatchesTagExact(CharacterTag))
				{
					OutData = CharacterStates[Index];
					OutIndex = Index;
					return true;
				}
			}

			return false;
		}

		FParleyCharacterProgressionData* FindCharacterStateDataMutable(const FGameplayTag& CharacterTag, int32& OutIndex)
		{
			OutIndex = INDEX_NONE;
			if (!CharacterTag.IsValid())
			{
				return nullptr;
			}

			for (int32 Index = 0; Index < CharacterStates.Num(); ++Index)
			{
				if (CharacterStates[Index].CharacterTag.MatchesTagExact(CharacterTag))
				{
					OutIndex = Index;
					return &CharacterStates[Index];
				}
			}

			return nullptr;
		}

		FParleyCharacterProgressionData& FindOrAddCharacterStateData(const FGameplayTag& CharacterTag)
		{
			int32 CharacterIndex = INDEX_NONE;
			if (FParleyCharacterProgressionData* Existing = FindCharacterStateDataMutable(CharacterTag, CharacterIndex))
			{
				return *Existing;
			}

			FParleyCharacterProgressionData& Added = CharacterStates.AddDefaulted_GetRef();
			Added.CharacterTag = CharacterTag;
			return Added;
		}
	};

	struct FParleyProgressionMutator
	{
		UParleyDialogueSubsystem* Owner = nullptr;
		FParleyProgressionStore* ProgressionStore = nullptr;

		FParleyProgressionStore* GetCurrentProgressionStore() const
		{
			return ProgressionStore;
		}

		void MarkStateDirty() const
		{
			if (Owner)
			{
				Owner->OnProgressionStateMarkedDirty.Broadcast();
			}
		}

		bool AddProgressionTag(const FGameplayTag& ProgressionTag)
		{
			if (!ProgressionStore || !ProgressionTag.IsValid())
			{
				return false;
			}

			const bool bAlreadyHadTag = ProgressionStore->ProgressionTags.HasTagExact(ProgressionTag);
			ProgressionStore->ProgressionTags.AddTag(ProgressionTag);
			return !bAlreadyHadTag;
		}

		bool RemoveProgressionTag(const FGameplayTag& ProgressionTag)
		{
			if (!ProgressionStore || !ProgressionTag.IsValid())
			{
				return false;
			}

			if (!ProgressionStore->ProgressionTags.HasTagExact(ProgressionTag))
			{
				return false;
			}

			ProgressionStore->ProgressionTags.RemoveTag(ProgressionTag);
			return true;
		}
	};

	struct FDialogueCandidateEval
	{
		TObjectPtr<UParleyConversationAsset> Conversation = nullptr;
		int32 Priority = 0;
		int32 EffectivePriority = 0;
		int32 OfferWeight = 1;
		float ChanceOffered = 1.0f;
		bool bSeenByGame = false;
		bool bSeenByPlayer = false;
		bool bCompletedByGame = false;
		bool bCompletedByPlayer = false;
		bool bRepeatable = false;
		bool bSeenThisCycle = false;
		bool bSkippedThisCycle = false;
	};

	static bool SortCandidatesByPriority(const FDialogueCandidateEval& Lhs, const FDialogueCandidateEval& Rhs)
	{
		return Lhs.EffectivePriority > Rhs.EffectivePriority;
	}

	struct FParleyActiveDialogueSession
	{
		FString SessionId;
		FGameplayTag ConversationTag;
		FGameplayTag PrimarySpeakerTag;
		FGameplayTag SourceSpeakerTag;
		TObjectPtr<UParleyConversationAsset> ConversationAsset = nullptr;
		FGuid CurrentNodeId;
		FGuid WaitingChoiceNodeId;
		FGameplayTag InitiatorCharacterTag = FGameplayTag();
		FGameplayTag OwnerCharacterTag = FGameplayTag();
		bool bIsSharedSession = false;
		bool bConversationImportant = false;
		bool bConversationPrivate = false;
		bool bWaitingForChoice = false;
		bool bChoiceRequiresAllViewers = false;
		bool bWaitingForAdvanceInput = false;
		TArray<FDialogueChoiceView> CurrentChoices;
		FGameplayTag CurrentSpeakerTag;
		FName CurrentSpeakerLineFontStyleTag;
		TSoftObjectPtr<UFont> CurrentSpeakerLineFont;
		FText CurrentLineText;
		FSpeakerPortraitData CurrentSpeakerPortrait;
		FGuid WaitingLineNodeId;
		int32 WaitingMultiLineEntryIndex = INDEX_NONE;
		TMap<FGuid, FGuid> RuntimeChoiceSelections;
		FGameplayTagContainer TransientConversationTags;
		TArray<FGuid> PendingSequenceBranchNodeIds;
		TMap<FGuid, int32> PendingMultiLineStartIndexByNode;
		TSet<FGameplayTag> Participants;
		TSet<TWeakObjectPtr<UParleySpeakerComponent>> SpeakerComponentsWithEmotionOverride;
		FTimerHandle AutoAdvanceTimerHandle;
	};

	static bool IsAuthorityWorld_Dialogue(const UWorld* World)
	{
		if (!World)
		{
			return false;
		}

		return World->GetNetMode() == NM_Standalone || World->GetAuthGameMode() != nullptr;
	}

	static APlayerState* GetPlayerStateFromController(const APlayerController* PC)
	{
		return PC ? PC->GetPlayerState<APlayerState>() : nullptr;
	}

	static FGameplayTag ReadGameplayTagProperty(const UObject* Object, const FName PropertyName)
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

	static bool ReadBoolProperty(const UObject* Object, const FName PropertyName, bool& OutValue)
	{
		if (!Object)
		{
			return false;
		}

		const FBoolProperty* Property = FindFProperty<FBoolProperty>(Object->GetClass(), PropertyName);
		if (!Property)
		{
			return false;
		}

		OutValue = Property->GetPropertyValue_InContainer(Object);
		return true;
	}

	static FGameplayTag GetCharacterTagFromPlayerState(const APlayerState* PlayerState)
	{
		if (!PlayerState)
		{
			return FGameplayTag();
		}

		if (const APlayerController* OwnerController = Cast<APlayerController>(PlayerState->GetOwner()))
		{
			if (const APawn* Pawn = OwnerController->GetPawn())
			{
				if (const UParleySpeakerComponent* SpeakerComponent = Pawn->FindComponentByClass<UParleySpeakerComponent>())
				{
					const FGameplayTag PawnSpeakerTag = SpeakerComponent->GetSpeakerTag();
					if (PawnSpeakerTag.IsValid())
					{
						return PawnSpeakerTag;
					}
				}
			}
		}

		return ReadGameplayTagProperty(PlayerState, TEXT("CurrentCharacterTag"));
	}

	static FGameplayTag GetCurrentCharacterTagFromPlayerState(const APlayerState* PlayerState)
	{
		return GetCharacterTagFromPlayerState(PlayerState);
	}

	static FText GetPlayerDisplayNameFromPlayerState(const APlayerState* PlayerState)
	{
		if (!PlayerState)
		{
			return FText();
		}

		if (const FStrProperty* DisplayNameProperty = FindFProperty<FStrProperty>(PlayerState->GetClass(), TEXT("DisplayName")))
		{
			const FString DisplayName = DisplayNameProperty->GetPropertyValue_InContainer(PlayerState);
			if (!DisplayName.IsEmpty())
			{
				return FText::FromString(DisplayName);
			}
		}

		return FText::FromString(PlayerState->GetPlayerName());
	}

	static void GetLoadoutTagsFromPlayerState(const APlayerState* PlayerState, FGameplayTagContainer& OutTags)
	{
		OutTags.Reset();
		if (!PlayerState)
		{
			return;
		}

		const FStructProperty* LoadoutProperty = FindFProperty<FStructProperty>(PlayerState->GetClass(), TEXT("LoadoutTags"));
		if (!LoadoutProperty || LoadoutProperty->Struct != TBaseStructure<FGameplayTagContainer>::Get())
		{
			return;
		}

		OutTags = *LoadoutProperty->ContainerPtrToValuePtr<FGameplayTagContainer>(PlayerState);
	}

	static bool IsDialogueAutoAdvanceEnabledForPlayerState(const APlayerState* PlayerState)
	{
		bool bAutoAdvance = false;
		return ReadBoolProperty(PlayerState, TEXT("bDialogueAutoAdvanceEnabled"), bAutoAdvance) ? bAutoAdvance : false;
	}

	static FGameplayTag GetCharacterTagFromController(const APlayerController* PC)
	{
		if (!PC)
		{
			return FGameplayTag();
		}

		if (const IParleyPlayerControllerInterface* ControllerInterface = Cast<IParleyPlayerControllerInterface>(const_cast<APlayerController*>(PC)))
		{
			const FGameplayTag InterfaceSlot = ControllerInterface->GetCharacterTag();
			if (InterfaceSlot.IsValid())
			{
				return InterfaceSlot;
			}
		}

		return GetCharacterTagFromPlayerState(GetPlayerStateFromController(PC));
	}

	static FString BuildSessionId()
	{
		return FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensInBraces);
	}

	static bool IsModeInContainer(const FGameplayTag& ModeTag, const FGameplayTagContainer& ModeSet)
	{
		if (!ModeTag.IsValid())
		{
			return false;
		}

		for (const FGameplayTag Allowed : ModeSet)
		{
			if (ModeTag.MatchesTagExact(Allowed))
			{
				return true;
			}
		}

		return false;
	}

	static FGameplayTag GetCurrentModeTag(const UParleyDialogueSubsystem* Subsystem, const UWorld* World)
	{
		if (Subsystem && Subsystem->OnQueryCurrentModeTag.IsBound())
		{
			return Subsystem->OnQueryCurrentModeTag.Execute();
		}

		(void)World;
		return FGameplayTag();
	}

	static FGameplayTag BuildTagFromRootAndLeaf(const FGameplayTag& RootTag, const FName LeafRowName)
	{
		if (!RootTag.IsValid() || LeafRowName.IsNone())
		{
			return FGameplayTag();
		}
		const FString Path = FString::Printf(TEXT("%s.%s"), *RootTag.ToString(), *LeafRowName.ToString());
		return UGameplayTagsManager::Get().RequestGameplayTag(FName(*Path), false);
	}

	static int32 ResolveRelationshipLevelFromThresholds(const float Points, const TArray<float>& Thresholds)
	{
		int32 Level = 0;
		for (const float Threshold : Thresholds)
		{
			if (Points >= Threshold)
			{
				++Level;
			}
		}
		return Level;
	}

	static bool IsLineNodeData(const FInstancedStruct& NodeData)
	{
		return NodeData.GetScriptStruct() == FDialogueLineNodeData::StaticStruct();
	}
}

struct UParleyDialogueSubsystem::FParleyDialogueRuntimeState
{
	TMap<FGameplayTag, TObjectPtr<UParleyConversationAsset>> ConversationsByTag;
	TMap<FGameplayTag, FParleySpeakerRow> SpeakerRowsByTag;
	TArray<FParleyActiveDialogueSession> ActiveSessions;
	TMap<FGameplayTag, FGameplayTagContainer> SeenByPlayerTransient;
	TMap<FGameplayTag, FGameplayTagContainer> SkippedByPlayerTransient;
	TMap<FGameplayTag, TMap<FGameplayTag, int32>> SpeakerOfferCountsByPlayerTransient;
	FGameplayTagContainer SeenByGameTransient;
	TMap<FGameplayTag, FGameplayTag> EavesdropTargetByViewer;
	FParleyProgressionStore ProgressionStoreState;
	FParleyProgressionMutator ProgressionMutator;
};

void UParleyDialogueSubsystem::FParleyDialogueRuntimeStateDeleter::operator()(FParleyDialogueRuntimeState* Ptr) const
{
	delete Ptr;
}

static bool AddConversationToRuntimeRegistry(
	TMap<FGameplayTag, TObjectPtr<UParleyConversationAsset>>& ConversationsByTag,
	UParleyConversationAsset* Conversation,
	const FGameplayTag& ForcedConversationTag,
	const FString& SourceLabel)
{
	if (!Conversation)
	{
		UE_LOG(ParleyLog, Warning, TEXT("[Dialogue] Skipping null conversation from %s."), *SourceLabel);
		return false;
	}

	if (ForcedConversationTag.IsValid() && !Conversation->Header.ConversationTag.IsValid())
	{
		Conversation->Header.ConversationTag = ForcedConversationTag;
	}

	const FGameplayTag ConversationTag = Conversation->Header.ConversationTag;
	if (ForcedConversationTag.IsValid() && ConversationTag.IsValid() && !ConversationTag.MatchesTagExact(ForcedConversationTag))
	{
		UE_LOG(ParleyLog, Warning,
			TEXT("[Dialogue] Conversation '%s' tag mismatch: asset '%s' vs lookup '%s' (%s). Asset tag will be used."),
			*GetNameSafe(Conversation),
			*ConversationTag.ToString(),
			*ForcedConversationTag.ToString(),
			*SourceLabel);
	}

	if (!ConversationTag.IsValid())
	{
		UE_LOG(ParleyLog, Warning, TEXT("[Dialogue] Skipping conversation '%s' from %s: ConversationTag is invalid."),
			*GetNameSafe(Conversation),
			*SourceLabel);
		return false;
	}

	if (ConversationsByTag.Contains(ConversationTag))
	{
		UE_LOG(ParleyLog, Error, TEXT("[Dialogue] Duplicate ConversationTag '%s' from %s. Existing asset '%s' kept; duplicate '%s' ignored."),
			*ConversationTag.ToString(),
			*SourceLabel,
			*GetNameSafe(ConversationsByTag[ConversationTag]),
			*GetNameSafe(Conversation));
		return false;
	}

	if (!Conversation->Header.PrimarySpeakerTag.IsValid())
	{
		UE_LOG(ParleyLog, Warning,
			TEXT("[Dialogue] Conversation '%s' from %s has no PrimarySpeakerTag; it will still be registered but will fail offer gating."),
			*ConversationTag.ToString(),
			*SourceLabel);
	}

	ConversationsByTag.Add(ConversationTag, Conversation);
	UE_LOG(ParleyLog, Verbose,
		TEXT("[Dialogue] Registered conversation '%s' (PrimarySpeaker:%s, Priority:%d) from %s."),
		*ConversationTag.ToString(),
		*Conversation->Header.PrimarySpeakerTag.ToString(),
		Conversation->Header.Priority,
		*SourceLabel);
	return true;
}

UParleyDialogueSubsystem::FParleyDialogueRuntimeState& UParleyDialogueSubsystem::GetRuntimeState()
{
	if (!RuntimeState.IsValid())
	{
		RuntimeState.Reset(new FParleyDialogueRuntimeState());
		RuntimeState->ProgressionMutator.Owner = this;
		RuntimeState->ProgressionMutator.ProgressionStore = &RuntimeState->ProgressionStoreState;
	}
	return *RuntimeState.Get();
}

const UParleyDialogueSubsystem::FParleyDialogueRuntimeState& UParleyDialogueSubsystem::GetRuntimeState() const
{
	static const FParleyDialogueRuntimeState EmptyState;
	return RuntimeState.IsValid() ? *RuntimeState.Get() : EmptyState;
}

static FParleyProgressionMutator* GetProgressionMutator(const UParleyDialogueSubsystem* Subsystem)
{
	return Subsystem ? &const_cast<UParleyDialogueSubsystem*>(Subsystem)->GetRuntimeState().ProgressionMutator : nullptr;
}

static FParleyProgressionStore* GetProgressionStore(UParleyDialogueSubsystem* Subsystem)
{
	return Subsystem ? &Subsystem->GetRuntimeState().ProgressionStoreState : nullptr;
}

static const FParleyProgressionStore* GetProgressionStore(const UParleyDialogueSubsystem* Subsystem)
{
	return Subsystem ? &Subsystem->GetRuntimeState().ProgressionStoreState : nullptr;
}

static UTagKeySubsystem* GetLookupSubsystem(const UParleyDialogueSubsystem* Subsystem)
{
	if (!Subsystem)
	{
		return nullptr;
	}
	if (UGameInstance* GI = Subsystem->GetGameInstance())
	{
		return GI->GetSubsystem<UTagKeySubsystem>();
	}
	return nullptr;
}

static APlayerState* FindPlayerStateByCharacterTag(const UWorld* World, const FGameplayTag Slot);
static const FDialoguePlayerPersistentState* FindPlayerDialogueStateByCharacterTag(
	const FParleyProgressionStore* ProgressionStore,
	const FGameplayTag Slot,
	const UWorld* World = nullptr);
static FGameplayTag ResolveDialogueCharacterTagFromIdentity(
	const FParleyProgressionStore* ProgressionStore,
	const FParleyPlayerIdentity& Identity,
	const UWorld* World = nullptr);

static FParleyPlayerIdentity BuildPlayerIdentityFromState(const APlayerState* PS)
{
	FParleyPlayerIdentity Identity;
	if (!PS)
	{
		return Identity;
	}

	Identity.LegacyId = PS->GetPlayerId();
	Identity.DisplayName = GetPlayerDisplayNameFromPlayerState(PS);
	Identity.PlayerCharacterTag = GetCharacterTagFromPlayerState(PS);
	if (PS->GetUniqueId().IsValid())
	{
		Identity.UniqueNetIdString = PS->GetUniqueId()->ToString();
		Identity.UniqueNetIdType = PS->GetUniqueId()->GetType().ToString();
	}
	return Identity;
}

static FGameplayTag ResolveDialogueCharacterTagFromPlayerState(const APlayerState* PlayerState)
{
	if (!PlayerState)
	{
		return FGameplayTag();
	}

	return GetCurrentCharacterTagFromPlayerState(PlayerState);
}

static FGameplayTag ResolveDialogueCharacterTagFromIdentity(
	const FParleyProgressionStore* ProgressionStore,
	const FParleyPlayerIdentity& Identity,
	const UWorld* World)
{
	if (!ProgressionStore)
	{
		return FGameplayTag();
	}

	// Character-owned dialogue state should follow the currently controlled character first.
	// Slot mapping in progression store is retained as fallback for detached/offline contexts.
	if (Identity.PlayerCharacterTag.IsValid())
	{
		if (const APlayerState* LivePlayerState = FindPlayerStateByCharacterTag(World, Identity.PlayerCharacterTag))
		{
			const FGameplayTag LiveCharacterTag = ResolveDialogueCharacterTagFromPlayerState(LivePlayerState);
			if (LiveCharacterTag.IsValid())
			{
				return LiveCharacterTag;
			}
		}
	}

	FParleyPlayerStateProgressionData PlayerStateData;
	int32 PlayerIndex = INDEX_NONE;
	if (ProgressionStore->FindPlayerStateDataByIdentity(Identity, PlayerStateData, PlayerIndex))
	{
		return PlayerStateData.ResolveCurrentCharacterTag();
	}

	if (Identity.PlayerCharacterTag.IsValid())
	{
		if (ProgressionStore->FindPlayerStateDataByCharacterTag(Identity.PlayerCharacterTag, PlayerStateData, PlayerIndex))
		{
			return PlayerStateData.ResolveCurrentCharacterTag();
		}

		return GetDefaultCharacterTagForSlot(Identity.PlayerCharacterTag);
	}

	return FGameplayTag();
}

static const FDialoguePlayerPersistentState* FindPlayerDialogueState(
	const FParleyProgressionStore* ProgressionStore,
	const FParleyPlayerIdentity& Identity,
	const UWorld* World = nullptr)
{
	if (!ProgressionStore)
	{
		return nullptr;
	}

	const FGameplayTag CharacterTag = ResolveDialogueCharacterTagFromIdentity(ProgressionStore, Identity, World);
	FParleyCharacterProgressionData CharacterState;
	int32 CharacterIndex = INDEX_NONE;
	if (!ProgressionStore->FindCharacterStateDataByTag(CharacterTag, CharacterState, CharacterIndex)
		|| !ProgressionStore->CharacterStates.IsValidIndex(CharacterIndex))
	{
		return nullptr;
	}

	return &ProgressionStore->CharacterStates[CharacterIndex].DialogueState;
}

static FDialoguePlayerPersistentState* FindPlayerDialogueStateMutable(
	FParleyProgressionStore* ProgressionStore,
	const FParleyPlayerIdentity& Identity,
	const UWorld* World = nullptr)
{
	if (!ProgressionStore)
	{
		return nullptr;
	}

	const FGameplayTag CharacterTag = ResolveDialogueCharacterTagFromIdentity(ProgressionStore, Identity, World);
	int32 CharacterIndex = INDEX_NONE;
	FParleyCharacterProgressionData* CharacterState = ProgressionStore->FindCharacterStateDataMutable(CharacterTag, CharacterIndex);
	return CharacterState ? &CharacterState->DialogueState : nullptr;
}

static bool IsConversationCompletedByGame(const UParleyDialogueSubsystem* Subsystem, const FGameplayTag ConversationTag)
{
	const FParleyProgressionStore* ProgressionStore = GetProgressionStore(Subsystem);
	return ProgressionStore && ConversationTag.IsValid() && ProgressionStore->DialogueCompletedConversationTagsByGame.HasTagExact(ConversationTag);
}

static bool IsConversationCompletedByPlayer(const UParleyDialogueSubsystem* Subsystem, const FParleyPlayerIdentity& Identity, const FGameplayTag ConversationTag)
{
	if (Subsystem && Subsystem->OnQueryConversationCompleted.IsBound())
	{
		return Subsystem->OnQueryConversationCompleted.Execute(ConversationTag, GetDefaultCharacterTagForSlot(Identity.PlayerCharacterTag));
	}

	const FParleyProgressionStore* ProgressionStore = GetProgressionStore(Subsystem);
	if (!ProgressionStore || !ConversationTag.IsValid())
	{
		return false;
	}

	const FDialoguePlayerPersistentState* PlayerState = FindPlayerDialogueState(
		ProgressionStore,
		Identity,
		Subsystem ? Subsystem->GetWorld() : nullptr);
	return PlayerState && PlayerState->CompletedConversationTags.HasTagExact(ConversationTag);
}

static FParleyActiveDialogueSession* FindSessionByOwnerCharacter(TArray<FParleyActiveDialogueSession>& Sessions, const FGameplayTag Slot)
{
	for (FParleyActiveDialogueSession& Session : Sessions)
	{
		if (!Session.bIsSharedSession && Session.OwnerCharacterTag == Slot)
		{
			return &Session;
		}
	}
	return nullptr;
}

static FParleyActiveDialogueSession* FindPerPlayerSessionByPrimarySpeaker(
	TArray<FParleyActiveDialogueSession>& Sessions,
	const FGameplayTag& PrimarySpeakerTag,
	const FGameplayTag ExcludedOwnerCharacterTag = FGameplayTag())
{
	if (!PrimarySpeakerTag.IsValid())
	{
		return nullptr;
	}

	for (FParleyActiveDialogueSession& Session : Sessions)
	{
		if (Session.bIsSharedSession)
		{
			continue;
		}

		if (ExcludedOwnerCharacterTag.IsValid() && Session.OwnerCharacterTag == ExcludedOwnerCharacterTag)
		{
			continue;
		}

		if (Session.PrimarySpeakerTag.MatchesTagExact(PrimarySpeakerTag))
		{
			return &Session;
		}
	}

	return nullptr;
}

static const FParleyActiveDialogueSession* FindPerPlayerSessionByPrimarySpeaker(
	const TArray<FParleyActiveDialogueSession>& Sessions,
	const FGameplayTag& PrimarySpeakerTag,
	const FGameplayTag ExcludedOwnerCharacterTag = FGameplayTag())
{
	if (!PrimarySpeakerTag.IsValid())
	{
		return nullptr;
	}

	for (const FParleyActiveDialogueSession& Session : Sessions)
	{
		if (Session.bIsSharedSession)
		{
			continue;
		}

		if (ExcludedOwnerCharacterTag.IsValid() && Session.OwnerCharacterTag == ExcludedOwnerCharacterTag)
		{
			continue;
		}

		if (Session.PrimarySpeakerTag.MatchesTagExact(PrimarySpeakerTag))
		{
			return &Session;
		}
	}

	return nullptr;
}

static FParleyActiveDialogueSession* FindSharedSession(TArray<FParleyActiveDialogueSession>& Sessions)
{
	for (FParleyActiveDialogueSession& Session : Sessions)
	{
		if (Session.bIsSharedSession)
		{
			return &Session;
		}
	}
	return nullptr;
}

static FParleyActiveDialogueSession* FindSessionForCharacter(TArray<FParleyActiveDialogueSession>& Sessions, const FGameplayTag Slot)
{
	for (FParleyActiveDialogueSession& Session : Sessions)
	{
		if (Session.Participants.Contains(Slot))
		{
			return &Session;
		}
	}
	return nullptr;
}

static const FParleyActiveDialogueSession* FindSessionForCharacter(const TArray<FParleyActiveDialogueSession>& Sessions, const FGameplayTag Slot)
{
	for (const FParleyActiveDialogueSession& Session : Sessions)
	{
		if (Session.Participants.Contains(Slot))
		{
			return &Session;
		}
	}
	return nullptr;
}

static APlayerController* FindPlayerControllerByCharacter(const UWorld* World, const FGameplayTag Slot)
{
	if (!World || !Slot.IsValid())
	{
		return nullptr;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get())
		{
			if (const APlayerState* PS = PC->GetPlayerState<APlayerState>())
			{
				if (GetCharacterTagFromPlayerState(PS) == Slot)
				{
					return PC;
				}
			}
		}
	}
	return nullptr;
}

static bool CompareNumeric(const float Left, const EDialogueComparisonOp Op, const float Right)
{
	switch (Op)
	{
	case EDialogueComparisonOp::Equals:
		return FMath::IsNearlyEqual(Left, Right);
	case EDialogueComparisonOp::NotEquals:
		return !FMath::IsNearlyEqual(Left, Right);
	case EDialogueComparisonOp::GreaterThan:
		return Left > Right;
	case EDialogueComparisonOp::GreaterOrEqual:
		return Left >= Right;
	case EDialogueComparisonOp::LessThan:
		return Left < Right;
	case EDialogueComparisonOp::LessOrEqual:
		return Left <= Right;
	default:
		return false;
	}
}

static bool CompareBool(const bool Left, const EDialogueComparisonOp Op, const bool Right)
{
	switch (Op)
	{
	case EDialogueComparisonOp::Equals:
		return Left == Right;
	case EDialogueComparisonOp::NotEquals:
		return Left != Right;
	case EDialogueComparisonOp::Present:
		return Left;
	case EDialogueComparisonOp::Absent:
		return !Left;
	default:
		return false;
	}
}

static bool EvaluateTagContainerCondition(const FGameplayTagContainer& Container, const FDialogueCondition& Condition)
{
	const bool bHasTag = Condition.TagValue.IsValid() && Container.HasTag(Condition.TagValue);
	switch (Condition.Operator)
	{
	case EDialogueComparisonOp::Present:
	case EDialogueComparisonOp::Contains:
	case EDialogueComparisonOp::Equals:
		return bHasTag;
	case EDialogueComparisonOp::Absent:
	case EDialogueComparisonOp::NotContains:
	case EDialogueComparisonOp::NotEquals:
		return !bHasTag;
	default:
		return false;
	}
}

static FDialoguePlayerPersistentState* FindOrAddPlayerDialogueState(
	FParleyProgressionStore* ProgressionStore,
	const FParleyPlayerIdentity& Identity,
	const UWorld* World = nullptr)
{
	if (FDialoguePlayerPersistentState* Existing = FindPlayerDialogueStateMutable(ProgressionStore, Identity, World))
	{
		return Existing;
	}

	if (!ProgressionStore)
	{
		return nullptr;
	}

	const FGameplayTag CharacterTag = ResolveDialogueCharacterTagFromIdentity(ProgressionStore, Identity, World);
	if (!CharacterTag.IsValid())
	{
		return nullptr;
	}

	FParleyCharacterProgressionData& CharacterState = ProgressionStore->FindOrAddCharacterStateData(CharacterTag);
	return &CharacterState.DialogueState;
}

static FDialoguePlayerPersistentState* FindOrAddPlayerDialogueStateByCharacterTag(
	FParleyProgressionStore* ProgressionStore,
	const UParleyDialogueSubsystem* DialogueSubsystem,
	const FGameplayTag Slot)
{
	if (!ProgressionStore || !Slot.IsValid())
	{
		return nullptr;
	}

	const APlayerState* PlayerState = FindPlayerStateByCharacterTag(DialogueSubsystem ? DialogueSubsystem->GetWorld() : nullptr, Slot);
	if (PlayerState)
	{
		const FGameplayTag CharacterTag = ResolveDialogueCharacterTagFromPlayerState(PlayerState);
		if (CharacterTag.IsValid())
		{
			ProgressionStore->CharacterTagByIdentity.FindOrAdd(Slot) = CharacterTag;
		}
		return FindOrAddPlayerDialogueState(
			ProgressionStore,
			BuildPlayerIdentityFromState(PlayerState),
			DialogueSubsystem ? DialogueSubsystem->GetWorld() : nullptr);
	}

	const FGameplayTag CharacterTag = GetDefaultCharacterTagForSlot(Slot);
	if (!CharacterTag.IsValid())
	{
		return nullptr;
	}

	FParleyCharacterProgressionData& CharacterState = ProgressionStore->FindOrAddCharacterStateData(CharacterTag);
	ProgressionStore->CharacterTagByIdentity.FindOrAdd(Slot) = CharacterTag;
	return &CharacterState.DialogueState;
}

	static bool AreTagContainersEquivalent(const FGameplayTagContainer& Left, const FGameplayTagContainer& Right)
	{
		return Left.Num() == Right.Num() && Left.HasAllExact(Right) && Right.HasAllExact(Left);
	}

	static void BuildSpeakerOfferCountMap(
		const TArray<FDialogueSpeakerCycleOfferCount>& Source,
		TMap<FGameplayTag, int32>& OutMap)
	{
		OutMap.Reset();
		for (const FDialogueSpeakerCycleOfferCount& Entry : Source)
		{
			if (!Entry.SpeakerTag.IsValid() || Entry.OfferCount <= 0)
			{
				continue;
			}

			const int32 Existing = OutMap.FindRef(Entry.SpeakerTag);
			OutMap.Add(Entry.SpeakerTag, FMath::Max(Existing, Entry.OfferCount));
		}
	}

	static void BuildSpeakerOfferCountArray(
		const TMap<FGameplayTag, int32>& Source,
		TArray<FDialogueSpeakerCycleOfferCount>& OutArray)
	{
		OutArray.Reset();
		for (const TPair<FGameplayTag, int32>& Pair : Source)
		{
			if (!Pair.Key.IsValid() || Pair.Value <= 0)
			{
				continue;
			}

			FDialogueSpeakerCycleOfferCount& Added = OutArray.AddDefaulted_GetRef();
			Added.SpeakerTag = Pair.Key;
			Added.OfferCount = Pair.Value;
		}
	}

	static bool AreSpeakerOfferCountMapsEquivalent(
		const TMap<FGameplayTag, int32>& Left,
		const TMap<FGameplayTag, int32>& Right)
	{
		if (Left.Num() != Right.Num())
		{
			return false;
		}

		for (const TPair<FGameplayTag, int32>& Pair : Left)
		{
			if (Right.FindRef(Pair.Key) != Pair.Value)
			{
				return false;
			}
		}

		return true;
	}

static void SyncCycleOfferStateFromProgressionStoreForCharacter(
	const UParleyDialogueSubsystem* DialogueSubsystem,
	const FGameplayTag Slot,
	TMap<FGameplayTag, FGameplayTagContainer>& SeenByPlayerTransient,
	TMap<FGameplayTag, FGameplayTagContainer>& SkippedByPlayerTransient)
{
	if (!Slot.IsValid())
	{
		return;
	}

	const FParleyProgressionStore* ProgressionStore = GetProgressionStore(DialogueSubsystem);
	if (!ProgressionStore)
	{
		SeenByPlayerTransient.Remove(Slot);
		SkippedByPlayerTransient.Remove(Slot);
		return;
	}

	const FDialoguePlayerPersistentState* PlayerState = FindPlayerDialogueStateByCharacterTag(
		ProgressionStore,
		Slot,
		DialogueSubsystem ? DialogueSubsystem->GetWorld() : nullptr);
	if (!PlayerState)
	{
		SeenByPlayerTransient.Remove(Slot);
		SkippedByPlayerTransient.Remove(Slot);
		return;
	}

	SeenByPlayerTransient.FindOrAdd(Slot) = PlayerState->SeenConversationTagsThisCycle;
	SkippedByPlayerTransient.FindOrAdd(Slot) = PlayerState->SkippedConversationTagsThisCycle;
}

static void SyncSpeakerOfferCountsFromProgressionStoreForCharacter(
	const UParleyDialogueSubsystem* DialogueSubsystem,
	const FGameplayTag Slot,
	TMap<FGameplayTag, TMap<FGameplayTag, int32>>& SpeakerOfferCountsByPlayerTransient)
{
	if (!Slot.IsValid())
	{
		return;
	}

	const FParleyProgressionStore* ProgressionStore = GetProgressionStore(DialogueSubsystem);
	if (!ProgressionStore)
	{
		SpeakerOfferCountsByPlayerTransient.Remove(Slot);
		return;
	}

	const FDialoguePlayerPersistentState* PlayerState = FindPlayerDialogueStateByCharacterTag(
		ProgressionStore,
		Slot,
		DialogueSubsystem ? DialogueSubsystem->GetWorld() : nullptr);
	if (!PlayerState)
	{
		SpeakerOfferCountsByPlayerTransient.Remove(Slot);
		return;
	}

	TMap<FGameplayTag, int32> CountMap;
	BuildSpeakerOfferCountMap(PlayerState->SpeakerOfferCountsThisCycle, CountMap);
	SpeakerOfferCountsByPlayerTransient.FindOrAdd(Slot) = MoveTemp(CountMap);
}

static void PersistCycleOfferStateForCharacter(
	UParleyDialogueSubsystem* DialogueSubsystem,
	const FGameplayTag Slot,
	const TMap<FGameplayTag, FGameplayTagContainer>& SeenByPlayerTransient,
	const TMap<FGameplayTag, FGameplayTagContainer>& SkippedByPlayerTransient,
	const bool bMarkProgressionDirty)
{
	if (!DialogueSubsystem || !Slot.IsValid())
	{
		return;
	}

	FParleyProgressionStore* ProgressionStore = GetProgressionStore(DialogueSubsystem);
	if (!ProgressionStore)
	{
		return;
	}

	FDialoguePlayerPersistentState* PlayerState = FindOrAddPlayerDialogueStateByCharacterTag(ProgressionStore, DialogueSubsystem, Slot);
	if (!PlayerState)
	{
		return;
	}

	const FGameplayTagContainer* SeenRuntimeTags = SeenByPlayerTransient.Find(Slot);
	const FGameplayTagContainer* SkippedRuntimeTags = SkippedByPlayerTransient.Find(Slot);
	const FGameplayTagContainer NewSeenTags = SeenRuntimeTags ? *SeenRuntimeTags : FGameplayTagContainer();
	const FGameplayTagContainer NewSkippedTags = SkippedRuntimeTags ? *SkippedRuntimeTags : FGameplayTagContainer();

	const bool bSeenChanged = !AreTagContainersEquivalent(PlayerState->SeenConversationTagsThisCycle, NewSeenTags);
	const bool bSkippedChanged = !AreTagContainersEquivalent(PlayerState->SkippedConversationTagsThisCycle, NewSkippedTags);
	if (!bSeenChanged && !bSkippedChanged)
	{
		return;
	}

	PlayerState->SeenConversationTagsThisCycle = NewSeenTags;
	PlayerState->SkippedConversationTagsThisCycle = NewSkippedTags;

	if (bMarkProgressionDirty)
	{
		if (FParleyProgressionMutator* ProgressionMutator = GetProgressionMutator(DialogueSubsystem))
		{
			ProgressionMutator->MarkStateDirty();
		}
	}
}

static void PersistSpeakerOfferCountsForCharacter(
	UParleyDialogueSubsystem* DialogueSubsystem,
	const FGameplayTag Slot,
	const TMap<FGameplayTag, TMap<FGameplayTag, int32>>& SpeakerOfferCountsByPlayerTransient,
	const bool bMarkProgressionDirty)
{
	if (!DialogueSubsystem || !Slot.IsValid())
	{
		return;
	}

	FParleyProgressionStore* ProgressionStore = GetProgressionStore(DialogueSubsystem);
	if (!ProgressionStore)
	{
		return;
	}

	FDialoguePlayerPersistentState* PlayerState = FindOrAddPlayerDialogueStateByCharacterTag(ProgressionStore, DialogueSubsystem, Slot);
	if (!PlayerState)
	{
		return;
	}

	const TMap<FGameplayTag, int32>* RuntimeMap = SpeakerOfferCountsByPlayerTransient.Find(Slot);
	TMap<FGameplayTag, int32> NewMap;
	if (RuntimeMap)
	{
		NewMap = *RuntimeMap;
	}

	TMap<FGameplayTag, int32> ExistingMap;
	BuildSpeakerOfferCountMap(PlayerState->SpeakerOfferCountsThisCycle, ExistingMap);
	if (AreSpeakerOfferCountMapsEquivalent(ExistingMap, NewMap))
	{
		return;
	}

	BuildSpeakerOfferCountArray(NewMap, PlayerState->SpeakerOfferCountsThisCycle);
	if (bMarkProgressionDirty)
	{
		if (FParleyProgressionMutator* ProgressionMutator = GetProgressionMutator(DialogueSubsystem))
		{
			ProgressionMutator->MarkStateDirty();
		}
	}
}

static void PersistSeenCycleTagsForCharacter(
	UParleyDialogueSubsystem* DialogueSubsystem,
	const FGameplayTag Slot,
	const TMap<FGameplayTag, FGameplayTagContainer>& SeenByPlayerTransient,
	const bool bMarkProgressionDirty)
{
	if (!DialogueSubsystem || !Slot.IsValid())
	{
		return;
	}

	FParleyProgressionStore* ProgressionStore = GetProgressionStore(DialogueSubsystem);
	if (!ProgressionStore)
	{
		return;
	}

	FDialoguePlayerPersistentState* PlayerState = FindOrAddPlayerDialogueStateByCharacterTag(ProgressionStore, DialogueSubsystem, Slot);
	if (!PlayerState)
	{
		return;
	}

	const FGameplayTagContainer* SeenRuntimeTags = SeenByPlayerTransient.Find(Slot);
	const FGameplayTagContainer NewSeenTags = SeenRuntimeTags ? *SeenRuntimeTags : FGameplayTagContainer();
	if (AreTagContainersEquivalent(PlayerState->SeenConversationTagsThisCycle, NewSeenTags))
	{
		return;
	}

	PlayerState->SeenConversationTagsThisCycle = NewSeenTags;
	if (bMarkProgressionDirty)
	{
		if (FParleyProgressionMutator* ProgressionMutator = GetProgressionMutator(DialogueSubsystem))
		{
			ProgressionMutator->MarkStateDirty();
		}
	}
}

static FGameplayTag GetDialogueSpeakerPlayerPlaceholderTag()
{
	return UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Parley.Speaker.Requester"), false);
}

static FGameplayTag GetDialogueSpeakerOwnerPlaceholderTag()
{
	return UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Parley.Speaker.Owner"), false);
}

static bool IsRequesterPlaceholderSpeakerTag(const FGameplayTag& SpeakerTag)
{
	if (!SpeakerTag.IsValid())
	{
		return false;
	}

	const FGameplayTag RequesterPlaceholderTag = GetDialogueSpeakerPlayerPlaceholderTag();
	return RequesterPlaceholderTag.IsValid() && SpeakerTag.MatchesTagExact(RequesterPlaceholderTag);
}

static bool IsOwnerPlaceholderSpeakerTag(const FGameplayTag& SpeakerTag)
{
	const FGameplayTag OwnerPlaceholderTag = GetDialogueSpeakerOwnerPlaceholderTag();
	return SpeakerTag.IsValid()
		&& OwnerPlaceholderTag.IsValid()
		&& SpeakerTag.MatchesTagExact(OwnerPlaceholderTag);
}

static FGameplayTag ResolvePlayerSpeakerTagFromCharacterTag(const FGameplayTag& CharacterTag)
{
	return CharacterTag;
}

static bool PassesCharacterRestriction(
	const FGameplayTag& RestrictionTag,
	const FGameplayTag& ResolvedPlayerSpeakerTag)
{
	if (!RestrictionTag.IsValid())
	{
		return true;
	}

	return ResolvedPlayerSpeakerTag.IsValid()
		&& (ResolvedPlayerSpeakerTag.MatchesTag(RestrictionTag) || RestrictionTag.MatchesTag(ResolvedPlayerSpeakerTag));
}

static FGameplayTag ResolvePlayerSpeakerTag(const APlayerState* PlayerState)
{
	if (!PlayerState)
	{
		return FGameplayTag();
	}

	const FGameplayTag CurrentCharacterTag = ResolveDialogueCharacterTagFromPlayerState(PlayerState);
	const FGameplayTag CharacterResolvedSpeakerTag = ResolvePlayerSpeakerTagFromCharacterTag(CurrentCharacterTag);
	if (CharacterResolvedSpeakerTag.IsValid())
	{
		return CharacterResolvedSpeakerTag;
	}

	return GetCharacterTagFromPlayerState(PlayerState);
}

static FGameplayTag ResolveSpeakerTagForContext(
	const FGameplayTag& RequestedSpeakerTag,
	const FDialogueRuntimeContext& Context,
	const FGameplayTag& FallbackSpeakerTag = FGameplayTag())
{
	if (RequestedSpeakerTag.IsValid())
	{
		if (IsRequesterPlaceholderSpeakerTag(RequestedSpeakerTag))
		{
			if (Context.SourceSpeakerTag.IsValid())
			{
				return Context.SourceSpeakerTag;
			}

			if (Context.ResolvedPlayerSpeakerTag.IsValid())
			{
				return Context.ResolvedPlayerSpeakerTag;
			}

			return FallbackSpeakerTag;
		}
		if (IsOwnerPlaceholderSpeakerTag(RequestedSpeakerTag))
		{
			return Context.PrimarySpeakerTag.IsValid() ? Context.PrimarySpeakerTag : FallbackSpeakerTag;
		}

		return RequestedSpeakerTag;
	}

	return FallbackSpeakerTag;
}

static FGameplayTag StripLeafGameplayTag(const FGameplayTag& Tag)
{
	if (!Tag.IsValid())
	{
		return FGameplayTag();
	}

	FString Path = Tag.ToString();
	int32 LastDot = INDEX_NONE;
	if (!Path.FindLastChar(TEXT('.'), LastDot) || LastDot <= 0)
	{
		return FGameplayTag();
	}

	Path.LeftInline(LastDot, EAllowShrinking::No);
	return UGameplayTagsManager::Get().RequestGameplayTag(FName(*Path), false);
}

static const FParleySpeakerRow* FindSpeakerRowByConversationSpeakerTag(
	const TMap<FGameplayTag, FParleySpeakerRow>& SpeakerRowsByTag,
	const FGameplayTag& LineSpeakerTag,
	FGameplayTag& OutResolvedSpeakerRowTag)
{
	OutResolvedSpeakerRowTag = FGameplayTag();
	FGameplayTag Candidate = LineSpeakerTag;

	while (Candidate.IsValid())
	{
		if (const FParleySpeakerRow* Found = SpeakerRowsByTag.Find(Candidate))
		{
			OutResolvedSpeakerRowTag = Candidate;
			return Found;
		}
		Candidate = StripLeafGameplayTag(Candidate);
	}

	return nullptr;
}

static bool IsBuiltInDialogueSpeakerTag(const FGameplayTag& SpeakerTag)
{
	if (!SpeakerTag.IsValid())
	{
		return false;
	}

	if (IsRequesterPlaceholderSpeakerTag(SpeakerTag))
	{
		return true;
	}
	if (IsOwnerPlaceholderSpeakerTag(SpeakerTag))
	{
		return true;
	}

	return false;
}

static bool IsResolvableConversationSpeakerTag(
	const TMap<FGameplayTag, FParleySpeakerRow>& SpeakerRowsByTag,
	const FGameplayTag& SpeakerTag)
{
	if (!SpeakerTag.IsValid())
	{
		return false;
	}

	if (IsBuiltInDialogueSpeakerTag(SpeakerTag))
	{
		return true;
	}

	FGameplayTag ResolvedSpeakerRowTag;
	return FindSpeakerRowByConversationSpeakerTag(SpeakerRowsByTag, SpeakerTag, ResolvedSpeakerRowTag) != nullptr;
}

static bool IsStructurallyValidConversationSpeakerTag(
	const FGameplayTag& SpeakerTag,
	const FGameplayTag& SpeakerRootTag)
{
	if (!SpeakerTag.IsValid())
	{
		return false;
	}

	if (IsBuiltInDialogueSpeakerTag(SpeakerTag))
	{
		return true;
	}

	return SpeakerRootTag.IsValid() && SpeakerTag.MatchesTag(SpeakerRootTag);
}

static const FParleySpeakerRow* ResolveSpeakerRowForPresentation(
	const TMap<FGameplayTag, FParleySpeakerRow>& SpeakerRowsByTag,
	const FGameplayTag& RequestedSpeakerTag,
	FGameplayTag& OutResolvedSpeakerRowTag)
{
	const FParleySpeakerRow* SpeakerRow = FindSpeakerRowByConversationSpeakerTag(
		SpeakerRowsByTag,
		RequestedSpeakerTag,
		OutResolvedSpeakerRowTag);
	if (SpeakerRow || !IsBuiltInDialogueSpeakerTag(RequestedSpeakerTag))
	{
		return SpeakerRow;
	}

	TArray<FGameplayTag> FallbackCandidates;
	const FGameplayTag RequesterTag = GetDialogueSpeakerPlayerPlaceholderTag();
	const FGameplayTag OwnerTag = GetDialogueSpeakerOwnerPlaceholderTag();
	if (OwnerTag.IsValid() && RequestedSpeakerTag.MatchesTag(OwnerTag))
	{
		FallbackCandidates = { RequesterTag };
	}
	else
	{
		FallbackCandidates = { OwnerTag, RequesterTag };
	}

	for (const FGameplayTag CandidateTag : FallbackCandidates)
	{
		if (!CandidateTag.IsValid())
		{
			continue;
		}

		SpeakerRow = FindSpeakerRowByConversationSpeakerTag(SpeakerRowsByTag, CandidateTag, OutResolvedSpeakerRowTag);
		if (SpeakerRow)
		{
			return SpeakerRow;
		}
	}

	return nullptr;
}

static const FDialoguePlayerPersistentState* FindPlayerDialogueStateByCharacterTag(
	const FParleyProgressionStore* ProgressionStore,
	const FGameplayTag Slot,
	const UWorld* World)
{
	if (!ProgressionStore || !Slot.IsValid())
	{
		return nullptr;
	}

	if (const APlayerState* PlayerState = FindPlayerStateByCharacterTag(World, Slot))
	{
		return FindPlayerDialogueState(ProgressionStore, BuildPlayerIdentityFromState(PlayerState), World);
	}

	FParleyCharacterProgressionData CharacterState;
	int32 CharacterIndex = INDEX_NONE;
	if (!ProgressionStore->FindCharacterStateDataByTag(GetDefaultCharacterTagForSlot(Slot), CharacterState, CharacterIndex)
		|| !ProgressionStore->CharacterStates.IsValidIndex(CharacterIndex))
	{
		return nullptr;
	}

	return &ProgressionStore->CharacterStates[CharacterIndex].DialogueState;
}

static FSpeakerPortraitData ResolvePortraitForSpeaker(
	const TMap<FGameplayTag, FParleySpeakerRow>& SpeakerRowsByTag,
	const FGameplayTag& LineSpeakerTag)
{
	FSpeakerPortraitData ResolvedPortrait;
	FGameplayTag SpeakerRowTag;
	const FParleySpeakerRow* SpeakerRow = ResolveSpeakerRowForPresentation(SpeakerRowsByTag, LineSpeakerTag, SpeakerRowTag);
	if (!SpeakerRow)
	{
		return ResolvedPortrait;
	}

	for (const FSpeakerPortraitEntry& Entry : SpeakerRow->Portraits)
	{
		if (Entry.PortraitTag.IsValid() && Entry.PortraitTag.MatchesTagExact(LineSpeakerTag))
		{
			return Entry.Portrait;
		}
	}

	if (SpeakerRowTag.IsValid())
	{
		const FString DefaultPortraitPath = FString::Printf(TEXT("%s.Default"), *SpeakerRowTag.ToString());
		const FGameplayTag DefaultPortraitTag = UGameplayTagsManager::Get().RequestGameplayTag(FName(*DefaultPortraitPath), false);
		if (DefaultPortraitTag.IsValid())
		{
			for (const FSpeakerPortraitEntry& Entry : SpeakerRow->Portraits)
			{
				if (Entry.PortraitTag.IsValid() && Entry.PortraitTag.MatchesTagExact(DefaultPortraitTag))
				{
					return Entry.Portrait;
				}
			}
		}
	}

	return SpeakerRow->DefaultPortrait;
}

static FGameplayTag GetDialogueEmotionRootTag()
{
	return UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Parley.Emotion"), false);
}

static FGameplayTag GetDialogueDefaultEmotionTag()
{
	return UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Parley.Emotion.Default"), false);
}

static FGameplayTag BuildEmotionTagFromSpeakerTag(
	const FGameplayTag& SpeakerTag,
	const FGameplayTag& ResolvedSpeakerRowTag)
{
	if (!SpeakerTag.IsValid())
	{
		return FGameplayTag();
	}

	const FGameplayTag EmotionRootTag = GetDialogueEmotionRootTag();
	if (!EmotionRootTag.IsValid())
	{
		return FGameplayTag();
	}

	const FString SpeakerPath = SpeakerTag.ToString();
	TArray<FString> CandidateBasePaths;
	if (ResolvedSpeakerRowTag.IsValid())
	{
		CandidateBasePaths.Add(ResolvedSpeakerRowTag.ToString());
	}

	// Built-in speaker aliases can appear directly in line authoring with suffix emotion segments
	// (for example Parley.Speaker.Requester.Angry), so include them as explicit suffix bases.
	const FGameplayTag RequesterTag = GetDialogueSpeakerPlayerPlaceholderTag();
	if (RequesterTag.IsValid())
	{
		CandidateBasePaths.AddUnique(RequesterTag.ToString());
	}
	const FGameplayTag OwnerTag = GetDialogueSpeakerOwnerPlaceholderTag();
	if (OwnerTag.IsValid())
	{
		CandidateBasePaths.AddUnique(OwnerTag.ToString());
	}

	// Only treat explicit suffix segments as emotion keys.
	// This avoids misinterpreting base speaker ids (for example Parley.Speaker.Fred) as Parley.Emotion.Fred.
	for (const FString& BasePath : CandidateBasePaths)
	{
		if (BasePath.IsEmpty())
		{
			continue;
		}

		const FString Prefix = BasePath + TEXT(".");
		if (!SpeakerPath.StartsWith(Prefix))
		{
			continue;
		}

		const FString EmotionSuffix = SpeakerPath.RightChop(Prefix.Len());
		if (EmotionSuffix.IsEmpty())
		{
			continue;
		}

		const FString EmotionPath = FString::Printf(TEXT("%s.%s"), *EmotionRootTag.ToString(), *EmotionSuffix);
		return UGameplayTagsManager::Get().RequestGameplayTag(FName(*EmotionPath), false);
	}

	return FGameplayTag();
}

static FGameplayTag ResolvePresentationEmotionTagFromSpeakerTag(
	const FGameplayTag& SpeakerTag,
	const FGameplayTag& ResolvedSpeakerRowTag)
{
	const FGameplayTag ExplicitEmotionTag = BuildEmotionTagFromSpeakerTag(SpeakerTag, ResolvedSpeakerRowTag);
	if (ExplicitEmotionTag.IsValid())
	{
		return ExplicitEmotionTag;
	}

	return GetDialogueDefaultEmotionTag();
}

static bool ResolveSpeakerEmotionFallbackAudioForSpeaker(
	const TMap<FGameplayTag, FParleySpeakerRow>& SpeakerRowsByTag,
	const FGameplayTag& LineSpeakerTag,
	USoundBase*& OutNativeSound,
	FGameplayTag& OutAudioCueTag)
{
	OutNativeSound = nullptr;
	OutAudioCueTag = FGameplayTag();

	FGameplayTag SpeakerRowTag;
	const FParleySpeakerRow* SpeakerRow = ResolveSpeakerRowForPresentation(SpeakerRowsByTag, LineSpeakerTag, SpeakerRowTag);
	if (!SpeakerRow || SpeakerRow->EmotionAudioFallbacks.IsEmpty())
	{
		return false;
	}

	const FGameplayTag ResolvedEmotionTag = BuildEmotionTagFromSpeakerTag(LineSpeakerTag, SpeakerRowTag);
	const FGameplayTag DefaultEmotionTag = GetDialogueDefaultEmotionTag();
	const FParleySpeakerEmotionAudioEntry* DefaultEntry = nullptr;
	const FParleySpeakerEmotionAudioEntry* UnscopedEntry = nullptr;

	for (const FParleySpeakerEmotionAudioEntry& Entry : SpeakerRow->EmotionAudioFallbacks)
	{
		if (ResolvedEmotionTag.IsValid() && Entry.EmotionTag.IsValid() && Entry.EmotionTag.MatchesTagExact(ResolvedEmotionTag))
		{
			OutNativeSound = Entry.NativeSound;
			OutAudioCueTag = Entry.AudioCueTag;
			return OutNativeSound != nullptr || OutAudioCueTag.IsValid();
		}

		if (DefaultEmotionTag.IsValid() && Entry.EmotionTag.IsValid() && Entry.EmotionTag.MatchesTagExact(DefaultEmotionTag))
		{
			DefaultEntry = &Entry;
		}
		else if (!Entry.EmotionTag.IsValid() && !UnscopedEntry)
		{
			UnscopedEntry = &Entry;
		}
	}

	const FParleySpeakerEmotionAudioEntry* ChosenEntry = DefaultEntry ? DefaultEntry : UnscopedEntry;
	if (!ChosenEntry)
	{
		return false;
	}

	OutNativeSound = ChosenEntry->NativeSound;
	OutAudioCueTag = ChosenEntry->AudioCueTag;
	return OutNativeSound != nullptr || OutAudioCueTag.IsValid();
}

static FString NormalizeDialogueFieldToken(const FString& RawFieldToken)
{
	FString Normalized = RawFieldToken;
	Normalized.TrimStartAndEndInline();
	Normalized.ReplaceInline(TEXT(" "), TEXT(""));
	Normalized.ReplaceInline(TEXT("_"), TEXT(""));
	Normalized.ReplaceInline(TEXT("-"), TEXT(""));
	Normalized.ToLowerInline();
	return Normalized;
}

static bool ResolveSpeakerFieldValue(
	const FParleySpeakerRow& SpeakerRow,
	const FGameplayTag& ResolvedSpeakerRowTag,
	const FString& RequestedFieldToken,
	FString& OutValue)
{
	OutValue.Empty();
	const FString Field = NormalizeDialogueFieldToken(RequestedFieldToken.IsEmpty() ? TEXT("displayname") : RequestedFieldToken);

	if (Field == TEXT("displayname") || Field == TEXT("name"))
	{
		OutValue = SpeakerRow.DisplayName.ToString();
		return true;
	}
	if (Field == TEXT("description") || Field == TEXT("desc"))
	{
		OutValue = SpeakerRow.Description.ToString();
		return true;
	}
	if (Field == TEXT("speakertag") || Field == TEXT("tag"))
	{
		if (SpeakerRow.SpeakerTag.IsValid())
		{
			OutValue = SpeakerRow.SpeakerTag.ToString();
			return true;
		}
		if (ResolvedSpeakerRowTag.IsValid())
		{
			OutValue = ResolvedSpeakerRowTag.ToString();
			return true;
		}
		return false;
	}
	if (Field == TEXT("faction") || Field == TEXT("factiontag"))
	{
		if (!SpeakerRow.FactionTag.IsValid())
		{
			return false;
		}
		OutValue = SpeakerRow.FactionTag.ToString();
		return true;
	}
	if (Field == TEXT("font") || Field == TEXT("fontstyle") || Field == TEXT("linefontstyle") || Field == TEXT("linefontstyletag"))
	{
		if (!SpeakerRow.LineFont.IsNull())
		{
			OutValue = SpeakerRow.LineFont.ToSoftObjectPath().ToString();
			return true;
		}

		if (SpeakerRow.LineFontStyleTag.IsNone())
		{
			return false;
		}
		OutValue = SpeakerRow.LineFontStyleTag.ToString();
		return true;
	}

	return false;
}

static bool ResolveInstancedStructFieldValue(
	const FInstancedStruct& StructValue,
	const FString& RequestedFieldToken,
	FString& OutValue)
{
	OutValue.Empty();
	const UScriptStruct* ScriptStruct = StructValue.GetScriptStruct();
	const void* StructMemory = StructValue.GetMemory();
	if (!ScriptStruct || !StructMemory)
	{
		return false;
	}

	TArray<FString> CandidateFields;
	CandidateFields.Add(NormalizeDialogueFieldToken(RequestedFieldToken.IsEmpty() ? TEXT("displayname") : RequestedFieldToken));
	if (CandidateFields[0] == TEXT("name"))
	{
		CandidateFields.Add(TEXT("displayname"));
	}
	if (CandidateFields[0] == TEXT("displayname"))
	{
		CandidateFields.Add(TEXT("name"));
	}
	if (CandidateFields[0] == TEXT("tag"))
	{
		CandidateFields.Add(TEXT("speakertag"));
		CandidateFields.Add(TEXT("conversationtag"));
	}
	if (CandidateFields[0] == TEXT("font") || CandidateFields[0] == TEXT("fontstyle"))
	{
		CandidateFields.Add(TEXT("linefont"));
		CandidateFields.Add(TEXT("linefontstyletag"));
	}

	for (TFieldIterator<const FProperty> It(ScriptStruct); It; ++It)
	{
		const FProperty* Property = *It;
		if (!Property)
		{
			continue;
		}

		const FString PropertyName = NormalizeDialogueFieldToken(Property->GetName());
		if (!CandidateFields.Contains(PropertyName))
		{
			continue;
		}

		if (const FTextProperty* TextProperty = CastField<const FTextProperty>(Property))
		{
			OutValue = TextProperty->GetPropertyValue_InContainer(StructMemory).ToString();
			return true;
		}
		if (const FNameProperty* NameProperty = CastField<const FNameProperty>(Property))
		{
			OutValue = NameProperty->GetPropertyValue_InContainer(StructMemory).ToString();
			return true;
		}
		if (const FStrProperty* StringProperty = CastField<const FStrProperty>(Property))
		{
			OutValue = StringProperty->GetPropertyValue_InContainer(StructMemory);
			return true;
		}
		if (const FStructProperty* StructProperty = CastField<const FStructProperty>(Property))
		{
			if (StructProperty->Struct == TBaseStructure<FGameplayTag>::Get())
			{
				const FGameplayTag* TagValue = StructProperty->ContainerPtrToValuePtr<FGameplayTag>(StructMemory);
				if (!TagValue || !TagValue->IsValid())
				{
					return false;
				}
				OutValue = TagValue->ToString();
				return true;
			}
		}

		FString Exported;
		const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(StructMemory);
		Property->ExportTextItem_Direct(Exported, ValuePtr, nullptr, nullptr, PPF_None);
		Exported.TrimStartAndEndInline();
		if (Exported.StartsWith(TEXT("\"")) && Exported.EndsWith(TEXT("\"")) && Exported.Len() >= 2)
		{
			Exported = Exported.Mid(1, Exported.Len() - 2);
		}
		OutValue = Exported;
		return true;
	}

	return false;
}

static FGameplayTag ResolveGameplayTagFromToken(
	const FString& RawTagToken,
	const TMap<FGameplayTag, FParleySpeakerRow>& SpeakerRowsByTag)
{
	FString TagToken = RawTagToken;
	TagToken.TrimStartAndEndInline();
	if (TagToken.IsEmpty())
	{
		return FGameplayTag();
	}

	const FGameplayTag Requested = UGameplayTagsManager::Get().RequestGameplayTag(FName(*TagToken), false);
	if (Requested.IsValid())
	{
		return Requested;
	}

	for (const TPair<FGameplayTag, FParleySpeakerRow>& Pair : SpeakerRowsByTag)
	{
		if (Pair.Key.ToString().Equals(TagToken, ESearchCase::IgnoreCase))
		{
			return Pair.Key;
		}
	}

	return FGameplayTag();
}

static bool ResolveDialogueTagFieldValue(
	const UParleyDialogueSubsystem* DialogueSubsystem,
	const TMap<FGameplayTag, FParleySpeakerRow>& SpeakerRowsByTag,
	const FGameplayTag& RequestedTag,
	const FString& RequestedFieldToken,
	FString& OutValue,
	FString* OutFailureReason = nullptr)
{
	auto SetFailureReason = [OutFailureReason](const FString& Reason)
	{
		if (OutFailureReason)
		{
			*OutFailureReason = Reason;
		}
	};

	OutValue.Empty();
	if (OutFailureReason)
	{
		OutFailureReason->Empty();
	}

	if (!RequestedTag.IsValid())
	{
		SetFailureReason(TEXT("Gameplay tag token is invalid."));
		return false;
	}

	FString SpeakerFieldFailure;
	FGameplayTag ResolvedSpeakerRowTag;
	if (const FParleySpeakerRow* SpeakerRow = ResolveSpeakerRowForPresentation(SpeakerRowsByTag, RequestedTag, ResolvedSpeakerRowTag))
	{
		if (ResolveSpeakerFieldValue(*SpeakerRow, ResolvedSpeakerRowTag, RequestedFieldToken, OutValue))
		{
			return true;
		}
		SpeakerFieldFailure = FString::Printf(
			TEXT("Field '%s' was not found on speaker row '%s'."),
			*RequestedFieldToken,
			*ResolvedSpeakerRowTag.ToString());
	}

	UTagKeySubsystem* Lookup = GetLookupSubsystem(DialogueSubsystem);
	if (!Lookup)
	{
		if (!SpeakerFieldFailure.IsEmpty())
		{
			SetFailureReason(SpeakerFieldFailure + TEXT(" TagKeySubsystem is unavailable."));
		}
		else
		{
			SetFailureReason(TEXT("TagKeySubsystem is unavailable."));
		}
		return false;
	}

	FInstancedStruct RowData;
	FString LookupError;
	if (!Lookup->TryResolveRowStructForTag(RequestedTag, RowData, LookupError))
	{
		if (LookupError.IsEmpty())
		{
			LookupError = TEXT("resolver returned no details");
		}
		if (!SpeakerFieldFailure.IsEmpty())
		{
			SetFailureReason(FString::Printf(TEXT("%s Content lookup failed for tag '%s' (%s)."), *SpeakerFieldFailure, *RequestedTag.ToString(), *LookupError));
		}
		else
		{
			SetFailureReason(FString::Printf(TEXT("Content lookup failed for tag '%s' (%s)."), *RequestedTag.ToString(), *LookupError));
		}
		return false;
	}

	if (!ResolveInstancedStructFieldValue(RowData, RequestedFieldToken, OutValue))
	{
		SetFailureReason(FString::Printf(
			TEXT("Field '%s' was not found in content row resolved for '%s'."),
			*RequestedFieldToken,
			*RequestedTag.ToString()));
		return false;
	}

	return true;
}

static FString ApplyDialogueLookupTokens(
	const FString& SourceText,
	const UParleyDialogueSubsystem* DialogueSubsystem,
	const TMap<FGameplayTag, FParleySpeakerRow>& SpeakerRowsByTag,
	const FDialogueRuntimeContext& Context,
	const FGameplayTag& CurrentLineSpeakerTag)
{
	if (SourceText.IsEmpty())
	{
		return SourceText;
	}

	FString Result;
	Result.Reserve(SourceText.Len() + 32);
	static const FString UnknownCommandLiteral = TEXT("UNKNOWN");

	auto AppendUnknownAndLog = [&](const FString& TokenText, const FString& ErrorReason)
	{
		const FString ConversationLabel = Context.ConversationTag.IsValid()
			? Context.ConversationTag.ToString()
			: TEXT("<InvalidConversationTag>");
		const FGameplayTag EffectiveSpeakerTag = CurrentLineSpeakerTag.IsValid() ? CurrentLineSpeakerTag : Context.PrimarySpeakerTag;
		const FString SpeakerLabel = EffectiveSpeakerTag.IsValid()
			? EffectiveSpeakerTag.ToString()
			: TEXT("<InvalidSpeakerTag>");

		UE_LOG(ParleyLog, Error,
			TEXT("[Dialogue] Line command '[%s]' resolved to UNKNOWN. Reason: %s (Conversation=%s Speaker=%s)"),
			*TokenText,
			*ErrorReason,
			*ConversationLabel,
			*SpeakerLabel);
		Result += UnknownCommandLiteral;
	};

	for (int32 Index = 0; Index < SourceText.Len();)
	{
		if (SourceText[Index] != TCHAR('['))
		{
			Result.AppendChar(SourceText[Index]);
			++Index;
			continue;
		}

		int32 CloseIndex = INDEX_NONE;
		for (int32 Search = Index + 1; Search < SourceText.Len(); ++Search)
		{
			if (SourceText[Search] == TCHAR(']'))
			{
				CloseIndex = Search;
				break;
			}
		}

		if (CloseIndex == INDEX_NONE)
		{
			Result.AppendChar(SourceText[Index]);
			++Index;
			continue;
		}

		const FString Token = SourceText.Mid(Index + 1, CloseIndex - Index - 1).TrimStartAndEnd();
		const FString TokenLower = Token.ToLower();
		if (TokenLower == TEXT("/font"))
		{
			Result += SourceText.Mid(Index, CloseIndex - Index + 1);
			Index = CloseIndex + 1;
			continue;
		}
		if (TokenLower.StartsWith(TEXT("font:")))
		{
			FString FontStyle = Token.Mid(5).TrimStartAndEnd();
			FontStyle.ReplaceInline(TEXT(" "), TEXT(""));
			if (FontStyle.IsEmpty())
			{
				AppendUnknownAndLog(Token, TEXT("font command is missing a style name."));
			}
			else
			{
				Result += SourceText.Mid(Index, CloseIndex - Index + 1);
			}
			Index = CloseIndex + 1;
			continue;
		}

		FGameplayTag TargetTag;
		FString FieldToken = TEXT("displayname");
		FString CommandToken = Token;
		int32 FieldSeparator = INDEX_NONE;
		if (Token.FindLastChar(TEXT('-'), FieldSeparator) && FieldSeparator > 0 && FieldSeparator < Token.Len() - 1)
		{
			CommandToken = Token.Left(FieldSeparator);
			FieldToken = Token.Mid(FieldSeparator + 1);
		}
		const FString CommandTokenLower = CommandToken.ToLower();

		if (CommandTokenLower == TEXT("speaker"))
		{
			TargetTag = CurrentLineSpeakerTag.IsValid() ? CurrentLineSpeakerTag : Context.PrimarySpeakerTag;
		}
		else
		{
			if (!CommandToken.Contains(TEXT(".")))
			{
				AppendUnknownAndLog(Token, TEXT("command is not recognized."));
				Index = CloseIndex + 1;
				continue;
			}

			TargetTag = ResolveGameplayTagFromToken(CommandToken, SpeakerRowsByTag);
			if (!TargetTag.IsValid())
			{
				AppendUnknownAndLog(Token, FString::Printf(TEXT("could not resolve gameplay tag '%s'."), *CommandToken));
				Index = CloseIndex + 1;
				continue;
			}
		}

		FString Replacement;
		FString ResolveErrorReason;
		if (ResolveDialogueTagFieldValue(DialogueSubsystem, SpeakerRowsByTag, TargetTag, FieldToken, Replacement, &ResolveErrorReason))
		{
			Result += Replacement;
		}
		else
		{
			if (ResolveErrorReason.IsEmpty())
			{
				ResolveErrorReason = TEXT("unspecified lookup failure.");
			}
			AppendUnknownAndLog(Token, ResolveErrorReason);
		}

		Index = CloseIndex + 1;
	}

	return Result;
}

static FString ApplyDialogueFontMarkup(const FString& SourceText)
{
	if (SourceText.IsEmpty())
	{
		return SourceText;
	}

	FString Result;
	Result.Reserve(SourceText.Len() + 16);
	int32 OpenFontCount = 0;

	for (int32 Index = 0; Index < SourceText.Len();)
	{
		if (SourceText[Index] != TCHAR('['))
		{
			Result.AppendChar(SourceText[Index]);
			++Index;
			continue;
		}

		int32 CloseIndex = INDEX_NONE;
		for (int32 Search = Index + 1; Search < SourceText.Len(); ++Search)
		{
			if (SourceText[Search] == TCHAR(']'))
			{
				CloseIndex = Search;
				break;
			}
		}

		if (CloseIndex == INDEX_NONE)
		{
			Result.AppendChar(SourceText[Index]);
			++Index;
			continue;
		}

		FString Token = SourceText.Mid(Index + 1, CloseIndex - Index - 1).TrimStartAndEnd();
		const FString TokenLower = Token.ToLower();
		if (TokenLower == TEXT("/font"))
		{
			if (OpenFontCount > 0)
			{
				Result += TEXT("</>");
				--OpenFontCount;
			}
			Index = CloseIndex + 1;
			continue;
		}

		if (TokenLower.StartsWith(TEXT("font:")))
		{
			FString StyleName = Token.Mid(5).TrimStartAndEnd();
			StyleName.ReplaceInline(TEXT(" "), TEXT(""));
			if (!StyleName.IsEmpty())
			{
				Result += FString::Printf(TEXT("<%s>"), *StyleName);
				++OpenFontCount;
				Index = CloseIndex + 1;
				continue;
			}
		}

		Result += SourceText.Mid(Index, CloseIndex - Index + 1);
		Index = CloseIndex + 1;
	}

	while (OpenFontCount-- > 0)
	{
		Result += TEXT("</>");
	}

	return Result;
}

static FString ApplyBasicDialogueStyleMarkup(const FString& SourceText)
{
	if (SourceText.IsEmpty())
	{
		return SourceText;
	}

	FString Result;
	Result.Reserve(SourceText.Len() + 24);

	bool bBoldItalicOpen = false;
	bool bItalicOpen = false;
	bool bBoldOpen = false;
	bool bStrikeOpen = false;

	for (int32 Index = 0; Index < SourceText.Len();)
	{
		if (Index + 2 < SourceText.Len()
			&& SourceText[Index] == TCHAR('*')
			&& SourceText[Index + 1] == TCHAR('*')
			&& SourceText[Index + 2] == TCHAR('*'))
		{
			Result += bBoldItalicOpen ? TEXT("</>") : TEXT("<bi>");
			bBoldItalicOpen = !bBoldItalicOpen;
			Index += 3;
			continue;
		}

		if (Index + 1 < SourceText.Len()
			&& SourceText[Index] == TCHAR('*')
			&& SourceText[Index + 1] == TCHAR('*'))
		{
			Result += bItalicOpen ? TEXT("</>") : TEXT("<i>");
			bItalicOpen = !bItalicOpen;
			Index += 2;
			continue;
		}

		if (SourceText[Index] == TCHAR('*'))
		{
			Result += bBoldOpen ? TEXT("</>") : TEXT("<b>");
			bBoldOpen = !bBoldOpen;
			++Index;
			continue;
		}

		if (Index + 1 < SourceText.Len()
			&& SourceText[Index] == TCHAR('-')
			&& SourceText[Index + 1] == TCHAR('-'))
		{
			Result += bStrikeOpen ? TEXT("</>") : TEXT("<s>");
			bStrikeOpen = !bStrikeOpen;
			Index += 2;
			continue;
		}

		Result.AppendChar(SourceText[Index]);
		++Index;
	}

	while (bStrikeOpen)
	{
		Result += TEXT("</>");
		bStrikeOpen = false;
	}
	while (bBoldOpen)
	{
		Result += TEXT("</>");
		bBoldOpen = false;
	}
	while (bItalicOpen)
	{
		Result += TEXT("</>");
		bItalicOpen = false;
	}
	while (bBoldItalicOpen)
	{
		Result += TEXT("</>");
		bBoldItalicOpen = false;
	}

	return Result;
}

static FText BuildFormattedDialogueLineText(
	const UParleyDialogueSubsystem* DialogueSubsystem,
	const TMap<FGameplayTag, FParleySpeakerRow>& SpeakerRowsByTag,
	const FDialogueRuntimeContext& Context,
	const FGameplayTag& ResolvedSpeakerTag,
	const FText& SourceLineText,
	FName& OutSpeakerLineFontStyleTag,
	TSoftObjectPtr<UFont>& OutSpeakerLineFont)
{
	OutSpeakerLineFontStyleTag = NAME_None;
	OutSpeakerLineFont = TSoftObjectPtr<UFont>();

	const FString RawLineText = SourceLineText.ToString();
	if (RawLineText.IsEmpty())
	{
		return SourceLineText;
	}

	FGameplayTag SpeakerRowTag;
	if (const FParleySpeakerRow* SpeakerRow = ResolveSpeakerRowForPresentation(SpeakerRowsByTag, ResolvedSpeakerTag, SpeakerRowTag))
	{
		OutSpeakerLineFont = SpeakerRow->LineFont;
		OutSpeakerLineFontStyleTag = SpeakerRow->LineFontStyleTag;
	}

	FString Formatted = ApplyDialogueLookupTokens(RawLineText, DialogueSubsystem, SpeakerRowsByTag, Context, ResolvedSpeakerTag);
	Formatted = ApplyDialogueFontMarkup(Formatted);
	Formatted = ApplyBasicDialogueStyleMarkup(Formatted);
	if (!OutSpeakerLineFontStyleTag.IsNone())
	{
		Formatted = FString::Printf(TEXT("<%s>%s</>"), *OutSpeakerLineFontStyleTag.ToString(), *Formatted);
	}

	if (Formatted.Equals(RawLineText, ESearchCase::CaseSensitive) && OutSpeakerLineFontStyleTag.IsNone())
	{
		return SourceLineText;
	}

	return FText::FromString(Formatted);
}

static const FDialogueCompiledNode* FindNodeById(const FParleyActiveDialogueSession& Session, const FGuid& NodeId)
{
	if (!Session.ConversationAsset)
	{
		return nullptr;
	}

	return Session.ConversationAsset->FindCompiledNode(NodeId);
}

static bool GetProgressionTagsForIdentity(
	const FParleyProgressionStore* ProgressionStore,
	const FParleyPlayerIdentity& Identity,
	FGameplayTagContainer& OutTags,
	const UWorld* World = nullptr)
{
	OutTags.Reset();
	if (!ProgressionStore)
	{
		return false;
	}

	const FDialoguePlayerPersistentState* PlayerState = FindPlayerDialogueState(ProgressionStore, Identity, World);
	if (!PlayerState)
	{
		return false;
	}

	OutTags = PlayerState->ProgressionTags;
	return true;
}

static bool EvaluateConditionGroupInternal(
	const UParleyDialogueSubsystem* DialogueSubsystem,
	const FDialogueConditionGroup& Group,
	const FDialogueRuntimeContext& Context,
	const bool bEmptyDefault)
{
	if (!DialogueSubsystem)
	{
		return false;
	}

	if (Group.Conditions.IsEmpty())
	{
		return bEmptyDefault;
	}

	if (Group.MatchMode == EDialogueConditionMatchMode::All)
	{
		for (const FDialogueCondition& Condition : Group.Conditions)
		{
			if (!DialogueSubsystem->EvaluateDialogueCondition(Condition, Context))
			{
				return false;
			}
		}
		return true;
	}

	for (const FDialogueCondition& Condition : Group.Conditions)
	{
		if (DialogueSubsystem->EvaluateDialogueCondition(Condition, Context))
		{
			return true;
		}
	}
	return false;
}

static bool PassesLockedConditions(const UParleyDialogueSubsystem* DialogueSubsystem, const FDialogueConditionGroup& Group, const FDialogueRuntimeContext& Context)
{
	return EvaluateConditionGroupInternal(DialogueSubsystem, Group, Context, true);
}

static bool PassesBlockedConditions(const UParleyDialogueSubsystem* DialogueSubsystem, const FDialogueConditionGroup& Group, const FDialogueRuntimeContext& Context)
{
	return !EvaluateConditionGroupInternal(DialogueSubsystem, Group, Context, false);
}

static TArray<FGameplayTag> GetAllControlledCharacters(const UWorld* World)
{
	TArray<FGameplayTag> Slots;
	if (!World)
	{
		return Slots;
	}

	const AGameStateBase* GameState = World->GetGameState<AGameStateBase>();
	if (!GameState)
	{
		return Slots;
	}

	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		const APlayerState* ARPlayerState = Cast<APlayerState>(PlayerState);
		if (!ARPlayerState)
		{
			continue;
		}

		const FGameplayTag Slot = GetCharacterTagFromPlayerState(ARPlayerState);
		if (Slot.IsValid())
		{
			Slots.AddUnique(Slot);
		}
	}

	return Slots;
}

static APlayerState* FindPlayerStateByCharacterTag(const UWorld* World, const FGameplayTag Slot)
{
	if (!World || !Slot.IsValid())
	{
		return nullptr;
	}

	const AGameStateBase* GameState = World->GetGameState<AGameStateBase>();
	if (!GameState)
	{
		return nullptr;
	}

	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		APlayerState* ARPlayerState = Cast<APlayerState>(PlayerState);
		if (ARPlayerState && GetCharacterTagFromPlayerState(ARPlayerState) == Slot)
		{
			return ARPlayerState;
		}
	}

	return nullptr;
}

static bool IsModeDialogueEnabled(const UParleyDialogueSettings* Settings, const FGameplayTag& ModeTag)
{
	if (!Settings || !ModeTag.IsValid())
	{
		return false;
	}

	return IsModeInContainer(ModeTag, Settings->SharedDialogueModeTags)
		|| IsModeInContainer(ModeTag, Settings->PerPlayerDialogueModeTags);
}

static bool IsBusySpeakerLockEnabled(const UParleyDialogueSettings* Settings, const FGameplayTag& ModeTag)
{
	return Settings
		&& Settings->bOnlyOneTalkerPerSpeakerInPerPlayerModes
		&& IsModeInContainer(ModeTag, Settings->PerPlayerDialogueModeTags);
}

static bool DoesSessionRejectEavesdrop(const FParleyActiveDialogueSession& Session)
{
	// Important choice flow can temporarily override privacy by forcing all viewers.
	return Session.bConversationPrivate && !Session.bChoiceRequiresAllViewers;
}

void UParleyDialogueSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UTagKeySubsystem>();

	RuntimeState.Reset(new FParleyDialogueRuntimeState());
	FParleyDialogueRuntimeState& Runtime = *RuntimeState.Get();
	Runtime.ProgressionMutator.Owner = this;
	Runtime.ProgressionMutator.ProgressionStore = &Runtime.ProgressionStoreState;
	Runtime.ConversationsByTag.Reset();
	Runtime.SpeakerRowsByTag.Reset();
	Runtime.ActiveSessions.Reset();
	Runtime.SeenByPlayerTransient.Reset();
	Runtime.SkippedByPlayerTransient.Reset();
	Runtime.SpeakerOfferCountsByPlayerTransient.Reset();
	Runtime.SeenByGameTransient.Reset();
	Runtime.EavesdropTargetByViewer.Reset();

	const UParleyDialogueSettings* Settings = GetDefault<UParleyDialogueSettings>();
	if (!Settings)
	{
		UE_LOG(ParleyLog, Warning, TEXT("[Dialogue] Dialogue settings unavailable; runtime initialization aborted."));
		return;
	}

	if (!Settings->ConversationDefinitionRootTag.IsValid())
	{
		UE_LOG(ParleyLog, Warning, TEXT("[Dialogue] ConversationDefinitionRootTag is invalid; conversation lookup cannot run."));
	}

	if (!Settings->SpeakerDefinitionRootTag.IsValid())
	{
		UE_LOG(ParleyLog, Warning, TEXT("[Dialogue] SpeakerDefinitionRootTag is invalid; speaker lookup cannot run."));
	}

	int32 ConversationsFromLookup = 0;
	if (UTagKeySubsystem* Lookup = GetLookupSubsystem(this))
	{
		UDataTable* ConversationTable = nullptr;
		FString LookupError;
		FGameplayTag MatchedRoot;
		const bool bHasConversationRoot = Settings->ConversationDefinitionRootTag.IsValid();

		// Prefer explicit row-struct routing when present; fall back to a direct root match.
		if (!Lookup->TryResolveDataTableForRowStruct(FParleyConversationAssetRow::StaticStruct(), ConversationTable, MatchedRoot, LookupError))
		{
			LookupError.Empty();
			if (bHasConversationRoot)
			{
				Lookup->TryResolveDataTableForRootTag(Settings->ConversationDefinitionRootTag, ConversationTable, LookupError);
			}
		}

		if (ConversationTable)
		{
			UE_LOG(ParleyLog, Log,
				TEXT("[Dialogue] Resolved conversation table '%s' (Root=%s, RowStruct=%s)."),
				*ConversationTable->GetName(),
				MatchedRoot.IsValid() ? *MatchedRoot.ToString() : *Settings->ConversationDefinitionRootTag.ToString(),
				*GetNameSafe(ConversationTable->GetRowStruct()));
			if (ConversationTable->GetRowStruct() != FParleyConversationAssetRow::StaticStruct())
			{
				UE_LOG(ParleyLog, Warning,
					TEXT("[Dialogue] TagKey conversation table '%s' row struct mismatch (%s); expected FParleyConversationAssetRow."),
					*ConversationTable->GetName(),
					*GetNameSafe(ConversationTable->GetRowStruct()));
			}
			else
			{
				for (const FName RowName : ConversationTable->GetRowNames())
				{
					const FParleyConversationAssetRow* Row = ConversationTable->FindRow<FParleyConversationAssetRow>(RowName, TEXT("DialogueConversationLookup"), false);
					if (!Row)
					{
						continue;
					}

					FGameplayTag RowConversationTag = Row->ConversationTag;
					if (!RowConversationTag.IsValid() && bHasConversationRoot)
					{
						RowConversationTag = BuildTagFromRootAndLeaf(Settings->ConversationDefinitionRootTag, RowName);
					}

					UParleyConversationAsset* Conversation = Row->Conversation.LoadSynchronous();
					if (!Conversation)
					{
						UE_LOG(ParleyLog, Warning,
							TEXT("[Dialogue] TagKey row '%s' missing conversation asset reference; skipping."),
							*RowName.ToString());
						continue;
					}

					if (AddConversationToRuntimeRegistry(
						Runtime.ConversationsByTag,
						Conversation,
						RowConversationTag,
						FString::Printf(TEXT("TagKey row '%s'"), *RowName.ToString())))
					{
						++ConversationsFromLookup;
					}
				}
			}
		}
		else if (!LookupError.IsEmpty())
		{
			UE_LOG(ParleyLog, Warning,
				TEXT("[Dialogue] No TagKey conversation table resolved for root '%s': %s"),
				*Settings->ConversationDefinitionRootTag.ToString(),
				*LookupError);
		}
		else
		{
			UE_LOG(ParleyLog, Warning,
				TEXT("[Dialogue] No TagKey conversation table resolved for root '%s' (empty lookup error)."),
				*Settings->ConversationDefinitionRootTag.ToString());
		}
	}
	else
	{
		UE_LOG(ParleyLog, Warning, TEXT("[Dialogue] TagKeySubsystem unavailable during dialogue initialization."));
	}

	if (UTagKeySubsystem* Lookup = GetLookupSubsystem(this))
	{
		UDataTable* SpeakerTable = nullptr;
		FGameplayTag MatchedRoot;
		FString LookupError;
		if (!Lookup->TryResolveDataTableForRowStruct(FParleySpeakerRow::StaticStruct(), SpeakerTable, MatchedRoot, LookupError))
		{
			LookupError.Empty();
			if (Settings->SpeakerDefinitionRootTag.IsValid())
			{
				Lookup->TryResolveDataTableForRootTag(Settings->SpeakerDefinitionRootTag, SpeakerTable, LookupError);
				MatchedRoot = Settings->SpeakerDefinitionRootTag;
			}
		}

		FGameplayTag EffectiveSpeakerRoot = MatchedRoot;
		if (!EffectiveSpeakerRoot.IsValid() && Settings->SpeakerDefinitionRootTag.IsValid())
		{
			EffectiveSpeakerRoot = Settings->SpeakerDefinitionRootTag;
		}

		if (SpeakerTable && SpeakerTable->GetRowStruct() == FParleySpeakerRow::StaticStruct())
		{
			for (const FName RowName : SpeakerTable->GetRowNames())
			{
				const FParleySpeakerRow* Typed = SpeakerTable->FindRow<FParleySpeakerRow>(RowName, TEXT("DialogueSpeakerLookup"), false);
				if (!Typed)
				{
					continue;
				}

				FParleySpeakerRow Row = *Typed;
				if (!Row.SpeakerTag.IsValid())
				{
					Row.SpeakerTag = BuildTagFromRootAndLeaf(EffectiveSpeakerRoot, RowName);
				}

				if (!Row.SpeakerTag.IsValid())
				{
					UE_LOG(ParleyLog, Verbose, TEXT("[Dialogue] Speaker row '%s' skipped: no valid SpeakerTag."), *RowName.ToString());
					continue;
				}

				if (Runtime.SpeakerRowsByTag.Contains(Row.SpeakerTag))
				{
					UE_LOG(ParleyLog, Error, TEXT("[Dialogue] Duplicate SpeakerTag '%s' resolved from speaker content rows; later duplicate ignored."),
						*Row.SpeakerTag.ToString());
					continue;
				}

				Runtime.SpeakerRowsByTag.Add(Row.SpeakerTag, Row);
			}
		}
		else if (!LookupError.IsEmpty())
		{
			UE_LOG(
				ParleyLog,
				Warning,
				TEXT("[Dialogue] Speaker table resolution failed for root '%s': %s"),
				*Settings->SpeakerDefinitionRootTag.ToString(),
				*LookupError);
		}
	}
	else
	{
		UE_LOG(ParleyLog, Warning, TEXT("[Dialogue] TagKeySubsystem unavailable during speaker initialization."));
	}

	if (Runtime.ConversationsByTag.IsEmpty())
	{
		UE_LOG(ParleyLog, Warning, TEXT("[Dialogue] No conversations registered from TagKey. Dialogue offering will fail."));
	}
	else
	{
		UE_LOG(ParleyLog, Log, TEXT("[Dialogue] Registered %d conversations (lookup rows loaded=%d) and %d speakers."),
			Runtime.ConversationsByTag.Num(),
			ConversationsFromLookup,
			Runtime.SpeakerRowsByTag.Num());
	}
}

void UParleyDialogueSubsystem::Deinitialize()
{
	RuntimeState.Reset();
	Super::Deinitialize();
}


#include "ParleyDialogueSubsystem_Mutations.inl"
#include "ParleyDialogueSubsystem_Validation.inl"
#include "ParleyDialogueSubsystem_Runtime.inl"
#include "ParleyDialogueSubsystem_API.inl"
