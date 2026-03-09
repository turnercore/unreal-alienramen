#include "ARDialogueSubsystem.h"

#include "ARContentLookupSettings.h"
#include "ARDialogueConversationAsset.h"
#include "ARDialogueSettings.h"
#include "ARFactionSubsystem.h"
#include "ARGameModeBase.h"
#include "ARGameStateBase.h"
#include "ARLog.h"
#include "ARNPCSubsystem.h"
#include "ARPlayerController.h"
#include "ARPlayerStateBase.h"
#include "ARSaveGame.h"
#include "ARSaveSubsystem.h"
#include "ContentLookupSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "GameplayTagsManager.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

namespace
{
	struct FDialogueCandidateEval
	{
		TObjectPtr<UARDialogueConversationAsset> Conversation = nullptr;
		int32 Priority = 0;
		bool bSeenByGame = false;
		bool bSeenByPlayer = false;
		bool bCompletedByGame = false;
		bool bCompletedByPlayer = false;
		bool bRepeatable = false;
	};

	static bool SortCandidatesByPriority(const FDialogueCandidateEval& Lhs, const FDialogueCandidateEval& Rhs)
	{
		return Lhs.Priority > Rhs.Priority;
	}

	struct FARActiveDialogueSession
	{
		FString SessionId;
		FGameplayTag ConversationTag;
		FGameplayTag PrimarySpeakerTag;
		TObjectPtr<UARDialogueConversationAsset> ConversationAsset = nullptr;
		FGuid CurrentNodeId;
		FGuid WaitingChoiceNodeId;
		EARPlayerSlot InitiatorSlot = EARPlayerSlot::Unknown;
		EARPlayerSlot OwnerSlot = EARPlayerSlot::Unknown;
		bool bIsSharedSession = false;
		bool bConversationImportant = false;
		bool bWaitingForChoice = false;
		bool bChoiceRequiresAllViewers = false;
		bool bWaitingForAdvanceInput = false;
		TArray<FDialogueChoiceView> CurrentChoices;
		FGameplayTag CurrentSpeakerTag;
		FText CurrentLineText;
		FSpeakerPortraitData CurrentSpeakerPortrait;
		FGuid WaitingLineNodeId;
		TMap<FGuid, FGuid> RuntimeChoiceSelections;
		FGameplayTagContainer TransientConversationTags;
		TSet<EARPlayerSlot> Participants;
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

	static AARPlayerStateBase* GetARPlayerStateFromController(const AARPlayerController* PC)
	{
		return PC ? PC->GetPlayerState<AARPlayerStateBase>() : nullptr;
	}

	static EARPlayerSlot GetSlotFromController(const AARPlayerController* PC)
	{
		const AARPlayerStateBase* PS = GetARPlayerStateFromController(PC);
		return PS ? PS->GetPlayerSlot() : EARPlayerSlot::Unknown;
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

	static FGameplayTag GetCurrentModeTag(const UWorld* World)
	{
		if (!World)
		{
			return FGameplayTag();
		}

		if (const AARGameModeBase* GM = World->GetAuthGameMode<AARGameModeBase>())
		{
			return GM->GetModeTag();
		}

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

struct UARDialogueSubsystem::FARDialogueRuntimeState
{
	TMap<FGameplayTag, TObjectPtr<UARDialogueConversationAsset>> ConversationsByTag;
	TMap<FGameplayTag, FARDialogueSpeakerRow> SpeakerRowsByTag;
	TArray<FARActiveDialogueSession> ActiveSessions;
	TMap<EARPlayerSlot, FGameplayTagContainer> SeenByPlayerTransient;
	FGameplayTagContainer SeenByGameTransient;
	TMap<EARPlayerSlot, EARPlayerSlot> EavesdropTargetByViewer;
};

static bool AddConversationToRuntimeRegistry(
	TMap<FGameplayTag, TObjectPtr<UARDialogueConversationAsset>>& ConversationsByTag,
	UARDialogueConversationAsset* Conversation,
	const FGameplayTag& ForcedConversationTag,
	const FString& SourceLabel)
{
	if (!Conversation)
	{
		UE_LOG(ARLog, Warning, TEXT("[Dialogue] Skipping null conversation from %s."), *SourceLabel);
		return false;
	}

	if (ForcedConversationTag.IsValid() && !Conversation->Header.ConversationTag.IsValid())
	{
		Conversation->Header.ConversationTag = ForcedConversationTag;
	}

	const FGameplayTag ConversationTag = Conversation->Header.ConversationTag;
	if (ForcedConversationTag.IsValid() && ConversationTag.IsValid() && !ConversationTag.MatchesTagExact(ForcedConversationTag))
	{
		UE_LOG(ARLog, Warning,
			TEXT("[Dialogue] Conversation '%s' tag mismatch: asset '%s' vs lookup '%s' (%s). Asset tag will be used."),
			*GetNameSafe(Conversation),
			*ConversationTag.ToString(),
			*ForcedConversationTag.ToString(),
			*SourceLabel);
	}

	if (!ConversationTag.IsValid())
	{
		UE_LOG(ARLog, Warning, TEXT("[Dialogue] Skipping conversation '%s' from %s: ConversationTag is invalid."),
			*GetNameSafe(Conversation),
			*SourceLabel);
		return false;
	}

	if (ConversationsByTag.Contains(ConversationTag))
	{
		UE_LOG(ARLog, Error, TEXT("[Dialogue] Duplicate ConversationTag '%s' from %s. Existing asset '%s' kept; duplicate '%s' ignored."),
			*ConversationTag.ToString(),
			*SourceLabel,
			*GetNameSafe(ConversationsByTag[ConversationTag]),
			*GetNameSafe(Conversation));
		return false;
	}

	if (!Conversation->Header.PrimarySpeakerTag.IsValid())
	{
		UE_LOG(ARLog, Warning,
			TEXT("[Dialogue] Conversation '%s' from %s has no PrimarySpeakerTag; it will still be registered but will fail offer gating."),
			*ConversationTag.ToString(),
			*SourceLabel);
	}

	ConversationsByTag.Add(ConversationTag, Conversation);
	UE_LOG(ARLog, Verbose,
		TEXT("[Dialogue] Registered conversation '%s' (PrimarySpeaker:%s, Priority:%d) from %s."),
		*ConversationTag.ToString(),
		*Conversation->Header.PrimarySpeakerTag.ToString(),
		Conversation->Header.Priority,
		*SourceLabel);
	return true;
}

UARDialogueSubsystem::FARDialogueRuntimeState& UARDialogueSubsystem::GetRuntimeState()
{
	if (!RuntimeState)
	{
		RuntimeState = new FARDialogueRuntimeState();
	}
	return *RuntimeState;
}

const UARDialogueSubsystem::FARDialogueRuntimeState& UARDialogueSubsystem::GetRuntimeState() const
{
	static const FARDialogueRuntimeState EmptyState;
	return RuntimeState ? *RuntimeState : EmptyState;
}

static UARSaveSubsystem* GetSaveSubsystem(const UARDialogueSubsystem* Subsystem)
{
	if (!Subsystem)
	{
		return nullptr;
	}
	if (UGameInstance* GI = Subsystem->GetGameInstance())
	{
		return GI->GetSubsystem<UARSaveSubsystem>();
	}
	return nullptr;
}

static UARSaveGame* GetCurrentSave(UARDialogueSubsystem* Subsystem)
{
	if (UARSaveSubsystem* SaveSubsystem = GetSaveSubsystem(Subsystem))
	{
		return SaveSubsystem->GetCurrentSaveGame();
	}
	return nullptr;
}

static const UARSaveGame* GetCurrentSave(const UARDialogueSubsystem* Subsystem)
{
	if (UARSaveSubsystem* SaveSubsystem = GetSaveSubsystem(Subsystem))
	{
		return SaveSubsystem->GetCurrentSaveGame();
	}
	return nullptr;
}

static UContentLookupSubsystem* GetLookupSubsystem(const UARDialogueSubsystem* Subsystem)
{
	if (!Subsystem)
	{
		return nullptr;
	}
	if (UGameInstance* GI = Subsystem->GetGameInstance())
	{
		return GI->GetSubsystem<UContentLookupSubsystem>();
	}
	return nullptr;
}

static FARPlayerIdentity BuildPlayerIdentityFromState(const AARPlayerStateBase* PS)
{
	FARPlayerIdentity Identity;
	if (!PS)
	{
		return Identity;
	}

	Identity.LegacyId = PS->GetPlayerId();
	Identity.DisplayName = FText::FromString(PS->GetDisplayNameValue());
	Identity.PlayerSlot = PS->GetPlayerSlot();
	if (PS->GetUniqueId().IsValid())
	{
		Identity.UniqueNetIdString = PS->GetUniqueId()->ToString();
		Identity.UniqueNetIdType = PS->GetUniqueId()->GetType().ToString();
	}
	return Identity;
}

static const FDialoguePlayerPersistentState* FindPlayerDialogueState(const UARSaveGame* SaveGame, const FARPlayerIdentity& Identity)
{
	if (!SaveGame)
	{
		return nullptr;
	}

	for (const FDialoguePlayerPersistentState& Entry : SaveGame->DialoguePlayerPersistentStates)
	{
		if (Entry.Identity.Matches(Identity))
		{
			return &Entry;
		}
	}

	return nullptr;
}

static bool IsConversationCompletedByGame(const UARDialogueSubsystem* Subsystem, const FGameplayTag ConversationTag)
{
	const UARSaveGame* SaveGame = GetCurrentSave(Subsystem);
	return SaveGame && ConversationTag.IsValid() && SaveGame->DialogueCompletedConversationTagsByGame.HasTagExact(ConversationTag);
}

static bool IsConversationCompletedByPlayer(const UARDialogueSubsystem* Subsystem, const FARPlayerIdentity& Identity, const FGameplayTag ConversationTag)
{
	const UARSaveGame* SaveGame = GetCurrentSave(Subsystem);
	if (!SaveGame || !ConversationTag.IsValid())
	{
		return false;
	}

	const FDialoguePlayerPersistentState* PlayerState = FindPlayerDialogueState(SaveGame, Identity);
	return PlayerState && PlayerState->CompletedConversationTags.HasTagExact(ConversationTag);
}

static FARActiveDialogueSession* FindSessionByOwnerSlot(TArray<FARActiveDialogueSession>& Sessions, const EARPlayerSlot Slot)
{
	for (FARActiveDialogueSession& Session : Sessions)
	{
		if (!Session.bIsSharedSession && Session.OwnerSlot == Slot)
		{
			return &Session;
		}
	}
	return nullptr;
}

static FARActiveDialogueSession* FindSharedSession(TArray<FARActiveDialogueSession>& Sessions)
{
	for (FARActiveDialogueSession& Session : Sessions)
	{
		if (Session.bIsSharedSession)
		{
			return &Session;
		}
	}
	return nullptr;
}

static FARActiveDialogueSession* FindSessionForSlot(TArray<FARActiveDialogueSession>& Sessions, const EARPlayerSlot Slot)
{
	for (FARActiveDialogueSession& Session : Sessions)
	{
		if (Session.Participants.Contains(Slot))
		{
			return &Session;
		}
	}
	return nullptr;
}

static const FARActiveDialogueSession* FindSessionForSlot(const TArray<FARActiveDialogueSession>& Sessions, const EARPlayerSlot Slot)
{
	for (const FARActiveDialogueSession& Session : Sessions)
	{
		if (Session.Participants.Contains(Slot))
		{
			return &Session;
		}
	}
	return nullptr;
}

static AARPlayerController* FindPlayerControllerBySlot(const UWorld* World, const EARPlayerSlot Slot)
{
	if (!World || Slot == EARPlayerSlot::Unknown)
	{
		return nullptr;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (AARPlayerController* PC = Cast<AARPlayerController>(It->Get()))
		{
			if (const AARPlayerStateBase* PS = PC->GetPlayerState<AARPlayerStateBase>())
			{
				if (PS->GetPlayerSlot() == Slot)
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

static FDialoguePlayerPersistentState* FindOrAddPlayerDialogueState(UARSaveGame* SaveGame, const FARPlayerIdentity& Identity)
{
	if (!SaveGame)
	{
		return nullptr;
	}

	for (FDialoguePlayerPersistentState& Entry : SaveGame->DialoguePlayerPersistentStates)
	{
		if (Entry.Identity.Matches(Identity))
		{
			return &Entry;
		}
	}

	FDialoguePlayerPersistentState& Added = SaveGame->DialoguePlayerPersistentStates.AddDefaulted_GetRef();
	Added.Identity = Identity;
	return &Added;
}

static FGameplayTag GetDialogueSpeakerPlayerPlaceholderTag()
{
	return UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Dialogue.Speaker.Player"), false);
}

static FGameplayTag GetDialogueSpeakerBrotherTag()
{
	return UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Dialogue.Speaker.Brother"), false);
}

static FGameplayTag GetDialogueSpeakerSisterTag()
{
	return UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Dialogue.Speaker.Sister"), false);
}

static FGameplayTag ResolvePlayerSpeakerTag(const AARPlayerStateBase* PlayerState)
{
	if (!PlayerState)
	{
		return FGameplayTag();
	}

	switch (PlayerState->GetCharacterPicked())
	{
	case EARCharacterChoice::Brother:
		return GetDialogueSpeakerBrotherTag();
	case EARCharacterChoice::Sister:
		return GetDialogueSpeakerSisterTag();
	default:
		break;
	}

	switch (PlayerState->GetPlayerSlot())
	{
	case EARPlayerSlot::P1:
		return GetDialogueSpeakerBrotherTag();
	case EARPlayerSlot::P2:
		return GetDialogueSpeakerSisterTag();
	default:
		return FGameplayTag();
	}
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

static const FARDialogueSpeakerRow* FindSpeakerRowByConversationSpeakerTag(
	const TMap<FGameplayTag, FARDialogueSpeakerRow>& SpeakerRowsByTag,
	const FGameplayTag& LineSpeakerTag,
	FGameplayTag& OutResolvedSpeakerRowTag)
{
	OutResolvedSpeakerRowTag = FGameplayTag();
	FGameplayTag Candidate = LineSpeakerTag;

	while (Candidate.IsValid())
	{
		if (const FARDialogueSpeakerRow* Found = SpeakerRowsByTag.Find(Candidate))
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

	const FGameplayTag PlayerTag = GetDialogueSpeakerPlayerPlaceholderTag();
	if (PlayerTag.IsValid() && SpeakerTag.MatchesTag(PlayerTag))
	{
		return true;
	}

	const FGameplayTag BrotherTag = GetDialogueSpeakerBrotherTag();
	if (BrotherTag.IsValid() && SpeakerTag.MatchesTag(BrotherTag))
	{
		return true;
	}

	const FGameplayTag SisterTag = GetDialogueSpeakerSisterTag();
	if (SisterTag.IsValid() && SpeakerTag.MatchesTag(SisterTag))
	{
		return true;
	}

	return false;
}

static bool IsResolvableConversationSpeakerTag(
	const TMap<FGameplayTag, FARDialogueSpeakerRow>& SpeakerRowsByTag,
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

static FSpeakerPortraitData ResolvePortraitForSpeaker(
	const TMap<FGameplayTag, FARDialogueSpeakerRow>& SpeakerRowsByTag,
	const FGameplayTag& LineSpeakerTag)
{
	FSpeakerPortraitData ResolvedPortrait;
	FGameplayTag SpeakerRowTag;
	const FARDialogueSpeakerRow* SpeakerRow = FindSpeakerRowByConversationSpeakerTag(SpeakerRowsByTag, LineSpeakerTag, SpeakerRowTag);
	if (!SpeakerRow && IsBuiltInDialogueSpeakerTag(LineSpeakerTag))
	{
		TArray<FGameplayTag> FallbackCandidates;
		const FGameplayTag PlayerTag = GetDialogueSpeakerPlayerPlaceholderTag();
		const FGameplayTag BrotherTag = GetDialogueSpeakerBrotherTag();
		const FGameplayTag SisterTag = GetDialogueSpeakerSisterTag();
		if (LineSpeakerTag.MatchesTag(BrotherTag))
		{
			FallbackCandidates = { PlayerTag, SisterTag };
		}
		else if (LineSpeakerTag.MatchesTag(SisterTag))
		{
			FallbackCandidates = { PlayerTag, BrotherTag };
		}
		else
		{
			FallbackCandidates = { BrotherTag, SisterTag };
		}

		for (const FGameplayTag CandidateTag : FallbackCandidates)
		{
			if (!CandidateTag.IsValid())
			{
				continue;
			}

			SpeakerRow = FindSpeakerRowByConversationSpeakerTag(SpeakerRowsByTag, CandidateTag, SpeakerRowTag);
			if (SpeakerRow)
			{
				break;
			}
		}
	}
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

static const FDialogueCompiledNode* FindNodeById(const FARActiveDialogueSession& Session, const FGuid& NodeId)
{
	if (!Session.ConversationAsset)
	{
		return nullptr;
	}

	return Session.ConversationAsset->FindCompiledNode(NodeId);
}

static bool GetProgressionTagsForIdentity(const UARSaveGame* SaveGame, const FARPlayerIdentity& Identity, FGameplayTagContainer& OutTags)
{
	OutTags.Reset();
	if (!SaveGame)
	{
		return false;
	}

	const FDialoguePlayerPersistentState* PlayerState = FindPlayerDialogueState(SaveGame, Identity);
	if (!PlayerState)
	{
		return false;
	}

	OutTags = PlayerState->ProgressionTags;
	return true;
}

static bool EvaluateConditionGroupInternal(
	const UARDialogueSubsystem* DialogueSubsystem,
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

static bool PassesLockedConditions(const UARDialogueSubsystem* DialogueSubsystem, const FDialogueConditionGroup& Group, const FDialogueRuntimeContext& Context)
{
	return EvaluateConditionGroupInternal(DialogueSubsystem, Group, Context, true);
}

static bool PassesBlockedConditions(const UARDialogueSubsystem* DialogueSubsystem, const FDialogueConditionGroup& Group, const FDialogueRuntimeContext& Context)
{
	return !EvaluateConditionGroupInternal(DialogueSubsystem, Group, Context, false);
}

static TArray<EARPlayerSlot> GetAllSlottedPlayers(const UWorld* World)
{
	TArray<EARPlayerSlot> Slots;
	if (!World)
	{
		return Slots;
	}

	const AARGameStateBase* GameState = World->GetGameState<AARGameStateBase>();
	if (!GameState)
	{
		return Slots;
	}

	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		const AARPlayerStateBase* ARPlayerState = Cast<AARPlayerStateBase>(PlayerState);
		if (!ARPlayerState)
		{
			continue;
		}

		const EARPlayerSlot Slot = ARPlayerState->GetPlayerSlot();
		if (Slot != EARPlayerSlot::Unknown)
		{
			Slots.AddUnique(Slot);
		}
	}

	return Slots;
}

static AARPlayerStateBase* FindPlayerStateBySlot(const UWorld* World, const EARPlayerSlot Slot)
{
	if (!World || Slot == EARPlayerSlot::Unknown)
	{
		return nullptr;
	}

	const AARGameStateBase* GameState = World->GetGameState<AARGameStateBase>();
	if (!GameState)
	{
		return nullptr;
	}

	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		AARPlayerStateBase* ARPlayerState = Cast<AARPlayerStateBase>(PlayerState);
		if (ARPlayerState && ARPlayerState->GetPlayerSlot() == Slot)
		{
			return ARPlayerState;
		}
	}

	return nullptr;
}

static bool IsModeDialogueEnabled(const UARDialogueSettings* Settings, const FGameplayTag& ModeTag)
{
	if (!Settings || !ModeTag.IsValid())
	{
		return false;
	}

	return IsModeInContainer(ModeTag, Settings->SharedDialogueModeTags)
		|| IsModeInContainer(ModeTag, Settings->PerPlayerDialogueModeTags);
}

void UARDialogueSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UContentLookupSubsystem>();

	FARDialogueRuntimeState& Runtime = GetRuntimeState();
	Runtime.ConversationsByTag.Reset();
	Runtime.SpeakerRowsByTag.Reset();

	const UARDialogueSettings* Settings = GetDefault<UARDialogueSettings>();
	if (!Settings)
	{
		UE_LOG(ARLog, Warning, TEXT("[Dialogue] Dialogue settings unavailable; runtime initialization aborted."));
		return;
	}

	if (!Settings->ConversationDefinitionRootTag.IsValid())
	{
		UE_LOG(ARLog, Warning, TEXT("[Dialogue] ConversationDefinitionRootTag is invalid; conversation lookup cannot run."));
	}

	if (!Settings->SpeakerDefinitionRootTag.IsValid())
	{
		UE_LOG(ARLog, Warning, TEXT("[Dialogue] SpeakerDefinitionRootTag is invalid; speaker lookup cannot run."));
	}

	int32 ConversationsFromLookup = 0;
	if (UContentLookupSubsystem* Lookup = GetLookupSubsystem(this))
	{
		UDataTable* ConversationTable = nullptr;
		FString LookupError;
		FGameplayTag MatchedRoot;
		const bool bHasConversationRoot = Settings->ConversationDefinitionRootTag.IsValid();

		// Prefer explicit row-struct routing when present; fall back to a direct root match.
		if (!Lookup->GetDataTableForRowStruct(FARDialogueConversationAssetRow::StaticStruct(), ConversationTable, MatchedRoot, LookupError))
		{
			LookupError.Empty();
			if (bHasConversationRoot)
			{
				Lookup->GetDataTableForRootTag(Settings->ConversationDefinitionRootTag, ConversationTable, LookupError);
			}
		}

		if (ConversationTable)
		{
			UE_LOG(ARLog, Log,
				TEXT("[Dialogue] Resolved conversation table '%s' (Root=%s, RowStruct=%s)."),
				*ConversationTable->GetName(),
				MatchedRoot.IsValid() ? *MatchedRoot.ToString() : *Settings->ConversationDefinitionRootTag.ToString(),
				*GetNameSafe(ConversationTable->GetRowStruct()));
			if (ConversationTable->GetRowStruct() != FARDialogueConversationAssetRow::StaticStruct())
			{
				UE_LOG(ARLog, Warning,
					TEXT("[Dialogue] ContentLookup conversation table '%s' row struct mismatch (%s); expected FARDialogueConversationAssetRow."),
					*ConversationTable->GetName(),
					*GetNameSafe(ConversationTable->GetRowStruct()));
			}
			else
			{
				for (const FName RowName : ConversationTable->GetRowNames())
				{
					const FARDialogueConversationAssetRow* Row = ConversationTable->FindRow<FARDialogueConversationAssetRow>(RowName, TEXT("DialogueConversationLookup"), false);
					if (!Row)
					{
						continue;
					}

					FGameplayTag RowConversationTag = Row->ConversationTag;
					if (!RowConversationTag.IsValid() && bHasConversationRoot)
					{
						RowConversationTag = BuildTagFromRootAndLeaf(Settings->ConversationDefinitionRootTag, RowName);
					}

					UARDialogueConversationAsset* Conversation = Row->Conversation.LoadSynchronous();
					if (!Conversation)
					{
						UE_LOG(ARLog, Warning,
							TEXT("[Dialogue] ContentLookup row '%s' missing conversation asset reference; skipping."),
							*RowName.ToString());
						continue;
					}

					if (AddConversationToRuntimeRegistry(
						Runtime.ConversationsByTag,
						Conversation,
						RowConversationTag,
						FString::Printf(TEXT("ContentLookup row '%s'"), *RowName.ToString())))
					{
						++ConversationsFromLookup;
					}
				}
			}
		}
		else if (!LookupError.IsEmpty())
		{
			UE_LOG(ARLog, Warning,
				TEXT("[Dialogue] No ContentLookup conversation table resolved for root '%s': %s"),
				*Settings->ConversationDefinitionRootTag.ToString(),
				*LookupError);
		}
		else
		{
			UE_LOG(ARLog, Warning,
				TEXT("[Dialogue] No ContentLookup conversation table resolved for root '%s' (empty lookup error)."),
				*Settings->ConversationDefinitionRootTag.ToString());
		}
	}
	else
	{
		UE_LOG(ARLog, Warning, TEXT("[Dialogue] ContentLookupSubsystem unavailable during dialogue initialization."));
	}

	if (UContentLookupSubsystem* Lookup = GetLookupSubsystem(this))
	{
		TArray<FName> RowNames;
		FString Error;
		if (Lookup->GetAllRowNamesForRootTag(Settings->SpeakerDefinitionRootTag, RowNames, Error))
		{
			for (const FName RowName : RowNames)
			{
				const FGameplayTag CandidateTag = BuildTagFromRootAndLeaf(Settings->SpeakerDefinitionRootTag, RowName);
				if (!CandidateTag.IsValid())
				{
					continue;
				}

				FInstancedStruct RowData;
				FString LookupError;
				if (!Lookup->LookupWithGameplayTag(CandidateTag, RowData, LookupError))
				{
					continue;
				}

				if (const FARDialogueSpeakerRow* Typed = RowData.GetPtr<FARDialogueSpeakerRow>())
				{
					FARDialogueSpeakerRow Row = *Typed;
					if (!Row.SpeakerTag.IsValid())
					{
						Row.SpeakerTag = CandidateTag;
					}

					if (Runtime.SpeakerRowsByTag.Contains(Row.SpeakerTag))
					{
						UE_LOG(ARLog, Error, TEXT("[Dialogue] Duplicate SpeakerTag '%s' resolved from speaker content rows; later duplicate ignored."),
							*Row.SpeakerTag.ToString());
						continue;
					}

					Runtime.SpeakerRowsByTag.Add(Row.SpeakerTag, Row);
				}
			}
		}
	}
	else
	{
		UE_LOG(ARLog, Warning, TEXT("[Dialogue] ContentLookupSubsystem unavailable during speaker initialization."));
	}

	if (Runtime.ConversationsByTag.IsEmpty())
	{
		UE_LOG(ARLog, Warning, TEXT("[Dialogue] No conversations registered from ContentLookup. Dialogue offering will fail."));
	}
	else
	{
		UE_LOG(ARLog, Log, TEXT("[Dialogue] Registered %d conversations (lookup) and %d speakers."),
			Runtime.ConversationsByTag.Num(),
			ConversationsFromLookup,
			Runtime.SpeakerRowsByTag.Num());
	}
}

void UARDialogueSubsystem::Deinitialize()
{
	delete RuntimeState;
	RuntimeState = nullptr;
	Super::Deinitialize();
}

FGameplayTagContainer UARDialogueSubsystem::GetCombinedDialogueTags(const FGameplayTagContainer& PlayerOnlyProgressionTags, const FGameplayTagContainer& GameOnlyProgressionTags) const
{
	FGameplayTagContainer Combined = PlayerOnlyProgressionTags;
	for (const FGameplayTag Tag : GameOnlyProgressionTags)
	{
		Combined.AddTag(Tag);
	}
	return Combined;
}

bool UARDialogueSubsystem::EvaluateDialogueCondition(const FDialogueCondition& Condition, const FDialogueRuntimeContext& Context) const
{
	switch (Condition.Source)
	{
	case EDialogueConditionSource::CombinedTags:
		return EvaluateTagContainerCondition(Context.CombinedProgressionTags, Condition);
	case EDialogueConditionSource::PlayerTags:
		return EvaluateTagContainerCondition(Context.PlayerOnlyProgressionTags, Condition);
	case EDialogueConditionSource::GameTags:
		return EvaluateTagContainerCondition(Context.GameOnlyProgressionTags, Condition);
	case EDialogueConditionSource::TransientConversationTags:
		return EvaluateTagContainerCondition(Context.TransientConversationTags, Condition);
	case EDialogueConditionSource::ActiveCharacter:
		if (Condition.Operator == EDialogueComparisonOp::Equals || Condition.Operator == EDialogueComparisonOp::Contains || Condition.Operator == EDialogueComparisonOp::Present)
		{
			return Context.ResolvedPlayerSpeakerTag.MatchesTagExact(Condition.TagValue);
		}
		if (Condition.Operator == EDialogueComparisonOp::NotEquals || Condition.Operator == EDialogueComparisonOp::NotContains || Condition.Operator == EDialogueComparisonOp::Absent)
		{
			return !Context.ResolvedPlayerSpeakerTag.MatchesTagExact(Condition.TagValue);
		}
		return false;
	case EDialogueConditionSource::RelationshipPoints:
		return CompareNumeric(Context.RelationshipPointsForPrimarySpeaker, Condition.Operator, Condition.NumericValue);
	case EDialogueConditionSource::RelationshipLevel:
		return CompareNumeric(static_cast<float>(Context.RelationshipLevelForPrimarySpeaker), Condition.Operator, Condition.NumericValue);
	case EDialogueConditionSource::SeenByPlayer:
		return CompareBool(Context.bSeenByPlayer, Condition.Operator, Condition.NumericValue > 0.0f);
	case EDialogueConditionSource::SeenByGame:
		return CompareBool(Context.bSeenByGame, Condition.Operator, Condition.NumericValue > 0.0f);
	case EDialogueConditionSource::CompletedByPlayer:
		return CompareBool(Context.bCompletedByPlayer, Condition.Operator, Condition.NumericValue > 0.0f);
	case EDialogueConditionSource::CompletedByGame:
		return CompareBool(Context.bCompletedByGame, Condition.Operator, Condition.NumericValue > 0.0f);
	case EDialogueConditionSource::PlayerKills:
		return CompareNumeric(static_cast<float>(Context.PlayerKills), Condition.Operator, Condition.NumericValue);
	case EDialogueConditionSource::TimePlayed:
		return CompareNumeric(Context.TimePlayed, Condition.Operator, Condition.NumericValue);
	case EDialogueConditionSource::Loadout:
		return EvaluateTagContainerCondition(Context.LoadoutView.LoadoutTags, Condition);
	case EDialogueConditionSource::InjectedVariable:
	{
		const FDialogueInjectedValue* Injected = Context.InjectedVariables.Find(Condition.VariableName);
		if (!Injected)
		{
			return Condition.Operator == EDialogueComparisonOp::Absent;
		}

		switch (Injected->ValueType)
		{
		case EDialogueInjectedValueType::Bool:
			return CompareBool(Injected->BoolValue, Condition.Operator, Condition.InjectedValue.BoolValue);
		case EDialogueInjectedValueType::Integer:
			return CompareNumeric(static_cast<float>(Injected->IntValue), Condition.Operator, static_cast<float>(Condition.InjectedValue.IntValue));
		case EDialogueInjectedValueType::Float:
			return CompareNumeric(Injected->FloatValue, Condition.Operator, Condition.InjectedValue.FloatValue);
		case EDialogueInjectedValueType::Tag:
		{
			FGameplayTagContainer Temp;
			if (Injected->TagValue.IsValid())
			{
				Temp.AddTag(Injected->TagValue);
			}
			return EvaluateTagContainerCondition(Temp, Condition);
		}
		case EDialogueInjectedValueType::Text:
		{
			const FString L = Injected->TextValue.ToString();
			const FString R = Condition.InjectedValue.TextValue.ToString();
			switch (Condition.Operator)
			{
			case EDialogueComparisonOp::Equals:
				return L.Equals(R, ESearchCase::CaseSensitive);
			case EDialogueComparisonOp::NotEquals:
				return !L.Equals(R, ESearchCase::CaseSensitive);
			case EDialogueComparisonOp::Contains:
				return L.Contains(R, ESearchCase::CaseSensitive);
			case EDialogueComparisonOp::NotContains:
				return !L.Contains(R, ESearchCase::CaseSensitive);
			case EDialogueComparisonOp::Present:
				return !L.IsEmpty();
			case EDialogueComparisonOp::Absent:
				return L.IsEmpty();
			default:
				return false;
			}
		}
		default:
			return false;
		}
	}
	default:
		return false;
	}
}

bool UARDialogueSubsystem::ApplyDialogueTagMutation(const FDialogueTagMutation& Mutation, const FDialogueRuntimeContext& Context)
{
	if (!Mutation.Tag.IsValid())
	{
		UE_LOG(ARLog, Verbose, TEXT("[Dialogue] Tag mutation skipped: invalid tag."));
		return false;
	}

	switch (Mutation.Target)
	{
	case EDialogueTagMutationTarget::ActivePlayerTransientConversation:
	{
		const AARPlayerStateBase* ActivePS = Cast<AARPlayerStateBase>(Context.ActivePlayerState);
		if (!ActivePS)
		{
			UE_LOG(ARLog, Verbose, TEXT("[Dialogue] Tag mutation skipped: no active player state for transient mutation."));
			return false;
		}

		FARDialogueRuntimeState& Runtime = GetRuntimeState();
		FARActiveDialogueSession* ActiveSession = FindSessionForSlot(Runtime.ActiveSessions, ActivePS->GetPlayerSlot());
		if (!ActiveSession)
		{
			UE_LOG(ARLog, Verbose, TEXT("[Dialogue] Tag mutation skipped: no active session for transient mutation."));
			return false;
		}

		if (Mutation.Operation == EDialogueTagMutationOp::Add)
		{
			ActiveSession->TransientConversationTags.AddTag(Mutation.Tag);
		}
		else
		{
			ActiveSession->TransientConversationTags.RemoveTag(Mutation.Tag);
		}

		return true;
	}
	case EDialogueTagMutationTarget::GameStateProgression:
	case EDialogueTagMutationTarget::ActivePlayerProgression:
		break;
	default:
		UE_LOG(ARLog, Warning, TEXT("[Dialogue] Tag mutation skipped: unsupported target %d."), static_cast<int32>(Mutation.Target));
		return false;
	}

	UARSaveSubsystem* SaveSubsystem = GetSaveSubsystem(this);
	UARSaveGame* SaveGame = GetCurrentSave(this);
	if (!SaveSubsystem || !SaveGame)
	{
		UE_LOG(ARLog, Warning, TEXT("[Dialogue] Tag mutation failed: save subsystem or save game unavailable."));
		return false;
	}

	switch (Mutation.Target)
	{
	case EDialogueTagMutationTarget::GameStateProgression:
		return Mutation.Operation == EDialogueTagMutationOp::Add
			? SaveSubsystem->AddProgressionTag(Mutation.Tag)
			: SaveSubsystem->RemoveProgressionTag(Mutation.Tag);
	case EDialogueTagMutationTarget::ActivePlayerProgression:
	{
		const AARPlayerStateBase* ActivePS = Cast<AARPlayerStateBase>(Context.ActivePlayerState);
		if (!ActivePS)
		{
			UE_LOG(ARLog, Verbose, TEXT("[Dialogue] Tag mutation skipped: no active player state for progression mutation."));
			return false;
		}

		if (FDialoguePlayerPersistentState* PlayerState = FindOrAddPlayerDialogueState(SaveGame, BuildPlayerIdentityFromState(ActivePS)))
		{
			if (Mutation.Operation == EDialogueTagMutationOp::Add)
			{
				PlayerState->ProgressionTags.AddTag(Mutation.Tag);
			}
			else
			{
				PlayerState->ProgressionTags.RemoveTag(Mutation.Tag);
			}
			SaveSubsystem->MarkSaveDirty();
			return true;
		}
		return false;
	}
	default:
		return false;
	}
}

bool UARDialogueSubsystem::ApplyDialogueRelationshipMutation(const FDialogueRelationshipMutationNodeData& Mutation, const FDialogueRuntimeContext& Context)
{
	UARSaveSubsystem* SaveSubsystem = GetSaveSubsystem(this);
	UARSaveGame* SaveGame = GetCurrentSave(this);
	if (!SaveSubsystem || !SaveGame)
	{
		UE_LOG(ARLog, Warning, TEXT("[Dialogue] Relationship mutation failed: save subsystem or save game unavailable."));
		return false;
	}

	const FGameplayTag TargetSpeakerTag = Mutation.TargetSpeakerTag.IsValid() ? Mutation.TargetSpeakerTag : Context.PrimarySpeakerTag;
	if (!TargetSpeakerTag.IsValid())
	{
		UE_LOG(ARLog, Verbose, TEXT("[Dialogue] Relationship mutation skipped: target speaker tag invalid."));
		return false;
	}

	FDialogueRelationshipState* RelationshipState = nullptr;
	for (FDialogueRelationshipState& Entry : SaveGame->DialogueRelationshipStates)
	{
		if (Entry.SpeakerTag.MatchesTagExact(TargetSpeakerTag))
		{
			RelationshipState = &Entry;
			break;
		}
	}
	if (!RelationshipState)
	{
		FDialogueRelationshipState& Added = SaveGame->DialogueRelationshipStates.AddDefaulted_GetRef();
		Added.SpeakerTag = TargetSpeakerTag;
		RelationshipState = &Added;
	}

	RelationshipState->RelationshipPoints += Mutation.DeltaPoints;
	SaveSubsystem->MarkSaveDirty();
	return true;
}

bool UARDialogueSubsystem::ApplyDialogueFactionMutation(const FDialogueFactionMutationNodeData& Mutation, const FDialogueRuntimeContext& Context)
{
	(void)Context;
	if (!Mutation.FactionTag.IsValid())
	{
		UE_LOG(ARLog, Verbose, TEXT("[Dialogue] Faction mutation skipped: invalid faction tag."));
		return false;
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UARFactionSubsystem* FactionSubsystem = GI->GetSubsystem<UARFactionSubsystem>())
		{
			return FactionSubsystem->ModifyFactionPopularity(Mutation.FactionTag, Mutation.DeltaPopularity);
		}
	}
	UE_LOG(ARLog, Warning, TEXT("[Dialogue] Faction mutation failed: faction subsystem unavailable."));
	return false;
}

bool UARDialogueSubsystem::ValidateSpeaker(const FARDialogueSpeakerRow& SpeakerRow, FDialogueValidationReport& OutReport) const
{
	OutReport = FDialogueValidationReport();
	auto Add = [&OutReport](EDialogueValidationSeverity Severity, const FString& Message)
	{
		FDialogueValidationIssue& Issue = OutReport.Issues.AddDefaulted_GetRef();
		Issue.Severity = Severity;
		Issue.Message = FText::FromString(Message);
	};

	if (!SpeakerRow.SpeakerTag.IsValid()) { Add(EDialogueValidationSeverity::Error, TEXT("SpeakerTag is required.")); }
	if (SpeakerRow.DisplayName.IsEmpty()) { Add(EDialogueValidationSeverity::Error, TEXT("DisplayName is required.")); }
	if (SpeakerRow.DefaultPortrait.PortraitTexture.IsNull()) { Add(EDialogueValidationSeverity::Error, TEXT("DefaultPortrait is required.")); }
	if (SpeakerRow.RelationshipThresholds.IsEmpty()) { Add(EDialogueValidationSeverity::Error, TEXT("RelationshipThresholds must not be empty.")); }

	float Last = -FLT_MAX;
	for (const float Value : SpeakerRow.RelationshipThresholds)
	{
		if (Value <= Last)
		{
			Add(EDialogueValidationSeverity::Error, TEXT("RelationshipThresholds must be strictly ascending."));
			break;
		}
		Last = Value;
	}

	TSet<FGameplayTag> SeenPortraitTags;
	for (const FSpeakerPortraitEntry& Entry : SpeakerRow.Portraits)
	{
		if (!Entry.PortraitTag.IsValid())
		{
			continue;
		}
		if (SeenPortraitTags.Contains(Entry.PortraitTag))
		{
			Add(EDialogueValidationSeverity::Error, FString::Printf(TEXT("Duplicate portrait tag '%s'."), *Entry.PortraitTag.ToString()));
		}
		SeenPortraitTags.Add(Entry.PortraitTag);
	}

	if (SpeakerRow.SpeakerTag.IsValid())
	{
		// Duplicate speaker-tag validation must work outside initialized runtime state.
		int32 MatchingSpeakerTagCount = 0;
		const UARDialogueSettings* DialogueSettings = GetDefault<UARDialogueSettings>();
		const UARContentLookupSettings* LookupSettings = GetDefault<UARContentLookupSettings>();
		UContentLookupRegistry* Registry = LookupSettings ? LookupSettings->RegistryAsset.LoadSynchronous() : nullptr;
		if (DialogueSettings && Registry && DialogueSettings->SpeakerDefinitionRootTag.IsValid())
		{
			const FContentLookupRoute* SpeakerRoute = Registry->Routes.FindByPredicate([DialogueSettings](const FContentLookupRoute& Route)
			{
				return Route.RootTag.MatchesTagExact(DialogueSettings->SpeakerDefinitionRootTag);
			});
			UDataTable* SpeakerTable = SpeakerRoute ? SpeakerRoute->DataTable.LoadSynchronous() : nullptr;
			if (SpeakerTable && SpeakerTable->GetRowStruct() == FARDialogueSpeakerRow::StaticStruct())
			{
				for (const FName RowName : SpeakerTable->GetRowNames())
				{
					const FARDialogueSpeakerRow* Row = SpeakerTable->FindRow<FARDialogueSpeakerRow>(RowName, TEXT("DialogueSpeakerValidate"), false);
					if (!Row)
					{
						continue;
					}

					FGameplayTag CandidateTag = Row->SpeakerTag;
					if (!CandidateTag.IsValid())
					{
						CandidateTag = BuildTagFromRootAndLeaf(DialogueSettings->SpeakerDefinitionRootTag, RowName);
					}

					if (CandidateTag.IsValid() && CandidateTag.MatchesTagExact(SpeakerRow.SpeakerTag))
					{
						++MatchingSpeakerTagCount;
					}
				}
			}
		}
		if (MatchingSpeakerTagCount > 1)
		{
			Add(EDialogueValidationSeverity::Error, FString::Printf(TEXT("Duplicate speaker tag '%s' detected (%d rows)."), *SpeakerRow.SpeakerTag.ToString(), MatchingSpeakerTagCount));
		}

		const FString DefaultPortraitPath = FString::Printf(TEXT("%s.Default"), *SpeakerRow.SpeakerTag.ToString());
		const FGameplayTag DefaultPortraitTag = UGameplayTagsManager::Get().RequestGameplayTag(FName(*DefaultPortraitPath), false);
		bool bFoundPortraitDefault = false;
		for (const FSpeakerPortraitEntry& Entry : SpeakerRow.Portraits)
		{
			if (DefaultPortraitTag.IsValid() && Entry.PortraitTag.MatchesTagExact(DefaultPortraitTag))
			{
				bFoundPortraitDefault = true;
				break;
			}
		}

		if (!bFoundPortraitDefault && SpeakerRow.DefaultPortrait.PortraitTexture.IsNull())
		{
			Add(EDialogueValidationSeverity::Error, TEXT("No portrait fallback can be resolved (.Default entry missing and DefaultPortrait not set)."));
		}
	}

	return !OutReport.HasErrors();
}

namespace
{
	static bool IsTagSource(const EDialogueConditionSource Source)
	{
		switch (Source)
		{
		case EDialogueConditionSource::CombinedTags:
		case EDialogueConditionSource::PlayerTags:
		case EDialogueConditionSource::GameTags:
		case EDialogueConditionSource::TransientConversationTags:
		case EDialogueConditionSource::ActiveCharacter:
		case EDialogueConditionSource::Loadout:
			return true;
		default:
			return false;
		}
	}

	static bool IsNumericSource(const EDialogueConditionSource Source)
	{
		switch (Source)
		{
		case EDialogueConditionSource::RelationshipPoints:
		case EDialogueConditionSource::RelationshipLevel:
		case EDialogueConditionSource::PlayerKills:
		case EDialogueConditionSource::TimePlayed:
			return true;
		default:
			return false;
		}
	}

	static bool IsBoolSource(const EDialogueConditionSource Source)
	{
		switch (Source)
		{
		case EDialogueConditionSource::SeenByPlayer:
		case EDialogueConditionSource::SeenByGame:
		case EDialogueConditionSource::CompletedByPlayer:
		case EDialogueConditionSource::CompletedByGame:
			return true;
		default:
			return false;
		}
	}

	static bool IsTagOperator(const EDialogueComparisonOp Op)
	{
		switch (Op)
		{
		case EDialogueComparisonOp::Equals:
		case EDialogueComparisonOp::NotEquals:
		case EDialogueComparisonOp::Contains:
		case EDialogueComparisonOp::NotContains:
		case EDialogueComparisonOp::Present:
		case EDialogueComparisonOp::Absent:
			return true;
		default:
			return false;
		}
	}

	static bool IsNumericOperator(const EDialogueComparisonOp Op)
	{
		switch (Op)
		{
		case EDialogueComparisonOp::Equals:
		case EDialogueComparisonOp::NotEquals:
		case EDialogueComparisonOp::GreaterThan:
		case EDialogueComparisonOp::GreaterOrEqual:
		case EDialogueComparisonOp::LessThan:
		case EDialogueComparisonOp::LessOrEqual:
			return true;
		default:
			return false;
		}
	}

	static bool IsBoolOperator(const EDialogueComparisonOp Op)
	{
		switch (Op)
		{
		case EDialogueComparisonOp::Equals:
		case EDialogueComparisonOp::NotEquals:
		case EDialogueComparisonOp::Present:
		case EDialogueComparisonOp::Absent:
			return true;
		default:
			return false;
		}
	}

	static FString BuildInjectedValueKey(const FDialogueInjectedValue& Value)
	{
		switch (Value.ValueType)
		{
		case EDialogueInjectedValueType::Bool:
			return FString::Printf(TEXT("B:%d"), Value.BoolValue ? 1 : 0);
		case EDialogueInjectedValueType::Integer:
			return FString::Printf(TEXT("I:%d"), Value.IntValue);
		case EDialogueInjectedValueType::Float:
			return FString::Printf(TEXT("F:%s"), *FString::SanitizeFloat(Value.FloatValue));
		case EDialogueInjectedValueType::Tag:
			return FString::Printf(TEXT("T:%s"), *Value.TagValue.ToString());
		case EDialogueInjectedValueType::Text:
			return FString::Printf(TEXT("X:%s"), *Value.TextValue.ToString());
		default:
			break;
		}

		return TEXT("N");
	}

	static FString BuildConditionKey(const FDialogueCondition& Condition)
	{
		return FString::Printf(
			TEXT("Src=%d|Op=%d|Tag=%s|Num=%s|Var=%s|Inj=%s"),
			static_cast<int32>(Condition.Source),
			static_cast<int32>(Condition.Operator),
			*Condition.TagValue.ToString(),
			*FString::SanitizeFloat(Condition.NumericValue),
			*Condition.VariableName.ToString(),
			*BuildInjectedValueKey(Condition.InjectedValue));
	}

	static FString BuildConditionGroupKey(const FDialogueConditionGroup& Group)
	{
		TArray<FString> ConditionKeys;
		ConditionKeys.Reserve(Group.Conditions.Num());
		for (const FDialogueCondition& Condition : Group.Conditions)
		{
			ConditionKeys.Add(BuildConditionKey(Condition));
		}
		ConditionKeys.Sort();

		FString Result = FString::Printf(TEXT("Mode=%d|Count=%d"), static_cast<int32>(Group.MatchMode), ConditionKeys.Num());
		for (const FString& Key : ConditionKeys)
		{
			Result += TEXT("|");
			Result += Key;
		}
		return Result;
	}

	static FString BuildConversationOfferGatingSignature(const FDialogueConversationHeader& Header)
	{
		return FString::Printf(
			TEXT("Speaker=%s|Pri=%d|MinRel=%s|Repeat=%d|SeenG=%d|SeenP=%d|DoneG=%d|Lock=%s|Block=%s"),
			*Header.PrimarySpeakerTag.ToString(),
			Header.Priority,
			*FString::SanitizeFloat(Header.MinimumRelationshipPoints),
			Header.bRepeatable ? 1 : 0,
			Header.bSeenByGameBlocksReoffer ? 1 : 0,
			Header.bSeenByPlayerBlocksReoffer ? 1 : 0,
			Header.bCompletedByGameBlocksReoffer ? 1 : 0,
			*BuildConditionGroupKey(Header.LockedConditions),
			*BuildConditionGroupKey(Header.BlockedConditions));
	}
}

bool UARDialogueSubsystem::ValidateConversation(UARDialogueConversationAsset* ConversationAsset, FDialogueValidationReport& OutReport) const
{
	OutReport = FDialogueValidationReport();
	if (!ConversationAsset)
	{
		FDialogueValidationIssue& Issue = OutReport.Issues.AddDefaulted_GetRef();
		Issue.Severity = EDialogueValidationSeverity::Error;
		Issue.Message = FText::FromString(TEXT("Conversation asset is null."));
		return false;
	}

	auto Add = [&OutReport](EDialogueValidationSeverity Severity, const FGuid& NodeId, const FString& Message)
	{
		FDialogueValidationIssue& Issue = OutReport.Issues.AddDefaulted_GetRef();
		Issue.Severity = Severity;
		Issue.NodeId = NodeId;
		Issue.Message = FText::FromString(Message);
	};

	auto ValidateCondition = [&Add](const FDialogueCondition& Condition, const FGuid& NodeId)
	{
		if (IsTagSource(Condition.Source))
		{
			if (!IsTagOperator(Condition.Operator))
			{
				Add(EDialogueValidationSeverity::Error, NodeId, TEXT("Condition operator is invalid for tag-based source."));
			}
			if (!Condition.TagValue.IsValid())
			{
				Add(EDialogueValidationSeverity::Error, NodeId, TEXT("Condition requires TagValue for tag-based source."));
			}
			return;
		}

		if (IsNumericSource(Condition.Source))
		{
			if (!IsNumericOperator(Condition.Operator))
			{
				Add(EDialogueValidationSeverity::Error, NodeId, TEXT("Condition operator is invalid for numeric source."));
			}
			return;
		}

		if (IsBoolSource(Condition.Source))
		{
			if (!IsBoolOperator(Condition.Operator))
			{
				Add(EDialogueValidationSeverity::Error, NodeId, TEXT("Condition operator is invalid for bool source."));
			}
			return;
		}

		if (Condition.Source == EDialogueConditionSource::InjectedVariable && Condition.VariableName.IsNone())
		{
			Add(EDialogueValidationSeverity::Error, NodeId, TEXT("Injected variable condition requires VariableName."));
		}
	};

	auto ValidateConditionGroup = [&ValidateCondition](const FDialogueConditionGroup& Group, const FGuid& NodeId)
	{
		for (const FDialogueCondition& Condition : Group.Conditions)
		{
			ValidateCondition(Condition, NodeId);
		}
	};

	if (!ConversationAsset->Header.ConversationTag.IsValid()) { Add(EDialogueValidationSeverity::Error, FGuid(), TEXT("ConversationTag is required.")); }
	if (!ConversationAsset->Header.PrimarySpeakerTag.IsValid()) { Add(EDialogueValidationSeverity::Error, FGuid(), TEXT("PrimarySpeakerTag is required.")); }
	if (!ConversationAsset->CompiledData.EnterNodeId.IsValid()) { Add(EDialogueValidationSeverity::Error, FGuid(), TEXT("Missing Enter node.")); }
	if (ConversationAsset->CompiledData.Nodes.IsEmpty()) { Add(EDialogueValidationSeverity::Error, FGuid(), TEXT("Compiled graph has no nodes.")); }
	ValidateConditionGroup(ConversationAsset->Header.LockedConditions, FGuid());
	ValidateConditionGroup(ConversationAsset->Header.BlockedConditions, FGuid());

	if (ConversationAsset->Header.ConversationTag.IsValid())
	{
#if WITH_EDITOR
		int32 MatchingConversationTagCount = 0;
		TArray<FString> OverlappingConversationLabels;
		const UARDialogueSettings* DialogueSettings = GetDefault<UARDialogueSettings>();
		const FString CurrentOfferGatingSignature = BuildConversationOfferGatingSignature(ConversationAsset->Header);

		if (UContentLookupSubsystem* Lookup = GetLookupSubsystem(this))
		{
			UDataTable* ConversationTable = nullptr;
			FGameplayTag MatchedRoot;
			FString LookupError;
			if (!Lookup->GetDataTableForRowStruct(FARDialogueConversationAssetRow::StaticStruct(), ConversationTable, MatchedRoot, LookupError))
			{
				LookupError.Empty();
				if (DialogueSettings && DialogueSettings->ConversationDefinitionRootTag.IsValid())
				{
					Lookup->GetDataTableForRootTag(DialogueSettings->ConversationDefinitionRootTag, ConversationTable, LookupError);
				}
			}

			if (ConversationTable && ConversationTable->GetRowStruct() == FARDialogueConversationAssetRow::StaticStruct())
			{
				for (const FName RowName : ConversationTable->GetRowNames())
				{
					const FARDialogueConversationAssetRow* Row = ConversationTable->FindRow<FARDialogueConversationAssetRow>(RowName, TEXT("DialogueConversationDuplicateCheck"), false);
					if (!Row)
					{
						continue;
					}

					FGameplayTag CandidateTag = Row->ConversationTag;
					if ((!CandidateTag.IsValid()) && DialogueSettings && DialogueSettings->ConversationDefinitionRootTag.IsValid())
					{
						CandidateTag = BuildTagFromRootAndLeaf(DialogueSettings->ConversationDefinitionRootTag, RowName);
					}

					if (CandidateTag.IsValid() && CandidateTag.MatchesTagExact(ConversationAsset->Header.ConversationTag))
					{
						++MatchingConversationTagCount;
					}

					UARDialogueConversationAsset* CandidateConversation = Row->Conversation.LoadSynchronous();
					if (!CandidateConversation || CandidateConversation == ConversationAsset)
					{
						continue;
					}

					if (BuildConversationOfferGatingSignature(CandidateConversation->Header) == CurrentOfferGatingSignature)
					{
						const FString CandidateLabel = CandidateConversation->Header.ConversationTag.IsValid()
							? CandidateConversation->Header.ConversationTag.ToString()
							: CandidateConversation->GetName();
						OverlappingConversationLabels.AddUnique(CandidateLabel);
					}
				}
			}
		}

		if (MatchingConversationTagCount > 1)
		{
			Add(EDialogueValidationSeverity::Error, FGuid(),
				FString::Printf(TEXT("Duplicate conversation tag '%s' detected (%d occurrences in ContentLookup rows)."),
					*ConversationAsset->Header.ConversationTag.ToString(),
					MatchingConversationTagCount));
		}

		if (!OverlappingConversationLabels.IsEmpty())
		{
			OverlappingConversationLabels.Sort();
			Add(
				EDialogueValidationSeverity::Warning,
				FGuid(),
				FString::Printf(
					TEXT("Offer overlap ambiguity: conversation shares identical primary-speaker/priority/gating with %d other conversation(s): %s"),
					OverlappingConversationLabels.Num(),
					*FString::Join(OverlappingConversationLabels, TEXT(", "))));
		}
#endif
	}

	const FARDialogueRuntimeState& Runtime = GetRuntimeState();
	TMap<FGameplayTag, FARDialogueSpeakerRow> ValidationSpeakerRows = Runtime.SpeakerRowsByTag;
	if (ValidationSpeakerRows.IsEmpty())
	{
		const UARDialogueSettings* DialogueSettings = GetDefault<UARDialogueSettings>();
		const UARContentLookupSettings* LookupSettings = GetDefault<UARContentLookupSettings>();
		UContentLookupRegistry* Registry = LookupSettings ? LookupSettings->RegistryAsset.LoadSynchronous() : nullptr;
		if (DialogueSettings && Registry && DialogueSettings->SpeakerDefinitionRootTag.IsValid())
		{
			const FContentLookupRoute* SpeakerRoute = Registry->Routes.FindByPredicate([DialogueSettings](const FContentLookupRoute& Route)
			{
				return Route.RootTag.MatchesTagExact(DialogueSettings->SpeakerDefinitionRootTag);
			});
			UDataTable* SpeakerTable = SpeakerRoute ? SpeakerRoute->DataTable.LoadSynchronous() : nullptr;
			if (SpeakerTable && SpeakerTable->GetRowStruct() == FARDialogueSpeakerRow::StaticStruct())
			{
				for (const FName RowName : SpeakerTable->GetRowNames())
				{
					const FARDialogueSpeakerRow* SpeakerRow = SpeakerTable->FindRow<FARDialogueSpeakerRow>(RowName, TEXT("DialogueValidationFallback"), false);
					if (!SpeakerRow)
					{
						continue;
					}

					FARDialogueSpeakerRow Copy = *SpeakerRow;
					if (!Copy.SpeakerTag.IsValid())
					{
						Copy.SpeakerTag = BuildTagFromRootAndLeaf(DialogueSettings->SpeakerDefinitionRootTag, RowName);
					}
					if (Copy.SpeakerTag.IsValid())
					{
						ValidationSpeakerRows.Add(Copy.SpeakerTag, Copy);
					}
				}
			}
		}
	}
	if (ConversationAsset->Header.PrimarySpeakerTag.IsValid()
		&& !IsResolvableConversationSpeakerTag(ValidationSpeakerRows, ConversationAsset->Header.PrimarySpeakerTag))
	{
		Add(EDialogueValidationSeverity::Error, FGuid(),
			FString::Printf(TEXT("PrimarySpeakerTag '%s' is not resolvable to a known speaker."),
				*ConversationAsset->Header.PrimarySpeakerTag.ToString()));
	}

	TSet<FGameplayTag> ParticipantSpeakerTags;
	for (const FGameplayTag ParticipantTag : ConversationAsset->Header.ParticipatingSpeakerTags)
	{
		ParticipantSpeakerTags.Add(ParticipantTag);
	}
	if (ConversationAsset->Header.PrimarySpeakerTag.IsValid())
	{
		ParticipantSpeakerTags.Add(ConversationAsset->Header.PrimarySpeakerTag);
	}

	for (const FGameplayTag ParticipantTag : ParticipantSpeakerTags)
	{
		if (!ParticipantTag.IsValid())
		{
			Add(EDialogueValidationSeverity::Error, FGuid(), TEXT("ParticipatingSpeakerTags contains an invalid tag."));
			continue;
		}

		if (!IsResolvableConversationSpeakerTag(ValidationSpeakerRows, ParticipantTag))
		{
			Add(EDialogueValidationSeverity::Error, FGuid(),
				FString::Printf(TEXT("Participating speaker tag '%s' is not resolvable to a known speaker."),
					*ParticipantTag.ToString()));
		}
	}

	TSet<FGuid> NodeIds;
	TMap<FGuid, const FDialogueCompiledNode*> NodeById;
	TMap<FGuid, TArray<FGuid>> OutgoingEdges;
	TMap<FGuid, int32> IncomingCountByNode;
	bool bCanEndNonCompleted = false;

	auto RegisterEdge = [&Add, &NodeById, &OutgoingEdges, &IncomingCountByNode, &bCanEndNonCompleted](const FGuid& FromNodeId, const FGuid& ToNodeId, const bool bOptional, const FString& EdgeLabel)
	{
		if (!ToNodeId.IsValid())
		{
			if (bOptional)
			{
				bCanEndNonCompleted = true;
			}
			return;
		}

		if (!NodeById.Contains(ToNodeId))
		{
			Add(EDialogueValidationSeverity::Error, FromNodeId, FString::Printf(TEXT("%s references missing node '%s'."), *EdgeLabel, *ToNodeId.ToString(EGuidFormats::DigitsWithHyphensInBraces)));
			return;
		}

		OutgoingEdges.FindOrAdd(FromNodeId).AddUnique(ToNodeId);
		IncomingCountByNode.FindOrAdd(ToNodeId) += 1;
	};

	for (const FDialogueCompiledNode& Node : ConversationAsset->CompiledData.Nodes)
	{
		if (!Node.NodeId.IsValid())
		{
			continue;
		}

		if (NodeIds.Contains(Node.NodeId))
		{
			Add(EDialogueValidationSeverity::Error, Node.NodeId, TEXT("Duplicate NodeId found."));
			continue;
		}

		NodeIds.Add(Node.NodeId);
		NodeById.Add(Node.NodeId, &Node);
		IncomingCountByNode.FindOrAdd(Node.NodeId);
	}

	int32 EnterCount = 0;
	TSet<FGuid> CompletedNodeIds;
	TSet<FGuid> LineGuidSet;
	for (const FDialogueCompiledNode& Node : ConversationAsset->CompiledData.Nodes)
	{
		if (!Node.NodeId.IsValid()) { Add(EDialogueValidationSeverity::Error, Node.NodeId, TEXT("Node has invalid NodeId.")); }
		if (Node.NodeType == EDialogueNodeType::Enter) { ++EnterCount; }
		if (Node.NodeType == EDialogueNodeType::Completed) { CompletedNodeIds.Add(Node.NodeId); }

		switch (Node.NodeType)
		{
		case EDialogueNodeType::Enter:
			RegisterEdge(Node.NodeId, Node.NextNodeId, true, TEXT("Enter node next output"));
			break;
		case EDialogueNodeType::Completed:
			break;
		case EDialogueNodeType::Line:
		{
			if (!IsLineNodeData(Node.NodeData))
			{
				Add(EDialogueValidationSeverity::Error, Node.NodeId, TEXT("Line node payload mismatch."));
			}
			else if (const FDialogueLineNodeData* LineData = Node.NodeData.GetPtr<FDialogueLineNodeData>())
			{
				if (LineData->Line.Text.IsEmpty()) { Add(EDialogueValidationSeverity::Error, Node.NodeId, TEXT("Line text is missing (sound-only lines are invalid).")); }
				if (!LineData->Line.SpeakerTag.IsValid())
				{
					Add(EDialogueValidationSeverity::Error, Node.NodeId, TEXT("Line speaker tag is invalid."));
				}
				else if (!IsResolvableConversationSpeakerTag(ValidationSpeakerRows, LineData->Line.SpeakerTag))
				{
					Add(EDialogueValidationSeverity::Error, Node.NodeId,
						FString::Printf(TEXT("Line speaker '%s' does not resolve to a known speaker."),
							*LineData->Line.SpeakerTag.ToString()));
				}
				else if (!ParticipantSpeakerTags.Contains(LineData->Line.SpeakerTag))
				{
					Add(EDialogueValidationSeverity::Warning, Node.NodeId,
						FString::Printf(TEXT("Line speaker '%s' is not listed in ParticipatingSpeakerTags/PrimarySpeakerTag."),
							*LineData->Line.SpeakerTag.ToString()));
				}
				if (!LineData->Line.LocalLineGuid.IsValid())
				{
					Add(EDialogueValidationSeverity::Error, Node.NodeId, TEXT("Line LocalLineGuid must be valid."));
				}
				else if (LineGuidSet.Contains(LineData->Line.LocalLineGuid))
				{
					Add(EDialogueValidationSeverity::Warning, Node.NodeId, TEXT("Repeated line LocalLineGuid detected in conversation."));
				}
				LineGuidSet.Add(LineData->Line.LocalLineGuid);

				ValidateConditionGroup(LineData->SkipLockedConditions, Node.NodeId);
				ValidateConditionGroup(LineData->SkipBlockedConditions, Node.NodeId);
			}
			RegisterEdge(Node.NodeId, Node.NextNodeId, true, TEXT("Line node next output"));
			break;
		}
		case EDialogueNodeType::Choice:
		{
			TSet<FGuid> SeenChoiceBranches;
			for (const FDialogueCompiledChoiceBranch& Branch : Node.ChoiceBranches)
			{
				if (!Branch.ChoiceBranchId.IsValid())
				{
					Add(EDialogueValidationSeverity::Error, Node.NodeId, TEXT("Choice branch has invalid ChoiceBranchId."));
					continue;
				}
				if (SeenChoiceBranches.Contains(Branch.ChoiceBranchId))
				{
					Add(EDialogueValidationSeverity::Error, Node.NodeId, TEXT("Choice branch id is duplicated in node."));
				}
				SeenChoiceBranches.Add(Branch.ChoiceBranchId);
				ValidateConditionGroup(Branch.LockedConditions, Node.NodeId);
				ValidateConditionGroup(Branch.BlockedConditions, Node.NodeId);
				RegisterEdge(Node.NodeId, Branch.NextNodeId, true, TEXT("Choice branch output"));
			}
			RegisterEdge(Node.NodeId, Node.FallbackNodeId, true, TEXT("Choice fallback output"));
			if (Node.ChoiceBranches.IsEmpty())
			{
				Add(EDialogueValidationSeverity::Warning, Node.NodeId, TEXT("Choice node has zero authored branches; fallback-only outcome."));
			}
			break;
		}
		case EDialogueNodeType::Bool:
		{
			const FDialogueBoolNodeData* BoolData = Node.NodeData.GetPtr<FDialogueBoolNodeData>();
			if (!BoolData)
			{
				Add(EDialogueValidationSeverity::Error, Node.NodeId, TEXT("Bool node payload mismatch."));
			}
			else
			{
				ValidateCondition(BoolData->Condition, Node.NodeId);
			}
			RegisterEdge(Node.NodeId, Node.TrueNodeId, true, TEXT("Bool true output"));
			RegisterEdge(Node.NodeId, Node.FalseNodeId, true, TEXT("Bool false output"));
			break;
		}
		case EDialogueNodeType::SwitchOnTagsByPriority:
		{
			TSet<FGuid> SeenSwitchBranchIds;
			TMap<FString, int32> FirstBranchIndexByGateSignature;
			for (int32 BranchIndex = 0; BranchIndex < Node.SwitchBranches.Num(); ++BranchIndex)
			{
				const FDialogueCompiledSwitchBranch& Branch = Node.SwitchBranches[BranchIndex];
				if (!Branch.BranchId.IsValid())
				{
					Add(EDialogueValidationSeverity::Error, Node.NodeId, TEXT("Switch branch has invalid BranchId."));
					continue;
				}
				if (SeenSwitchBranchIds.Contains(Branch.BranchId))
				{
					Add(EDialogueValidationSeverity::Error, Node.NodeId, TEXT("Switch branch id is duplicated in node."));
				}
				SeenSwitchBranchIds.Add(Branch.BranchId);
				ValidateConditionGroup(Branch.LockedConditions, Node.NodeId);
				ValidateConditionGroup(Branch.BlockedConditions, Node.NodeId);
				RegisterEdge(Node.NodeId, Branch.NextNodeId, true, TEXT("Switch branch output"));

				const FString GateSignature = BuildConditionGroupKey(Branch.LockedConditions) + TEXT("||") + BuildConditionGroupKey(Branch.BlockedConditions);
				if (const int32* ExistingIndex = FirstBranchIndexByGateSignature.Find(GateSignature))
				{
					Add(
						EDialogueValidationSeverity::Warning,
						Node.NodeId,
						FString::Printf(
							TEXT("Switch branch ordering ambiguity: branch %d and branch %d share identical gating; first-match order controls routing."),
							*ExistingIndex + 1,
							BranchIndex + 1));
				}
				else
				{
					FirstBranchIndexByGateSignature.Add(GateSignature, BranchIndex);
				}
			}
			if (Node.bSwitchHasDefaultOutput)
			{
				RegisterEdge(Node.NodeId, Node.SwitchDefaultNodeId, true, TEXT("Switch default output"));
			}
			else if (Node.SwitchBranches.IsEmpty())
			{
				Add(EDialogueValidationSeverity::Error, Node.NodeId, TEXT("Switch node has no branches and no default output."));
			}
			break;
		}
		case EDialogueNodeType::TagMutation:
		{
			const FDialogueTagMutationNodeData* MutationData = Node.NodeData.GetPtr<FDialogueTagMutationNodeData>();
			if (!MutationData)
			{
				Add(EDialogueValidationSeverity::Error, Node.NodeId, TEXT("Tag mutation node payload mismatch."));
			}
			else
			{
				for (const FDialogueTagMutation& Mutation : MutationData->Mutations)
				{
					if (!Mutation.Tag.IsValid())
					{
						Add(EDialogueValidationSeverity::Error, Node.NodeId, TEXT("Tag mutation contains invalid tag."));
					}
				}
			}
			RegisterEdge(Node.NodeId, Node.NextNodeId, true, TEXT("Tag mutation next output"));
			break;
		}
		case EDialogueNodeType::RelationshipMutation:
		{
			const FDialogueRelationshipMutationNodeData* MutationData = Node.NodeData.GetPtr<FDialogueRelationshipMutationNodeData>();
			if (!MutationData)
			{
				Add(EDialogueValidationSeverity::Error, Node.NodeId, TEXT("Relationship mutation node payload mismatch."));
			}
			RegisterEdge(Node.NodeId, Node.NextNodeId, true, TEXT("Relationship mutation next output"));
			break;
		}
		case EDialogueNodeType::FactionMutation:
		{
			const FDialogueFactionMutationNodeData* MutationData = Node.NodeData.GetPtr<FDialogueFactionMutationNodeData>();
			if (!MutationData)
			{
				Add(EDialogueValidationSeverity::Error, Node.NodeId, TEXT("Faction mutation node payload mismatch."));
			}
			else if (!MutationData->FactionTag.IsValid())
			{
				Add(EDialogueValidationSeverity::Error, Node.NodeId, TEXT("Faction mutation requires valid FactionTag."));
			}
			RegisterEdge(Node.NodeId, Node.NextNodeId, true, TEXT("Faction mutation next output"));
			break;
		}
		case EDialogueNodeType::Random:
		{
			TSet<FGuid> SeenRandomBranchIds;
			float TotalWeight = 0.0f;
			for (const FDialogueCompiledRandomBranch& Branch : Node.RandomBranches)
			{
				if (!Branch.BranchId.IsValid())
				{
					Add(EDialogueValidationSeverity::Error, Node.NodeId, TEXT("Random branch has invalid BranchId."));
					continue;
				}
				if (SeenRandomBranchIds.Contains(Branch.BranchId))
				{
					Add(EDialogueValidationSeverity::Error, Node.NodeId, TEXT("Random branch id is duplicated in node."));
				}
				SeenRandomBranchIds.Add(Branch.BranchId);
				if (Branch.Weight <= 0.0f)
				{
					Add(EDialogueValidationSeverity::Warning, Node.NodeId, TEXT("Random branch has non-positive weight and will never be selected."));
				}
				else
				{
					TotalWeight += Branch.Weight;
				}
				RegisterEdge(Node.NodeId, Branch.NextNodeId, true, TEXT("Random branch output"));
			}
			if (TotalWeight <= 0.0f)
			{
				Add(EDialogueValidationSeverity::Error, Node.NodeId, TEXT("Random node has no valid positive-weight branches."));
			}
			break;
		}
		default:
			Add(EDialogueValidationSeverity::Error, Node.NodeId, TEXT("Unknown node type."));
			break;
		}
	}
	if (EnterCount != 1) { Add(EDialogueValidationSeverity::Error, FGuid(), FString::Printf(TEXT("Exactly one Enter node required (found %d)."), EnterCount)); }
	if (ConversationAsset->CompiledData.EnterNodeId.IsValid() && !NodeById.Contains(ConversationAsset->CompiledData.EnterNodeId))
	{
		Add(EDialogueValidationSeverity::Error, FGuid(), TEXT("Compiled EnterNodeId does not resolve to a node."));
	}

	TSet<FGuid> ReachableNodeIds;
	if (NodeById.Contains(ConversationAsset->CompiledData.EnterNodeId))
	{
		TArray<FGuid> Stack;
		Stack.Add(ConversationAsset->CompiledData.EnterNodeId);
		while (!Stack.IsEmpty())
		{
			const FGuid Current = Stack.Pop(EAllowShrinking::No);
			if (ReachableNodeIds.Contains(Current))
			{
				continue;
			}
			ReachableNodeIds.Add(Current);

			if (const TArray<FGuid>* OutEdges = OutgoingEdges.Find(Current))
			{
				for (const FGuid Next : *OutEdges)
				{
					if (Next.IsValid() && !ReachableNodeIds.Contains(Next))
					{
						Stack.Add(Next);
					}
				}
			}
		}
	}

	bool bHasPathToCompleted = false;
	for (const FGuid CompletedNodeId : CompletedNodeIds)
	{
		if (ReachableNodeIds.Contains(CompletedNodeId))
		{
			bHasPathToCompleted = true;
			break;
		}
	}
	if (!bHasPathToCompleted)
	{
		Add(EDialogueValidationSeverity::Warning, FGuid(), TEXT("No path from Enter reaches any Completed node."));
	}

	for (const TPair<FGuid, const FDialogueCompiledNode*>& Pair : NodeById)
	{
		if (!ReachableNodeIds.Contains(Pair.Key))
		{
			Add(EDialogueValidationSeverity::Warning, Pair.Key, TEXT("Unreachable/orphan node detected."));
		}
		else if (Pair.Value->NodeType != EDialogueNodeType::Enter && IncomingCountByNode.FindRef(Pair.Key) == 0)
		{
			Add(EDialogueValidationSeverity::Warning, Pair.Key, TEXT("Reachable node has no incoming edges (orphan layout)."));
		}
	}

	if (bCanEndNonCompleted)
	{
		Add(EDialogueValidationSeverity::Warning, FGuid(), TEXT("Conversation has at least one path that can end non-completed."));
	}

	// Loop diagnostics: warn for reachable cycles that have no path to a Completed node.
	TMap<FGuid, TArray<FGuid>> ReverseEdges;
	for (const TPair<FGuid, TArray<FGuid>>& Pair : OutgoingEdges)
	{
		for (const FGuid To : Pair.Value)
		{
			ReverseEdges.FindOrAdd(To).AddUnique(Pair.Key);
		}
	}

	TSet<FGuid> CanReachCompleted;
	TArray<FGuid> ReverseStack = CompletedNodeIds.Array();
	while (!ReverseStack.IsEmpty())
	{
		const FGuid Current = ReverseStack.Pop(EAllowShrinking::No);
		if (CanReachCompleted.Contains(Current))
		{
			continue;
		}
		CanReachCompleted.Add(Current);

		if (const TArray<FGuid>* ReverseFrom = ReverseEdges.Find(Current))
		{
			for (const FGuid Previous : *ReverseFrom)
			{
				if (ReachableNodeIds.Contains(Previous) && !CanReachCompleted.Contains(Previous))
				{
					ReverseStack.Add(Previous);
				}
			}
		}
	}

	TSet<FGuid> VisitedNodes;
	TSet<FGuid> ActiveStackNodes;
	bool bFoundSuspiciousLoop = false;
	TFunction<void(FGuid)> WalkForCycles = [&](const FGuid Current)
	{
		if (bFoundSuspiciousLoop || !ReachableNodeIds.Contains(Current))
		{
			return;
		}

		VisitedNodes.Add(Current);
		ActiveStackNodes.Add(Current);

		if (const TArray<FGuid>* OutEdges = OutgoingEdges.Find(Current))
		{
			for (const FGuid Next : *OutEdges)
			{
				if (!ReachableNodeIds.Contains(Next))
				{
					continue;
				}

				if (ActiveStackNodes.Contains(Next))
				{
					if (!CanReachCompleted.Contains(Current) && !CanReachCompleted.Contains(Next))
					{
						Add(EDialogueValidationSeverity::Warning, Current, TEXT("Suspicious loop detected with no apparent path to Completed."));
						bFoundSuspiciousLoop = true;
						return;
					}
					continue;
				}

				if (!VisitedNodes.Contains(Next))
				{
					WalkForCycles(Next);
				}
			}
		}

		ActiveStackNodes.Remove(Current);
	};

	if (ReachableNodeIds.Contains(ConversationAsset->CompiledData.EnterNodeId))
	{
		WalkForCycles(ConversationAsset->CompiledData.EnterNodeId);
	}

	return !OutReport.HasErrors();
}

static FDialogueRuntimeContext BuildOfferContext(
	const UARDialogueSubsystem* DialogueSubsystem,
	const UARDialogueConversationAsset* Conversation,
	AARPlayerStateBase* RequesterPS,
	const FARPlayerIdentity& PlayerIdentity)
{
	FDialogueRuntimeContext Context;
	Context.World = DialogueSubsystem ? DialogueSubsystem->GetWorld() : nullptr;
	Context.GameState = Context.World ? Context.World->GetGameState() : nullptr;
	Context.ConversationTag = Conversation ? Conversation->Header.ConversationTag : FGameplayTag();
	Context.ConversationAsset = const_cast<UARDialogueConversationAsset*>(Conversation);
	Context.PrimarySpeakerTag = Conversation ? Conversation->Header.PrimarySpeakerTag : FGameplayTag();
	Context.ActivePlayerState = RequesterPS;
	Context.ActivePlayerController = RequesterPS ? Cast<APlayerController>(RequesterPS->GetOwner()) : nullptr;
	Context.ResolvedPlayerSpeakerTag = ResolvePlayerSpeakerTag(RequesterPS);
	Context.RelationshipPointsForPrimarySpeaker = DialogueSubsystem ? DialogueSubsystem->GetRelationshipPointsForSpeaker(Context.PrimarySpeakerTag) : 0.0f;
	Context.RelationshipLevelForPrimarySpeaker = DialogueSubsystem ? DialogueSubsystem->GetRelationshipLevelForSpeaker(Context.PrimarySpeakerTag) : 0;
	if (RequesterPS)
	{
		Context.LoadoutView.LoadoutTags = RequesterPS->LoadoutTags;
	}

	const UARSaveGame* SaveGame = GetCurrentSave(DialogueSubsystem);
	if (SaveGame)
	{
		Context.GameOnlyProgressionTags = SaveGame->ProgressionTags;
		GetProgressionTagsForIdentity(SaveGame, PlayerIdentity, Context.PlayerOnlyProgressionTags);
		Context.CombinedProgressionTags = DialogueSubsystem->GetCombinedDialogueTags(Context.PlayerOnlyProgressionTags, Context.GameOnlyProgressionTags);
		Context.bCompletedByGame = SaveGame->DialogueCompletedConversationTagsByGame.HasTagExact(Context.ConversationTag);
		if (const FDialoguePlayerPersistentState* PlayerState = FindPlayerDialogueState(SaveGame, PlayerIdentity))
		{
			Context.bCompletedByPlayer = PlayerState->CompletedConversationTags.HasTagExact(Context.ConversationTag);
		}
	}

	return Context;
}

static bool PassesConversationOfferRules(
	const UARDialogueSubsystem* DialogueSubsystem,
	const FDialogueRuntimeContext& Context,
	const FDialogueConversationHeader& Header)
{
	if (Context.RelationshipPointsForPrimarySpeaker < Header.MinimumRelationshipPoints)
	{
		return false;
	}

	if (!PassesLockedConditions(DialogueSubsystem, Header.LockedConditions, Context))
	{
		return false;
	}

	if (!PassesBlockedConditions(DialogueSubsystem, Header.BlockedConditions, Context))
	{
		return false;
	}

	return true;
}

static bool EvaluateConversationOfferRules(
	const UARDialogueSubsystem* DialogueSubsystem,
	const FDialogueRuntimeContext& Context,
	const FDialogueConversationHeader& Header,
	FString* OutFailureReason)
{
	if (Context.RelationshipPointsForPrimarySpeaker < Header.MinimumRelationshipPoints)
	{
		if (OutFailureReason)
		{
			*OutFailureReason = FString::Printf(
				TEXT("RelationshipPoints %.2f < MinimumRelationshipPoints %.2f."),
				Context.RelationshipPointsForPrimarySpeaker,
				Header.MinimumRelationshipPoints);
		}
		return false;
	}

	if (!PassesLockedConditions(DialogueSubsystem, Header.LockedConditions, Context))
	{
		if (OutFailureReason)
		{
			*OutFailureReason = TEXT("LockedConditions failed.");
		}
		return false;
	}

	if (!PassesBlockedConditions(DialogueSubsystem, Header.BlockedConditions, Context))
	{
		if (OutFailureReason)
		{
			*OutFailureReason = TEXT("BlockedConditions failed.");
		}
		return false;
	}

	return true;
}

bool UARDialogueSubsystem::GetAvailableConversationForNPC(AARPlayerController* RequestingController, FGameplayTag PrimarySpeakerTag, FDialogueConversationOffer& OutOffer, bool bNpcLocalStateAllowsDialogue)
{
	OutOffer = FDialogueConversationOffer();
	if (!RequestingController)
	{
		UE_LOG(ARLog, Verbose, TEXT("[Dialogue] Offer request ignored: RequestingController is null."));
		return false;
	}
	if (!PrimarySpeakerTag.IsValid())
	{
		UE_LOG(ARLog, Verbose, TEXT("[Dialogue] Offer request ignored: PrimarySpeakerTag is invalid."));
		return false;
	}

	if (!bNpcLocalStateAllowsDialogue)
	{
		UE_LOG(ARLog, Verbose, TEXT("[Dialogue] Offer blocked: NPC local state disallows dialogue for speaker '%s'."), *PrimarySpeakerTag.ToString());
		return false;
	}

	AARPlayerStateBase* RequesterPS = RequestingController->GetPlayerState<AARPlayerStateBase>();
	if (!RequesterPS)
	{
		UE_LOG(ARLog, Verbose, TEXT("[Dialogue] Offer blocked: RequestingController has no PlayerState for speaker '%s'."), *PrimarySpeakerTag.ToString());
		return false;
	}

	const FARPlayerIdentity PlayerIdentity = BuildPlayerIdentityFromState(RequesterPS);
	const EARPlayerSlot RequesterSlot = RequesterPS->GetPlayerSlot();
	if (RequesterSlot == EARPlayerSlot::Unknown)
	{
		UE_LOG(ARLog, Verbose, TEXT("[Dialogue] Offer blocked: Requester slot is Unknown for speaker '%s'."), *PrimarySpeakerTag.ToString());
		return false;
	}
	FARDialogueRuntimeState& Runtime = GetRuntimeState();
	const UWorld* World = GetWorld();
	const UARDialogueSettings* Settings = GetDefault<UARDialogueSettings>();
	const FGameplayTag ModeTag = GetCurrentModeTag(World);
	if (!IsModeDialogueEnabled(Settings, ModeTag))
	{
		UE_LOG(ARLog, Verbose, TEXT("[Dialogue] Offer blocked: mode '%s' not in dialogue-enabled tags."), *ModeTag.ToString());
		return false;
	}

	TArray<FDialogueCandidateEval> Unseen;
	TArray<FDialogueCandidateEval> Catchup;
	TArray<FDialogueCandidateEval> Repeatable;

	for (const TPair<FGameplayTag, TObjectPtr<UARDialogueConversationAsset>>& Pair : Runtime.ConversationsByTag)
	{
		UARDialogueConversationAsset* Conversation = Pair.Value;
		if (!Conversation || !Conversation->Header.PrimarySpeakerTag.MatchesTagExact(PrimarySpeakerTag))
		{
			continue;
		}

		FDialogueValidationReport Validation;
		if (!ValidateConversation(Conversation, Validation))
		{
			int32 ErrorCount = 0;
			for (const FDialogueValidationIssue& Issue : Validation.Issues)
			{
				if (Issue.Severity == EDialogueValidationSeverity::Error)
				{
					++ErrorCount;
				}
			}
			UE_LOG(ARLog, Warning,
				TEXT("[Dialogue] Offer skipped: conversation '%s' is invalid (%d issues, %d errors)."),
				*Conversation->Header.ConversationTag.ToString(),
				Validation.Issues.Num(),
				ErrorCount);
			continue;
		}

		FDialogueRuntimeContext Context = BuildOfferContext(this, Conversation, RequesterPS, PlayerIdentity);
		Context.bSeenByGame = Runtime.SeenByGameTransient.HasTagExact(Conversation->Header.ConversationTag);
		if (const FGameplayTagContainer* SeenForPlayer = Runtime.SeenByPlayerTransient.Find(RequesterSlot))
		{
			Context.bSeenByPlayer = SeenForPlayer->HasTagExact(Conversation->Header.ConversationTag);
		}
		FString OfferGateFailure;
		if (!EvaluateConversationOfferRules(this, Context, Conversation->Header, &OfferGateFailure))
		{
			UE_LOG(ARLog, Verbose,
				TEXT("[Dialogue] Offer skipped '%s' for speaker '%s': %s"),
				*Conversation->Header.ConversationTag.ToString(),
				*PrimarySpeakerTag.ToString(),
				*OfferGateFailure);
			continue;
		}

		FDialogueCandidateEval Candidate;
		Candidate.Conversation = Conversation;
		Candidate.Priority = Conversation->Header.Priority;
		Candidate.bRepeatable = Conversation->Header.bRepeatable;
		Candidate.bSeenByGame = Context.bSeenByGame;
		Candidate.bSeenByPlayer = Context.bSeenByPlayer;
		Candidate.bCompletedByGame = Context.bCompletedByGame;
		Candidate.bCompletedByPlayer = Context.bCompletedByPlayer;

		if (!Conversation->Header.bRepeatable && Candidate.bCompletedByPlayer)
		{
			UE_LOG(ARLog, Verbose,
				TEXT("[Dialogue] Offer skipped '%s': non-repeatable and already completed by requesting player."),
				*Conversation->Header.ConversationTag.ToString());
			continue;
		}
		if (Conversation->Header.bCompletedByGameBlocksReoffer && Candidate.bCompletedByGame)
		{
			UE_LOG(ARLog, Verbose,
				TEXT("[Dialogue] Offer skipped '%s': bCompletedByGameBlocksReoffer is true and conversation is completed by game."),
				*Conversation->Header.ConversationTag.ToString());
			continue;
		}
		if (Conversation->Header.bSeenByGameBlocksReoffer && Candidate.bSeenByGame)
		{
			UE_LOG(ARLog, Verbose,
				TEXT("[Dialogue] Offer skipped '%s': bSeenByGameBlocksReoffer is true and conversation is seen by game."),
				*Conversation->Header.ConversationTag.ToString());
			continue;
		}
		if (Conversation->Header.bSeenByPlayerBlocksReoffer && Candidate.bSeenByPlayer)
		{
			UE_LOG(ARLog, Verbose,
				TEXT("[Dialogue] Offer skipped '%s': bSeenByPlayerBlocksReoffer is true and conversation is seen by requesting player."),
				*Conversation->Header.ConversationTag.ToString());
			continue;
		}

		if (!Candidate.bSeenByGame && !Candidate.bSeenByPlayer)
		{
			Unseen.Add(Candidate);
		}
		else if (Candidate.bSeenByGame && !Candidate.bSeenByPlayer)
		{
			Catchup.Add(Candidate);
		}
		else
		{
			Repeatable.Add(Candidate);
		}
	}

	auto SelectCandidate = [](TArray<FDialogueCandidateEval>& Bucket, FDialogueCandidateEval& OutCandidate) -> bool
	{
		if (Bucket.IsEmpty())
		{
			return false;
		}
		Bucket.Sort(&SortCandidatesByPriority);
		const int32 BestPriority = Bucket[0].Priority;
		TArray<int32> Tied;
		for (int32 Index = 0; Index < Bucket.Num(); ++Index)
		{
			if (Bucket[Index].Priority != BestPriority)
			{
				break;
			}
			Tied.Add(Index);
		}
		OutCandidate = Bucket[Tied[FMath::RandRange(0, Tied.Num() - 1)]];
		return true;
	};

	FDialogueCandidateEval Picked;
	if (!SelectCandidate(Unseen, Picked) && !SelectCandidate(Catchup, Picked) && !SelectCandidate(Repeatable, Picked))
	{
		UE_LOG(ARLog, Verbose, TEXT("[Dialogue] Offer: no valid conversations for speaker '%s'."), *PrimarySpeakerTag.ToString());
		return false;
	}

	OutOffer.ConversationTag = Picked.Conversation ? Picked.Conversation->Header.ConversationTag : FGameplayTag();
	OutOffer.Priority = Picked.Priority;
	OutOffer.bUnseenByGame = !Picked.bSeenByGame;
	OutOffer.bUnseenByPlayer = !Picked.bSeenByPlayer;
	OutOffer.bCatchUpCandidate = Picked.bSeenByGame && !Picked.bSeenByPlayer;
	OutOffer.bRepeatableCandidate = Picked.bRepeatable;

	const FString SlotString = StaticEnum<EARPlayerSlot>()->GetNameStringByValue(static_cast<int64>(RequesterSlot));
	const FString BucketLabel = OutOffer.bUnseenByGame && OutOffer.bUnseenByPlayer
		? TEXT("Unseen")
		: (OutOffer.bCatchUpCandidate ? TEXT("CatchUp") : (OutOffer.bRepeatableCandidate ? TEXT("Repeatable") : TEXT("Seen")));
	UE_LOG(ARLog, Verbose,
		TEXT("[Dialogue] Offer resolved: slot %s speaker '%s' -> conversation '%s' (priority %d, bucket %s)."),
		*SlotString,
		*PrimarySpeakerTag.ToString(),
		*OutOffer.ConversationTag.ToString(),
		OutOffer.Priority,
		*BucketLabel);
	return OutOffer.ConversationTag.IsValid();
}

enum class EDialogueExecutionResult : uint8
{
	Waiting = 0,
	EndedCompleted,
	EndedNonCompleted,
	Failed
};

static void FillClientViewForSlot(const FARActiveDialogueSession& Session, const EARPlayerSlot Slot, FDialogueClientView& OutView)
{
	OutView = FDialogueClientView();
	OutView.SessionId = Session.SessionId;
	OutView.ConversationTag = Session.ConversationTag;
	OutView.CurrentNodeId = Session.CurrentNodeId;
	OutView.SpeakerTag = Session.CurrentSpeakerTag;
	OutView.LineText = Session.CurrentLineText;
	OutView.SpeakerPortrait = Session.CurrentSpeakerPortrait;
	OutView.Choices = Session.CurrentChoices;
	OutView.bWaitingForChoice = Session.bWaitingForChoice;
	OutView.bConversationImportant = Session.bConversationImportant;
	OutView.bIsEavesdropping = !Session.bIsSharedSession && Slot != Session.OwnerSlot;
	OutView.InitiatorSlot = Session.InitiatorSlot;
	OutView.OwnerSlot = Session.OwnerSlot;
}

static void BroadcastSessionUpdated(UARDialogueSubsystem* DialogueSubsystem, const FARActiveDialogueSession& Session)
{
	if (!DialogueSubsystem)
	{
		return;
	}

	for (const EARPlayerSlot Slot : Session.Participants)
	{
		if (Slot == EARPlayerSlot::Unknown)
		{
			continue;
		}

		FDialogueClientView View;
		FillClientViewForSlot(Session, Slot, View);
		DialogueSubsystem->OnDialogueSessionUpdated.Broadcast(View);

		if (AARPlayerController* TargetController = FindPlayerControllerBySlot(DialogueSubsystem->GetWorld(), Slot))
		{
			TargetController->ClientDialogueSessionUpdated(View);
		}
	}
}

static int32 FindSessionIndexForSlot(const TArray<FARActiveDialogueSession>& Sessions, const EARPlayerSlot Slot)
{
	for (int32 Index = 0; Index < Sessions.Num(); ++Index)
	{
		if (Sessions[Index].Participants.Contains(Slot))
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

static void ClearSessionPresentationState(FARActiveDialogueSession& Session)
{
	Session.bWaitingForChoice = false;
	Session.bWaitingForAdvanceInput = false;
	Session.bChoiceRequiresAllViewers = false;
	Session.WaitingChoiceNodeId.Invalidate();
	Session.WaitingLineNodeId.Invalidate();
	Session.CurrentChoices.Reset();
	Session.CurrentSpeakerTag = FGameplayTag();
	Session.CurrentLineText = FText::GetEmpty();
	Session.CurrentSpeakerPortrait = FSpeakerPortraitData();
}

static FARPlayerIdentity BuildOwnerIdentityForSession(const UWorld* World, const FARActiveDialogueSession& Session)
{
	const AARPlayerStateBase* OwnerPlayerState = FindPlayerStateBySlot(World, Session.OwnerSlot);
	return BuildPlayerIdentityFromState(OwnerPlayerState);
}

static FDialogueRuntimeContext BuildSessionContext(
	const UARDialogueSubsystem* DialogueSubsystem,
	const FARActiveDialogueSession& Session,
	const TMap<EARPlayerSlot, FGameplayTagContainer>& SeenByPlayerTransient,
	const FGameplayTagContainer& SeenByGameTransient)
{
	FDialogueRuntimeContext Context;
	Context.ConversationTag = Session.ConversationTag;
	Context.ConversationAsset = Session.ConversationAsset;
	Context.PrimarySpeakerTag = Session.PrimarySpeakerTag;
	Context.World = DialogueSubsystem ? DialogueSubsystem->GetWorld() : nullptr;
	Context.GameState = Context.World ? Context.World->GetGameState() : nullptr;
	Context.ActivePlayerController = FindPlayerControllerBySlot(Context.World, Session.OwnerSlot);
	Context.ActivePlayerState = FindPlayerStateBySlot(Context.World, Session.OwnerSlot);
	Context.ActivePawn = Context.ActivePlayerController ? Context.ActivePlayerController->GetPawn() : nullptr;

	EARPlayerSlot OtherSlot = EARPlayerSlot::Unknown;
	for (const EARPlayerSlot Candidate : Session.Participants)
	{
		if (Candidate != Session.OwnerSlot && Candidate != EARPlayerSlot::Unknown)
		{
			OtherSlot = Candidate;
			break;
		}
	}
	Context.OtherPlayerController = FindPlayerControllerBySlot(Context.World, OtherSlot);
	Context.OtherPlayerState = FindPlayerStateBySlot(Context.World, OtherSlot);
	Context.OtherPawn = Context.OtherPlayerController ? Context.OtherPlayerController->GetPawn() : nullptr;

	const AARPlayerStateBase* ActiveARPS = Cast<AARPlayerStateBase>(Context.ActivePlayerState);
	if (ActiveARPS)
	{
		Context.ResolvedPlayerSpeakerTag = ResolvePlayerSpeakerTag(ActiveARPS);
		Context.LoadoutView.LoadoutTags = ActiveARPS->LoadoutTags;
	}

	const UARSaveGame* SaveGame = GetCurrentSave(DialogueSubsystem);
	if (SaveGame)
	{
		Context.GameOnlyProgressionTags = SaveGame->ProgressionTags;
		const FARPlayerIdentity Identity = BuildOwnerIdentityForSession(Context.World, Session);
		GetProgressionTagsForIdentity(SaveGame, Identity, Context.PlayerOnlyProgressionTags);
		Context.CombinedProgressionTags = DialogueSubsystem->GetCombinedDialogueTags(Context.PlayerOnlyProgressionTags, Context.GameOnlyProgressionTags);
		Context.bCompletedByGame = SaveGame->DialogueCompletedConversationTagsByGame.HasTagExact(Session.ConversationTag);
		if (const FDialoguePlayerPersistentState* PlayerState = FindPlayerDialogueState(SaveGame, Identity))
		{
			Context.bCompletedByPlayer = PlayerState->CompletedConversationTags.HasTagExact(Session.ConversationTag);
		}
	}

	Context.TransientConversationTags = Session.TransientConversationTags;
	Context.bSeenByGame = SeenByGameTransient.HasTagExact(Session.ConversationTag);
	if (const FGameplayTagContainer* SeenForOwner = SeenByPlayerTransient.Find(Session.OwnerSlot))
	{
		Context.bSeenByPlayer = SeenForOwner->HasTagExact(Session.ConversationTag);
	}

	Context.RelationshipPointsForPrimarySpeaker = DialogueSubsystem->GetRelationshipPointsForSpeaker(Session.PrimarySpeakerTag);
	Context.RelationshipLevelForPrimarySpeaker = DialogueSubsystem->GetRelationshipLevelForSpeaker(Session.PrimarySpeakerTag);
	return Context;
}

static void AddSessionParticipant(
	FARActiveDialogueSession& Session,
	TMap<EARPlayerSlot, FGameplayTagContainer>& SeenByPlayerTransient,
	const EARPlayerSlot Slot)
{
	if (Slot == EARPlayerSlot::Unknown)
	{
		return;
	}

	Session.Participants.Add(Slot);
	SeenByPlayerTransient.FindOrAdd(Slot).AddTag(Session.ConversationTag);
}

static bool PersistCompletedConversation(
	UARDialogueSubsystem* DialogueSubsystem,
	const FARActiveDialogueSession& Session)
{
	UARSaveSubsystem* SaveSubsystem = GetSaveSubsystem(DialogueSubsystem);
	UARSaveGame* SaveGame = GetCurrentSave(DialogueSubsystem);
	if (!SaveGame || !SaveSubsystem)
	{
		UE_LOG(ARLog, Warning, TEXT("[Dialogue] PersistCompletedConversation failed: save subsystem or save game unavailable for '%s'."),
			*Session.ConversationTag.ToString());
		DialogueSubsystem->OnConversationCompleted.Broadcast(Session.ConversationTag);
		return false;
	}

	SaveGame->DialogueCompletedConversationTagsByGame.AddTag(Session.ConversationTag);

	const FARPlayerIdentity OwnerIdentity = BuildOwnerIdentityForSession(DialogueSubsystem->GetWorld(), Session);
	if (FDialoguePlayerPersistentState* PlayerState = FindOrAddPlayerDialogueState(SaveGame, OwnerIdentity))
	{
		PlayerState->CompletedConversationTags.AddTag(Session.ConversationTag);

		PlayerState->CompletedChoiceRecords.RemoveAll([&Session](const FDialogueChoiceMemoryRecord& Record)
		{
			return Record.ConversationTag.MatchesTagExact(Session.ConversationTag);
		});

		for (const TPair<FGuid, FGuid>& Pair : Session.RuntimeChoiceSelections)
		{
			if (!Pair.Key.IsValid() || !Pair.Value.IsValid())
			{
				continue;
			}

			FDialogueChoiceMemoryRecord& Added = PlayerState->CompletedChoiceRecords.AddDefaulted_GetRef();
			Added.ConversationTag = Session.ConversationTag;
			Added.ChoiceNodeId = Pair.Key;
			Added.SelectedBranchId = Pair.Value;
		}
	}

	SaveSubsystem->MarkSaveDirty();
	const FString OwnerSlotString = StaticEnum<EARPlayerSlot>()->GetNameStringByValue(static_cast<int64>(OwnerIdentity.PlayerSlot));
	UE_LOG(ARLog, Verbose, TEXT("[Dialogue] Conversation '%s' completed and persisted (PlayerSlot=%s)."),
		*Session.ConversationTag.ToString(),
		*OwnerSlotString);
	DialogueSubsystem->OnConversationCompleted.Broadcast(Session.ConversationTag);

	// Keep NPC talkable icons/state in sync after completion changes offer availability.
	if (UGameInstance* GI = DialogueSubsystem->GetGameInstance())
	{
		if (UARNPCSubsystem* NpcSubsystem = GI->GetSubsystem<UARNPCSubsystem>())
		{
			NpcSubsystem->RefreshAllNpcTalkableStates();
		}
	}

	return true;
}

static bool IsBranchConnected(const FGuid& NodeId)
{
	return NodeId.IsValid();
}

static EDialogueExecutionResult ExecuteSessionUntilWait(
	UARDialogueSubsystem* DialogueSubsystem,
	FARActiveDialogueSession& Session,
	const TMap<FGameplayTag, FARDialogueSpeakerRow>& SpeakerRowsByTag,
	TMap<EARPlayerSlot, FGameplayTagContainer>& SeenByPlayerTransient,
	const FGameplayTagContainer& SeenByGameTransient,
	const bool bAdvanceLineInput,
	const bool bPreviewMode = false,
	const FDialogueRuntimeContext* PreviewContextOverride = nullptr)
{
	if (!DialogueSubsystem || !Session.ConversationAsset)
	{
		return EDialogueExecutionResult::Failed;
	}

	const UARDialogueSettings* Settings = GetDefault<UARDialogueSettings>();
	const int32 MaxSteps = Settings ? FMath::Max(16, Settings->MaxExecutionStepsPerAdvance) : 1024;

	auto LogRuntimeError = [&](const FString& Message)
	{
		UE_LOG(ARLog, Error, TEXT("[Dialogue] %s (Conversation=%s Session=%s Node=%s)."),
			*Message,
			*Session.ConversationTag.ToString(),
			*Session.SessionId,
			*Session.CurrentNodeId.ToString(EGuidFormats::DigitsWithHyphensInBraces));
	};

	auto LogRuntimeWarning = [&](const FString& Message)
	{
		UE_LOG(ARLog, Warning, TEXT("[Dialogue] %s (Conversation=%s Session=%s Node=%s)."),
			*Message,
			*Session.ConversationTag.ToString(),
			*Session.SessionId,
			*Session.CurrentNodeId.ToString(EGuidFormats::DigitsWithHyphensInBraces));
	};

	if (Session.bWaitingForAdvanceInput)
	{
		if (!bAdvanceLineInput || !Session.WaitingLineNodeId.IsValid())
		{
			return EDialogueExecutionResult::Waiting;
		}

		const FDialogueCompiledNode* WaitingLineNode = FindNodeById(Session, Session.WaitingLineNodeId);
		if (!WaitingLineNode)
		{
			LogRuntimeError(TEXT("Waiting line node could not be resolved during advance."));
			return EDialogueExecutionResult::Failed;
		}

		ClearSessionPresentationState(Session);
		Session.CurrentNodeId = WaitingLineNode->NextNodeId;
		if (!Session.CurrentNodeId.IsValid())
		{
			LogRuntimeWarning(TEXT("Line node had no Next link during advance; ending non-completed."));
			return EDialogueExecutionResult::EndedNonCompleted;
		}
	}

	for (int32 StepIndex = 0; StepIndex < MaxSteps; ++StepIndex)
	{
		const FDialogueCompiledNode* Node = FindNodeById(Session, Session.CurrentNodeId);
		if (!Node)
		{
			LogRuntimeError(TEXT("Current node could not be resolved from compiled data."));
			return EDialogueExecutionResult::Failed;
		}

		FDialogueRuntimeContext Context = bPreviewMode && PreviewContextOverride
			? *PreviewContextOverride
			: BuildSessionContext(DialogueSubsystem, Session, SeenByPlayerTransient, SeenByGameTransient);
		Context.ConversationTag = Session.ConversationTag;
		Context.ConversationAsset = Session.ConversationAsset;
		Context.PrimarySpeakerTag = Session.PrimarySpeakerTag;
		Context.TransientConversationTags = Session.TransientConversationTags;

		switch (Node->NodeType)
		{
		case EDialogueNodeType::Enter:
		{
			if (!Node->NextNodeId.IsValid())
			{
				LogRuntimeWarning(TEXT("Enter node has no outgoing connection; ending non-completed."));
				return EDialogueExecutionResult::EndedNonCompleted;
			}
			Session.CurrentNodeId = Node->NextNodeId;
			break;
		}
		case EDialogueNodeType::Completed:
		{
			if (!bPreviewMode)
			{
				PersistCompletedConversation(DialogueSubsystem, Session);
			}
			return EDialogueExecutionResult::EndedCompleted;
		}
		case EDialogueNodeType::Line:
		{
			const FDialogueLineNodeData* LineData = Node->NodeData.GetPtr<FDialogueLineNodeData>();
			if (!LineData)
			{
				LogRuntimeError(TEXT("Line node payload missing."));
				return EDialogueExecutionResult::Failed;
			}

			const bool bShowLine = PassesLockedConditions(DialogueSubsystem, LineData->SkipLockedConditions, Context)
				&& PassesBlockedConditions(DialogueSubsystem, LineData->SkipBlockedConditions, Context);
			if (!bShowLine)
			{
				Session.CurrentNodeId = Node->NextNodeId;
				if (!Session.CurrentNodeId.IsValid())
				{
					LogRuntimeWarning(TEXT("Line node skip had no Next link; ending non-completed."));
					return EDialogueExecutionResult::EndedNonCompleted;
				}
				break;
			}

			ClearSessionPresentationState(Session);

			FGameplayTag ResolvedSpeakerTag = LineData->Line.SpeakerTag;
			if (ResolvedSpeakerTag.IsValid() && GetDialogueSpeakerPlayerPlaceholderTag().IsValid()
				&& ResolvedSpeakerTag.MatchesTagExact(GetDialogueSpeakerPlayerPlaceholderTag())
				&& Context.ResolvedPlayerSpeakerTag.IsValid())
			{
				ResolvedSpeakerTag = Context.ResolvedPlayerSpeakerTag;
			}

			Session.CurrentSpeakerTag = ResolvedSpeakerTag;
			Session.CurrentLineText = LineData->Line.Text;
			Session.CurrentSpeakerPortrait = ResolvePortraitForSpeaker(SpeakerRowsByTag, ResolvedSpeakerTag);
			Session.bWaitingForAdvanceInput = true;
			Session.WaitingLineNodeId = Node->NodeId;

			const AARPlayerStateBase* ActiveARPlayerState = Cast<AARPlayerStateBase>(Context.ActivePlayerState);
			const bool bAutoAdvanceEnabledForOwner = !bPreviewMode
				&& ActiveARPlayerState
				&& ActiveARPlayerState->IsDialogueAutoAdvanceEnabled();
			if (bAutoAdvanceEnabledForOwner && Context.World)
			{
				float DelaySeconds = LineData->Line.LengthSeconds;
				if (LineData->Line.Sound)
				{
					const float SoundDuration = LineData->Line.Sound->GetDuration();
					if (SoundDuration > 0.0f)
					{
						DelaySeconds = SoundDuration;
					}
				}
				DelaySeconds = FMath::Max(0.05f, DelaySeconds);
				FTimerDelegate AutoAdvanceDelegate = FTimerDelegate::CreateLambda(
					[WeakSubsystem = TWeakObjectPtr<UARDialogueSubsystem>(DialogueSubsystem), OwnerSlot = Session.OwnerSlot]()
					{
						if (UARDialogueSubsystem* Pinned = WeakSubsystem.Get())
						{
							if (AARPlayerController* OwnerController = FindPlayerControllerBySlot(Pinned->GetWorld(), OwnerSlot))
							{
								Pinned->AdvanceConversation(OwnerController);
							}
						}
					});

				Context.World->GetTimerManager().SetTimer(Session.AutoAdvanceTimerHandle, AutoAdvanceDelegate, DelaySeconds, false);
			}

			return EDialogueExecutionResult::Waiting;
		}
		case EDialogueNodeType::Choice:
		{
			ClearSessionPresentationState(Session);

			const FARPlayerIdentity OwnerIdentity = BuildOwnerIdentityForSession(Context.World, Session);
			if (Node->CompletedChoicePolicy == EDialogueCompletedChoicePolicy::LockedToRecordedChoice)
			{
				const UARSaveGame* SaveGame = GetCurrentSave(DialogueSubsystem);
				const FDialoguePlayerPersistentState* PlayerState = FindPlayerDialogueState(SaveGame, OwnerIdentity);
				const bool bConversationCompletedForOwner = PlayerState && PlayerState->CompletedConversationTags.HasTagExact(Session.ConversationTag);
				bool bFoundRecordedChoiceForNode = false;
				if (PlayerState)
				{
					for (const FDialogueChoiceMemoryRecord& Record : PlayerState->CompletedChoiceRecords)
					{
						if (!Record.ConversationTag.MatchesTagExact(Session.ConversationTag) || Record.ChoiceNodeId != Node->NodeId)
						{
							continue;
						}

						bFoundRecordedChoiceForNode = true;
						const FDialogueCompiledChoiceBranch* RecordedBranch = Node->ChoiceBranches.FindByPredicate(
							[&Record](const FDialogueCompiledChoiceBranch& Branch)
							{
								return Branch.ChoiceBranchId == Record.SelectedBranchId;
							});
						if (RecordedBranch && RecordedBranch->NextNodeId.IsValid())
						{
							Session.RuntimeChoiceSelections.Add(Node->NodeId, RecordedBranch->ChoiceBranchId);
							Session.CurrentNodeId = RecordedBranch->NextNodeId;
							goto NextStep;
						}

						LogRuntimeWarning(TEXT("Locked choice record points to a missing or unlinked branch; ending non-completed to enforce locked policy."));
						return EDialogueExecutionResult::EndedNonCompleted;
					}
				}

				if (bConversationCompletedForOwner && !bFoundRecordedChoiceForNode)
				{
					LogRuntimeWarning(TEXT("Locked choice policy requires a recorded branch for completed conversations; ending non-completed."));
					return EDialogueExecutionResult::EndedNonCompleted;
				}
			}

			for (const FDialogueCompiledChoiceBranch& Branch : Node->ChoiceBranches)
			{
				if (!PassesLockedConditions(DialogueSubsystem, Branch.LockedConditions, Context)
					|| !PassesBlockedConditions(DialogueSubsystem, Branch.BlockedConditions, Context))
				{
					continue;
				}

				FDialogueChoiceView& ChoiceView = Session.CurrentChoices.AddDefaulted_GetRef();
				ChoiceView.ChoiceBranchId = Branch.ChoiceBranchId;
				ChoiceView.ChoiceText = Branch.ChoiceText;
				ChoiceView.bCanChoose = true;
				ChoiceView.bImportant = Branch.bImportant;
			}

			bool bForceAllPlayersView = Session.bConversationImportant || Node->bChoiceNodeImportant;
			for (const FDialogueChoiceView& Choice : Session.CurrentChoices)
			{
				if (Choice.bImportant)
				{
					bForceAllPlayersView = true;
					break;
				}
			}

			if (bForceAllPlayersView)
			{
				const TArray<EARPlayerSlot> SlottedPlayers = GetAllSlottedPlayers(Context.World);
				for (const EARPlayerSlot Slot : SlottedPlayers)
				{
					AddSessionParticipant(Session, SeenByPlayerTransient, Slot);
				}
			}

			if (Session.CurrentChoices.IsEmpty())
			{
				if (!Node->FallbackNodeId.IsValid())
				{
					UE_LOG(ARLog, Error, TEXT("[Dialogue] Choice node '%s' has no valid choices and no fallback branch."), *Node->NodeId.ToString(EGuidFormats::DigitsWithHyphensInBraces));
					return EDialogueExecutionResult::EndedNonCompleted;
				}

				Session.CurrentNodeId = Node->FallbackNodeId;
				break;
			}

			Session.bWaitingForChoice = true;
			Session.WaitingChoiceNodeId = Node->NodeId;
			Session.bChoiceRequiresAllViewers = bForceAllPlayersView;
			return EDialogueExecutionResult::Waiting;
		}
		case EDialogueNodeType::Bool:
		{
			const FDialogueBoolNodeData* BoolData = Node->NodeData.GetPtr<FDialogueBoolNodeData>();
			if (!BoolData)
			{
				LogRuntimeError(TEXT("Bool node payload missing."));
				return EDialogueExecutionResult::Failed;
			}

			const bool bConditionPassed = DialogueSubsystem->EvaluateDialogueCondition(BoolData->Condition, Context);
			Session.CurrentNodeId = bConditionPassed ? Node->TrueNodeId : Node->FalseNodeId;
			if (!Session.CurrentNodeId.IsValid())
			{
				LogRuntimeWarning(TEXT("Bool node branch has no connection; ending non-completed."));
				return EDialogueExecutionResult::EndedNonCompleted;
			}
			break;
		}
		case EDialogueNodeType::SwitchOnTagsByPriority:
		{
			bool bMatched = false;
			for (const FDialogueCompiledSwitchBranch& Branch : Node->SwitchBranches)
			{
				if (PassesLockedConditions(DialogueSubsystem, Branch.LockedConditions, Context)
					&& PassesBlockedConditions(DialogueSubsystem, Branch.BlockedConditions, Context))
				{
					Session.CurrentNodeId = Branch.NextNodeId;
					if (!Session.CurrentNodeId.IsValid())
					{
						LogRuntimeError(TEXT("Switch branch selected but has no connection."));
						return EDialogueExecutionResult::Failed;
					}
					bMatched = true;
					break;
				}
			}

			if (!bMatched)
			{
				if (!Node->bSwitchHasDefaultOutput || !Node->SwitchDefaultNodeId.IsValid())
				{
					UE_LOG(ARLog, Error, TEXT("[Dialogue] Switch node '%s' has no matching branch and no default output."), *Node->NodeId.ToString(EGuidFormats::DigitsWithHyphensInBraces));
					return EDialogueExecutionResult::EndedNonCompleted;
				}
				Session.CurrentNodeId = Node->SwitchDefaultNodeId;
			}
			break;
		}
		case EDialogueNodeType::TagMutation:
		{
			const FDialogueTagMutationNodeData* MutationData = Node->NodeData.GetPtr<FDialogueTagMutationNodeData>();
			if (!MutationData)
			{
				LogRuntimeError(TEXT("Tag mutation node payload missing."));
				return EDialogueExecutionResult::Failed;
			}

			for (const FDialogueTagMutation& Mutation : MutationData->Mutations)
			{
				if (Mutation.Target == EDialogueTagMutationTarget::ActivePlayerTransientConversation)
				{
					if (Mutation.Operation == EDialogueTagMutationOp::Add)
					{
						Session.TransientConversationTags.AddTag(Mutation.Tag);
					}
					else
					{
						Session.TransientConversationTags.RemoveTag(Mutation.Tag);
					}
				}
				else
				{
					if (!bPreviewMode)
					{
						DialogueSubsystem->ApplyDialogueTagMutation(Mutation, Context);
					}
				}
			}

			Session.CurrentNodeId = Node->NextNodeId;
			if (!Session.CurrentNodeId.IsValid())
			{
				LogRuntimeWarning(TEXT("Tag mutation node has no Next link; ending non-completed."));
				return EDialogueExecutionResult::EndedNonCompleted;
			}
			break;
		}
		case EDialogueNodeType::RelationshipMutation:
		{
			const FDialogueRelationshipMutationNodeData* MutationData = Node->NodeData.GetPtr<FDialogueRelationshipMutationNodeData>();
			if (!MutationData)
			{
				LogRuntimeError(TEXT("Relationship mutation node payload missing."));
				return EDialogueExecutionResult::Failed;
			}

			if (!bPreviewMode)
			{
				DialogueSubsystem->ApplyDialogueRelationshipMutation(*MutationData, Context);
			}
			Session.CurrentNodeId = Node->NextNodeId;
			if (!Session.CurrentNodeId.IsValid())
			{
				LogRuntimeWarning(TEXT("Relationship mutation node has no Next link; ending non-completed."));
				return EDialogueExecutionResult::EndedNonCompleted;
			}
			break;
		}
		case EDialogueNodeType::FactionMutation:
		{
			const FDialogueFactionMutationNodeData* MutationData = Node->NodeData.GetPtr<FDialogueFactionMutationNodeData>();
			if (!MutationData)
			{
				LogRuntimeError(TEXT("Faction mutation node payload missing."));
				return EDialogueExecutionResult::Failed;
			}

			if (!bPreviewMode)
			{
				DialogueSubsystem->ApplyDialogueFactionMutation(*MutationData, Context);
			}
			Session.CurrentNodeId = Node->NextNodeId;
			if (!Session.CurrentNodeId.IsValid())
			{
				LogRuntimeWarning(TEXT("Faction mutation node has no Next link; ending non-completed."));
				return EDialogueExecutionResult::EndedNonCompleted;
			}
			break;
		}
		case EDialogueNodeType::Random:
		{
			float TotalWeight = 0.0f;
			for (const FDialogueCompiledRandomBranch& Branch : Node->RandomBranches)
			{
				if (Branch.NextNodeId.IsValid() && Branch.Weight > 0.0f)
				{
					TotalWeight += Branch.Weight;
				}
			}

			if (TotalWeight <= 0.0f)
			{
				LogRuntimeWarning(TEXT("Random node has no positive-weight branches at runtime; ending non-completed."));
				return EDialogueExecutionResult::EndedNonCompleted;
			}

			const float Pick = FMath::FRandRange(0.0f, TotalWeight);
			float Running = 0.0f;
			bool bRouted = false;
			for (const FDialogueCompiledRandomBranch& Branch : Node->RandomBranches)
			{
				if (!Branch.NextNodeId.IsValid() || Branch.Weight <= 0.0f)
				{
					continue;
				}

				Running += Branch.Weight;
				if (Pick <= Running)
				{
					Session.CurrentNodeId = Branch.NextNodeId;
					bRouted = true;
					break;
				}
			}

			if (!bRouted)
			{
				LogRuntimeWarning(TEXT("Random node failed to route despite positive weight; ending non-completed."));
				return EDialogueExecutionResult::EndedNonCompleted;
			}
			break;
		}
		default:
			LogRuntimeError(TEXT("Encountered unknown dialogue node type at runtime."));
			return EDialogueExecutionResult::Failed;
		}

NextStep:
		continue;
	}

	UE_LOG(ARLog, Error, TEXT("[Dialogue] Execution exceeded max steps for conversation '%s'."), *Session.ConversationTag.ToString());
	return EDialogueExecutionResult::Failed;
}

static bool ApplyPreviewAutoChoice(FARActiveDialogueSession& Session, FGuid& OutSelectedChoiceBranchId)
{
	OutSelectedChoiceBranchId.Invalidate();
	if (!Session.bWaitingForChoice || !Session.WaitingChoiceNodeId.IsValid())
	{
		return false;
	}

	const FDialogueChoiceView* VisibleChoice = Session.CurrentChoices.FindByPredicate([](const FDialogueChoiceView& ChoiceView)
	{
		return ChoiceView.bCanChoose && ChoiceView.ChoiceBranchId.IsValid();
	});
	if (!VisibleChoice)
	{
		return false;
	}

	const FDialogueCompiledNode* ChoiceNode = FindNodeById(Session, Session.WaitingChoiceNodeId);
	if (!ChoiceNode)
	{
		return false;
	}

	const FDialogueCompiledChoiceBranch* SelectedBranch = ChoiceNode->ChoiceBranches.FindByPredicate(
		[VisibleChoice](const FDialogueCompiledChoiceBranch& Branch)
		{
			return Branch.ChoiceBranchId == VisibleChoice->ChoiceBranchId;
		});
	if (!SelectedBranch || !SelectedBranch->NextNodeId.IsValid())
	{
		return false;
	}

	OutSelectedChoiceBranchId = VisibleChoice->ChoiceBranchId;
	ClearSessionPresentationState(Session);
	Session.RuntimeChoiceSelections.Add(ChoiceNode->NodeId, VisibleChoice->ChoiceBranchId);
	Session.CurrentNodeId = SelectedBranch->NextNodeId;
	return true;
}

static void RemoveSessionAt(UARDialogueSubsystem* DialogueSubsystem, TArray<FARActiveDialogueSession>& Sessions, const int32 SessionIndex)
{
	if (!DialogueSubsystem || !Sessions.IsValidIndex(SessionIndex))
	{
		return;
	}

	const FString SessionId = Sessions[SessionIndex].SessionId;
	const TSet<EARPlayerSlot> ParticipantSlots = Sessions[SessionIndex].Participants;
	if (UWorld* World = DialogueSubsystem->GetWorld())
	{
		World->GetTimerManager().ClearTimer(Sessions[SessionIndex].AutoAdvanceTimerHandle);
	}

	UE_LOG(ARLog, Verbose, TEXT("[Dialogue] Session '%s' removed (Participants=%d)."), *SessionId, ParticipantSlots.Num());
	Sessions.RemoveAtSwap(SessionIndex, 1, EAllowShrinking::No);
	DialogueSubsystem->OnDialogueSessionEnded.Broadcast(SessionId);
	for (const EARPlayerSlot Slot : ParticipantSlots)
	{
		if (AARPlayerController* TargetController = FindPlayerControllerBySlot(DialogueSubsystem->GetWorld(), Slot))
		{
			TargetController->ClientDialogueSessionEnded(SessionId);
		}
	}
}

bool UARDialogueSubsystem::TryStartDialogueWithNpc(AARPlayerController* RequestingController, FGameplayTag PrimarySpeakerTag)
{
	FDialogueConversationOffer Offer;
	if (!GetAvailableConversationForNPC(RequestingController, PrimarySpeakerTag, Offer, /*bNpcLocalStateAllowsDialogue=*/ true))
	{
		return false;
	}
	return StartConversation(RequestingController, Offer.ConversationTag, PrimarySpeakerTag);
}

bool UARDialogueSubsystem::StartConversation(AARPlayerController* RequestingController, FGameplayTag ConversationTag, FGameplayTag PrimarySpeakerTag)
{
	UWorld* World = GetWorld();
	if (!IsAuthorityWorld_Dialogue(World) || !RequestingController || !ConversationTag.IsValid())
	{
		UE_LOG(ARLog, Verbose, TEXT("[Dialogue] StartConversation rejected: invalid params or not authority (Controller=%s Tag=%s)."),
			*GetNameSafe(RequestingController),
			*ConversationTag.ToString());
		return false;
	}

	FARDialogueRuntimeState& Runtime = GetRuntimeState();
	UARDialogueConversationAsset* Conversation = Runtime.ConversationsByTag.FindRef(ConversationTag);
	if (!Conversation)
	{
		UE_LOG(ARLog, Warning, TEXT("[Dialogue] StartConversation failed: conversation '%s' not registered."), *ConversationTag.ToString());
		return false;
	}

	FDialogueValidationReport Validation;
	if (!ValidateConversation(Conversation, Validation))
	{
		UE_LOG(ARLog, Error,
			TEXT("[Dialogue] StartConversation rejected invalid conversation '%s' (%d issues)."),
			*ConversationTag.ToString(),
			Validation.Issues.Num());
		return false;
	}

	if (!PrimarySpeakerTag.IsValid())
	{
		PrimarySpeakerTag = Conversation->Header.PrimarySpeakerTag;
	}
	if (!PrimarySpeakerTag.MatchesTagExact(Conversation->Header.PrimarySpeakerTag))
	{
		UE_LOG(ARLog, Verbose,
			TEXT("[Dialogue] StartConversation rejected: PrimarySpeaker mismatch (requested %s, asset %s)."),
			*PrimarySpeakerTag.ToString(),
			*Conversation->Header.PrimarySpeakerTag.ToString());
		return false;
	}

	AARPlayerStateBase* RequesterPS = RequestingController->GetPlayerState<AARPlayerStateBase>();
	if (!RequesterPS)
	{
		UE_LOG(ARLog, Verbose, TEXT("[Dialogue] StartConversation rejected: RequestingController has no PlayerState."));
		return false;
	}
	const EARPlayerSlot RequesterSlot = RequesterPS->GetPlayerSlot();
	if (RequesterSlot == EARPlayerSlot::Unknown)
	{
		UE_LOG(ARLog, Verbose, TEXT("[Dialogue] StartConversation rejected: player slot is Unknown."));
		return false;
	}

	const FARPlayerIdentity RequesterIdentity = BuildPlayerIdentityFromState(RequesterPS);
	FDialogueRuntimeContext StartContext = BuildOfferContext(this, Conversation, RequesterPS, RequesterIdentity);
	StartContext.bSeenByGame = Runtime.SeenByGameTransient.HasTagExact(ConversationTag);
	if (const FGameplayTagContainer* SeenTags = Runtime.SeenByPlayerTransient.Find(RequesterSlot))
	{
		StartContext.bSeenByPlayer = SeenTags->HasTagExact(ConversationTag);
	}

	FString StartGateFailure;
	if (!EvaluateConversationOfferRules(this, StartContext, Conversation->Header, &StartGateFailure))
	{
		UE_LOG(ARLog, Verbose, TEXT("[Dialogue] StartConversation gated out for '%s': %s"), *ConversationTag.ToString(), *StartGateFailure);
		return false;
	}

	if (!Conversation->Header.bRepeatable && StartContext.bCompletedByPlayer)
	{
		UE_LOG(ARLog, Verbose, TEXT("[Dialogue] StartConversation blocked: non-repeatable already completed by player '%s'."), *ConversationTag.ToString());
		return false;
	}
	if (Conversation->Header.bCompletedByGameBlocksReoffer && StartContext.bCompletedByGame)
	{
		UE_LOG(ARLog, Verbose, TEXT("[Dialogue] StartConversation blocked: completed-by-game suppression for '%s'."), *ConversationTag.ToString());
		return false;
	}
	if (Conversation->Header.bSeenByGameBlocksReoffer && StartContext.bSeenByGame)
	{
		UE_LOG(ARLog, Verbose, TEXT("[Dialogue] StartConversation blocked: seen-by-game suppression for '%s'."), *ConversationTag.ToString());
		return false;
	}
	if (Conversation->Header.bSeenByPlayerBlocksReoffer && StartContext.bSeenByPlayer)
	{
		UE_LOG(ARLog, Verbose, TEXT("[Dialogue] StartConversation blocked: seen-by-player suppression for '%s'."), *ConversationTag.ToString());
		return false;
	}

	const UARDialogueSettings* Settings = GetDefault<UARDialogueSettings>();
	const FGameplayTag ModeTag = GetCurrentModeTag(World);
	if (!IsModeDialogueEnabled(Settings, ModeTag))
	{
		UE_LOG(ARLog, Verbose, TEXT("[Dialogue] StartConversation rejected: mode '%s' not dialogue-enabled."), *ModeTag.ToString());
		return false;
	}

	const bool bSharedMode = Settings && IsModeInContainer(ModeTag, Settings->SharedDialogueModeTags);

	if (bSharedMode)
	{
		if (FARActiveDialogueSession* Existing = FindSharedSession(Runtime.ActiveSessions))
		{
			if (Existing->ConversationTag.MatchesTagExact(ConversationTag))
			{
				AddSessionParticipant(*Existing, Runtime.SeenByPlayerTransient, RequesterSlot);
				BroadcastSessionUpdated(this, *Existing);
				UE_LOG(ARLog, Verbose, TEXT("[Dialogue] StartConversation: player joined existing shared session for '%s'."), *ConversationTag.ToString());
				return true;
			}
			UE_LOG(ARLog, Verbose, TEXT("[Dialogue] StartConversation rejected: shared session already running different conversation '%s'."), *Existing->ConversationTag.ToString());
			return false;
		}
	}
	else if (FindSessionByOwnerSlot(Runtime.ActiveSessions, RequesterSlot))
	{
		UE_LOG(ARLog, Verbose, TEXT("[Dialogue] StartConversation rejected: slot already owns a session."));
		return false;
	}

	FARActiveDialogueSession Session;
	Session.SessionId = BuildSessionId();
	Session.ConversationTag = ConversationTag;
	Session.PrimarySpeakerTag = PrimarySpeakerTag;
	Session.ConversationAsset = Conversation;
	Session.CurrentNodeId = Conversation->CompiledData.EnterNodeId;
	Session.InitiatorSlot = RequesterSlot;
	Session.OwnerSlot = RequesterSlot;
	Session.bIsSharedSession = bSharedMode;
	Session.bConversationImportant = Conversation->Header.bImportant;
	AddSessionParticipant(Session, Runtime.SeenByPlayerTransient, RequesterSlot);

	Runtime.SeenByGameTransient.AddTag(ConversationTag);

	// Important conversations force all slotted players into passive viewing.
	if (Session.bConversationImportant)
	{
		const TArray<EARPlayerSlot> SlottedPlayers = GetAllSlottedPlayers(World);
		for (const EARPlayerSlot Slot : SlottedPlayers)
		{
			AddSessionParticipant(Session, Runtime.SeenByPlayerTransient, Slot);
		}
	}

	Runtime.ActiveSessions.Add(MoveTemp(Session));
	const int32 NewSessionIndex = Runtime.ActiveSessions.Num() - 1;
	EDialogueExecutionResult Result = ExecuteSessionUntilWait(
		this,
		Runtime.ActiveSessions[NewSessionIndex],
		Runtime.SpeakerRowsByTag,
		Runtime.SeenByPlayerTransient,
		Runtime.SeenByGameTransient,
		false);
	if (Result == EDialogueExecutionResult::Waiting)
	{
		BroadcastSessionUpdated(this, Runtime.ActiveSessions[NewSessionIndex]);
		UE_LOG(ARLog, Verbose, TEXT("[Dialogue] StartConversation: session '%s' started for '%s' (slot %s)."),
			*Runtime.ActiveSessions[NewSessionIndex].SessionId,
			*ConversationTag.ToString(),
			*StaticEnum<EARPlayerSlot>()->GetNameStringByValue(static_cast<int64>(RequesterSlot)));
		return true;
	}

	UE_LOG(ARLog, Verbose, TEXT("[Dialogue] StartConversation auto-ended during initial execution for '%s' (Result=%d)."),
		*ConversationTag.ToString(),
		static_cast<int32>(Result));
	RemoveSessionAt(this, Runtime.ActiveSessions, NewSessionIndex);
	return Result == EDialogueExecutionResult::EndedCompleted || Result == EDialogueExecutionResult::EndedNonCompleted;
}

bool UARDialogueSubsystem::AdvanceConversation(AARPlayerController* RequestingController)
{
	UWorld* World = GetWorld();
	if (!IsAuthorityWorld_Dialogue(World) || !RequestingController)
	{
		UE_LOG(ARLog, Verbose, TEXT("[Dialogue] Advance rejected: not authority or invalid controller."));
		return false;
	}

	AARPlayerStateBase* RequesterPS = RequestingController->GetPlayerState<AARPlayerStateBase>();
	if (!RequesterPS)
	{
		UE_LOG(ARLog, Verbose, TEXT("[Dialogue] Advance rejected: controller has no PlayerState."));
		return false;
	}

	FARDialogueRuntimeState& Runtime = GetRuntimeState();
	const int32 SessionIndex = FindSessionIndexForSlot(Runtime.ActiveSessions, RequesterPS->GetPlayerSlot());
	if (!Runtime.ActiveSessions.IsValidIndex(SessionIndex))
	{
		UE_LOG(ARLog, Verbose, TEXT("[Dialogue] Advance rejected: no active session for slot %s."),
			*StaticEnum<EARPlayerSlot>()->GetNameStringByValue(static_cast<int64>(RequesterPS->GetPlayerSlot())));
		return false;
	}

	FARActiveDialogueSession& Session = Runtime.ActiveSessions[SessionIndex];
	if (RequesterPS->GetPlayerSlot() != Session.OwnerSlot)
	{
		UE_LOG(ARLog, Verbose, TEXT("[Dialogue] Advance rejected: slot %s is not the owner of session '%s'."),
			*StaticEnum<EARPlayerSlot>()->GetNameStringByValue(static_cast<int64>(RequesterPS->GetPlayerSlot())),
			*Session.SessionId);
		return false;
	}

	if (Session.bWaitingForChoice)
	{
		UE_LOG(ARLog, Verbose, TEXT("[Dialogue] Advance rejected: session '%s' waiting for choice."), *Session.SessionId);
		return false;
	}

	EDialogueExecutionResult Result = ExecuteSessionUntilWait(
		this,
		Session,
		Runtime.SpeakerRowsByTag,
		Runtime.SeenByPlayerTransient,
		Runtime.SeenByGameTransient,
		true);
	if (Result == EDialogueExecutionResult::Waiting)
	{
		BroadcastSessionUpdated(this, Session);
		return true;
	}

	UE_LOG(ARLog, Verbose, TEXT("[Dialogue] Session '%s' ended on advance (Result=%d)."),
		*Session.SessionId,
		static_cast<int32>(Result));
	RemoveSessionAt(this, Runtime.ActiveSessions, SessionIndex);
	return Result == EDialogueExecutionResult::EndedCompleted || Result == EDialogueExecutionResult::EndedNonCompleted;
}

bool UARDialogueSubsystem::SubmitChoice(AARPlayerController* RequestingController, FGuid ChoiceBranchId)
{
	UWorld* World = GetWorld();
	if (!IsAuthorityWorld_Dialogue(World) || !RequestingController || !ChoiceBranchId.IsValid())
	{
		UE_LOG(ARLog, Verbose, TEXT("[Dialogue] SubmitChoice rejected: invalid params or not authority (Controller=%s Branch=%s)."),
			*GetNameSafe(RequestingController),
			*ChoiceBranchId.ToString(EGuidFormats::DigitsWithHyphensInBraces));
		return false;
	}

	AARPlayerStateBase* RequesterPS = RequestingController->GetPlayerState<AARPlayerStateBase>();
	if (!RequesterPS)
	{
		UE_LOG(ARLog, Verbose, TEXT("[Dialogue] SubmitChoice rejected: controller has no PlayerState."));
		return false;
	}

	FARDialogueRuntimeState& Runtime = GetRuntimeState();
	const int32 SessionIndex = FindSessionIndexForSlot(Runtime.ActiveSessions, RequesterPS->GetPlayerSlot());
	if (!Runtime.ActiveSessions.IsValidIndex(SessionIndex))
	{
		UE_LOG(ARLog, Verbose, TEXT("[Dialogue] SubmitChoice rejected: no active session for slot %s."),
			*StaticEnum<EARPlayerSlot>()->GetNameStringByValue(static_cast<int64>(RequesterPS->GetPlayerSlot())));
		return false;
	}

	FARActiveDialogueSession& Session = Runtime.ActiveSessions[SessionIndex];
	if (!Session.bWaitingForChoice || !Session.WaitingChoiceNodeId.IsValid())
	{
		UE_LOG(ARLog, Verbose, TEXT("[Dialogue] SubmitChoice rejected: session '%s' not waiting for choice."), *Session.SessionId);
		return false;
	}

	if (RequesterPS->GetPlayerSlot() != Session.OwnerSlot)
	{
		UE_LOG(ARLog, Verbose, TEXT("[Dialogue] SubmitChoice rejected: slot %s is not the owner of session '%s'."),
			*StaticEnum<EARPlayerSlot>()->GetNameStringByValue(static_cast<int64>(RequesterPS->GetPlayerSlot())),
			*Session.SessionId);
		return false;
	}

	const FDialogueCompiledNode* ChoiceNode = FindNodeById(Session, Session.WaitingChoiceNodeId);
	if (!ChoiceNode)
	{
		UE_LOG(ARLog, Error, TEXT("[Dialogue] SubmitChoice failed: missing choice node '%s' in session '%s'."),
			*Session.WaitingChoiceNodeId.ToString(EGuidFormats::DigitsWithHyphensInBraces),
			*Session.SessionId);
		return false;
	}

	const FDialogueCompiledChoiceBranch* SelectedBranch = ChoiceNode->ChoiceBranches.FindByPredicate(
		[&ChoiceBranchId](const FDialogueCompiledChoiceBranch& Branch)
		{
			return Branch.ChoiceBranchId == ChoiceBranchId;
		});
	if (!SelectedBranch || !SelectedBranch->NextNodeId.IsValid())
	{
		UE_LOG(ARLog, Warning, TEXT("[Dialogue] SubmitChoice rejected: branch '%s' not found or unlinked in session '%s'."),
			*ChoiceBranchId.ToString(EGuidFormats::DigitsWithHyphensInBraces),
			*Session.SessionId);
		return false;
	}

	const bool bChoiceWasVisible = Session.CurrentChoices.ContainsByPredicate(
		[&ChoiceBranchId](const FDialogueChoiceView& ChoiceView)
		{
			return ChoiceView.ChoiceBranchId == ChoiceBranchId;
		});
	if (!bChoiceWasVisible)
	{
		UE_LOG(ARLog, Verbose, TEXT("[Dialogue] SubmitChoice rejected: branch '%s' not visible to player."), *ChoiceBranchId.ToString(EGuidFormats::DigitsWithHyphensInBraces));
		return false;
	}

	ClearSessionPresentationState(Session);
	Session.RuntimeChoiceSelections.Add(ChoiceNode->NodeId, ChoiceBranchId);
	Session.CurrentNodeId = SelectedBranch->NextNodeId;

	EDialogueExecutionResult Result = ExecuteSessionUntilWait(
		this,
		Session,
		Runtime.SpeakerRowsByTag,
		Runtime.SeenByPlayerTransient,
		Runtime.SeenByGameTransient,
		false);
	if (Result == EDialogueExecutionResult::Waiting)
	{
		BroadcastSessionUpdated(this, Session);
		return true;
	}

	UE_LOG(ARLog, Verbose, TEXT("[Dialogue] Session '%s' ended after choice (Result=%d)."),
		*Session.SessionId,
		static_cast<int32>(Result));
	RemoveSessionAt(this, Runtime.ActiveSessions, SessionIndex);
	return Result == EDialogueExecutionResult::EndedCompleted || Result == EDialogueExecutionResult::EndedNonCompleted;
}

bool UARDialogueSubsystem::ForceEavesdrop(AARPlayerController* RequestingController, bool bEnable, EARPlayerSlot TargetSlot)
{
	UWorld* World = GetWorld();
	if (!IsAuthorityWorld_Dialogue(World) || !RequestingController)
	{
		UE_LOG(ARLog, Verbose, TEXT("[Dialogue] ForceEavesdrop rejected: not authority or invalid controller."));
		return false;
	}

	AARPlayerStateBase* RequesterPS = RequestingController->GetPlayerState<AARPlayerStateBase>();
	if (!RequesterPS)
	{
		UE_LOG(ARLog, Verbose, TEXT("[Dialogue] ForceEavesdrop rejected: controller has no PlayerState."));
		return false;
	}

	const EARPlayerSlot ViewerSlot = RequesterPS->GetPlayerSlot();
	if (ViewerSlot == EARPlayerSlot::Unknown)
	{
		UE_LOG(ARLog, Verbose, TEXT("[Dialogue] ForceEavesdrop rejected: viewer slot is Unknown."));
		return false;
	}

	FARDialogueRuntimeState& Runtime = GetRuntimeState();
	if (bEnable)
	{
		if (TargetSlot == EARPlayerSlot::Unknown || TargetSlot == ViewerSlot)
		{
			UE_LOG(ARLog, Verbose, TEXT("[Dialogue] ForceEavesdrop rejected: target slot invalid (Viewer=%s Target=%s)."),
				*StaticEnum<EARPlayerSlot>()->GetNameStringByValue(static_cast<int64>(ViewerSlot)),
				*StaticEnum<EARPlayerSlot>()->GetNameStringByValue(static_cast<int64>(TargetSlot)));
			return false;
		}
		Runtime.EavesdropTargetByViewer.Add(ViewerSlot, TargetSlot);
		if (FARActiveDialogueSession* TargetSession = FindSessionByOwnerSlot(Runtime.ActiveSessions, TargetSlot))
		{
			AddSessionParticipant(*TargetSession, Runtime.SeenByPlayerTransient, ViewerSlot);
			BroadcastSessionUpdated(this, *TargetSession);
		}
		UE_LOG(ARLog, Verbose, TEXT("[Dialogue] ForceEavesdrop: viewer %s now eavesdropping owner %s."),
			*StaticEnum<EARPlayerSlot>()->GetNameStringByValue(static_cast<int64>(ViewerSlot)),
			*StaticEnum<EARPlayerSlot>()->GetNameStringByValue(static_cast<int64>(TargetSlot)));
		return true;
	}

	Runtime.EavesdropTargetByViewer.Remove(ViewerSlot);
	for (FARActiveDialogueSession& Session : Runtime.ActiveSessions)
	{
		if (!Session.bIsSharedSession && Session.OwnerSlot != ViewerSlot && !Session.bChoiceRequiresAllViewers)
		{
			Session.Participants.Remove(ViewerSlot);
			BroadcastSessionUpdated(this, Session);
		}
	}
	UE_LOG(ARLog, Verbose, TEXT("[Dialogue] ForceEavesdrop cleared for viewer %s."), *StaticEnum<EARPlayerSlot>()->GetNameStringByValue(static_cast<int64>(ViewerSlot)));
	return true;
}

bool UARDialogueSubsystem::PreviewConversationTrace(
	UARDialogueConversationAsset* ConversationAsset,
	const FDialogueRuntimeContext& PreviewContext,
	int32 MaxInteractiveSteps,
	TArray<FDialogueClientView>& OutViews,
	TArray<FGuid>& OutAutoSelectedChoiceBranchIds,
	bool& bOutEndedCompleted,
	FDialogueValidationReport& OutReport) const
{
	OutViews.Reset();
	OutAutoSelectedChoiceBranchIds.Reset();
	bOutEndedCompleted = false;
	if (!ValidateConversation(ConversationAsset, OutReport))
	{
		return false;
	}

	const int32 StepLimit = FMath::Clamp(MaxInteractiveSteps <= 0 ? 64 : MaxInteractiveSteps, 1, 512);

	FARActiveDialogueSession PreviewSession;
	PreviewSession.SessionId = TEXT("PreviewTrace");
	PreviewSession.ConversationTag = ConversationAsset->Header.ConversationTag;
	PreviewSession.PrimarySpeakerTag = ConversationAsset->Header.PrimarySpeakerTag;
	PreviewSession.ConversationAsset = ConversationAsset;
	PreviewSession.CurrentNodeId = ConversationAsset->CompiledData.EnterNodeId;
	PreviewSession.bConversationImportant = ConversationAsset->Header.bImportant;
	PreviewSession.OwnerSlot = EARPlayerSlot::P1;
	PreviewSession.InitiatorSlot = EARPlayerSlot::P1;
	PreviewSession.Participants.Add(EARPlayerSlot::P1);
	PreviewSession.TransientConversationTags = PreviewContext.TransientConversationTags;

	TMap<EARPlayerSlot, FGameplayTagContainer> PreviewSeenByPlayer;
	FGameplayTagContainer PreviewSeenByGame;
	if (PreviewContext.bSeenByPlayer && PreviewSession.ConversationTag.IsValid())
	{
		PreviewSeenByPlayer.FindOrAdd(EARPlayerSlot::P1).AddTag(PreviewSession.ConversationTag);
	}
	if (PreviewContext.bSeenByGame && PreviewSession.ConversationTag.IsValid())
	{
		PreviewSeenByGame.AddTag(PreviewSession.ConversationTag);
	}

	const FARDialogueRuntimeState& Runtime = GetRuntimeState();
	bool bAdvanceLineInput = false;
	for (int32 StepIndex = 0; StepIndex < StepLimit; ++StepIndex)
	{
		const EDialogueExecutionResult Result = ExecuteSessionUntilWait(
			const_cast<UARDialogueSubsystem*>(this),
			PreviewSession,
			Runtime.SpeakerRowsByTag,
			PreviewSeenByPlayer,
			PreviewSeenByGame,
			bAdvanceLineInput,
			true,
			&PreviewContext);

		bAdvanceLineInput = false;
		if (Result == EDialogueExecutionResult::Waiting)
		{
			FDialogueClientView View;
			FillClientViewForSlot(PreviewSession, EARPlayerSlot::P1, View);
			OutViews.Add(View);

			if (PreviewSession.bWaitingForChoice)
			{
				FGuid AutoSelectedBranchId;
				if (!ApplyPreviewAutoChoice(PreviewSession, AutoSelectedBranchId))
				{
					FDialogueValidationIssue& Issue = OutReport.Issues.AddDefaulted_GetRef();
					Issue.Severity = EDialogueValidationSeverity::Error;
					Issue.NodeId = PreviewSession.WaitingChoiceNodeId;
					Issue.Message = FText::FromString(TEXT("Preview trace failed to auto-select a valid choice branch."));
					return false;
				}

				OutAutoSelectedChoiceBranchIds.Add(AutoSelectedBranchId);
				continue;
			}

			if (PreviewSession.bWaitingForAdvanceInput)
			{
				bAdvanceLineInput = true;
				continue;
			}

			FDialogueValidationIssue& Issue = OutReport.Issues.AddDefaulted_GetRef();
			Issue.Severity = EDialogueValidationSeverity::Warning;
			Issue.NodeId = PreviewSession.CurrentNodeId;
			Issue.Message = FText::FromString(TEXT("Preview trace reached an unknown waiting state and stopped early."));
			return true;
		}

		if (Result == EDialogueExecutionResult::EndedCompleted)
		{
			bOutEndedCompleted = true;
			return true;
		}

		if (Result == EDialogueExecutionResult::EndedNonCompleted)
		{
			bOutEndedCompleted = false;
			return true;
		}

		FDialogueValidationIssue& Issue = OutReport.Issues.AddDefaulted_GetRef();
		Issue.Severity = EDialogueValidationSeverity::Error;
		Issue.NodeId = PreviewSession.CurrentNodeId;
		Issue.Message = FText::FromString(TEXT("Preview trace failed due to invalid runtime graph execution."));
		return false;
	}

	FDialogueValidationIssue& Issue = OutReport.Issues.AddDefaulted_GetRef();
	Issue.Severity = EDialogueValidationSeverity::Warning;
	Issue.Message = FText::FromString(TEXT("Preview trace hit the interactive-step cap and stopped early."));
	return true;
}

bool UARDialogueSubsystem::PreviewConversation(UARDialogueConversationAsset* ConversationAsset, const FDialogueRuntimeContext& PreviewContext, FDialogueClientView& OutFirstView, FDialogueValidationReport& OutReport) const
{
	OutFirstView = FDialogueClientView();

	TArray<FDialogueClientView> TraceViews;
	TArray<FGuid> AutoChoiceSelections;
	bool bEndedCompleted = false;
	if (!PreviewConversationTrace(
		ConversationAsset,
		PreviewContext,
		64,
		TraceViews,
		AutoChoiceSelections,
		bEndedCompleted,
		OutReport))
	{
		return false;
	}

	if (!TraceViews.IsEmpty())
	{
		OutFirstView = TraceViews[0];
		return true;
	}

	FDialogueValidationIssue& Issue = OutReport.Issues.AddDefaulted_GetRef();
	Issue.Severity = EDialogueValidationSeverity::Warning;
	Issue.Message = FText::FromString(TEXT("Conversation preview produced no player-visible steps."));
	return true;
}

bool UARDialogueSubsystem::HasUnlockedDialogueForNpcForSlot(FGameplayTag PrimarySpeakerTag, EARPlayerSlot PlayerSlot) const
{
	if (!PrimarySpeakerTag.IsValid() || PlayerSlot == EARPlayerSlot::Unknown)
	{
		return false;
	}

	AARPlayerController* PC = FindPlayerControllerBySlot(GetWorld(), PlayerSlot);
	if (!PC)
	{
		return false;
	}

	FDialogueConversationOffer Offer;
	return const_cast<UARDialogueSubsystem*>(this)->GetAvailableConversationForNPC(PC, PrimarySpeakerTag, Offer, /*bNpcLocalStateAllowsDialogue=*/ true);
}

bool UARDialogueSubsystem::HasUnlockedDialogueForNpcForAnyPlayer(FGameplayTag PrimarySpeakerTag) const
{
	if (!PrimarySpeakerTag.IsValid())
	{
		return false;
	}
	const UWorld* World = GetWorld();
	const AARGameStateBase* GS = World ? World->GetGameState<AARGameStateBase>() : nullptr;
	if (!GS)
	{
		return false;
	}

	for (APlayerState* PS : GS->PlayerArray)
	{
		const AARPlayerStateBase* ARPS = Cast<AARPlayerStateBase>(PS);
		if (!ARPS || ARPS->GetPlayerSlot() == EARPlayerSlot::Unknown)
		{
			continue;
		}
		if (HasUnlockedDialogueForNpcForSlot(PrimarySpeakerTag, ARPS->GetPlayerSlot()))
		{
			return true;
		}
	}

	return false;
}

bool UARDialogueSubsystem::GetLocalViewForController(const AARPlayerController* RequestingController, FDialogueClientView& OutView) const
{
	OutView = FDialogueClientView();
	if (!RequestingController)
	{
		return false;
	}
	const EARPlayerSlot Slot = GetSlotFromController(RequestingController);
	if (Slot == EARPlayerSlot::Unknown)
	{
		return false;
	}
	const FARActiveDialogueSession* Session = FindSessionForSlot(GetRuntimeState().ActiveSessions, Slot);
	if (!Session)
	{
		return false;
	}

	OutView.SessionId = Session->SessionId;
	OutView.ConversationTag = Session->ConversationTag;
	OutView.CurrentNodeId = Session->CurrentNodeId;
	OutView.SpeakerTag = Session->CurrentSpeakerTag;
	OutView.LineText = Session->CurrentLineText;
	OutView.SpeakerPortrait = Session->CurrentSpeakerPortrait;
	OutView.bWaitingForChoice = Session->bWaitingForChoice;
	OutView.bConversationImportant = Session->bConversationImportant;
	OutView.bIsEavesdropping = !Session->bIsSharedSession && Slot != Session->OwnerSlot;
	OutView.InitiatorSlot = Session->InitiatorSlot;
	OutView.OwnerSlot = Session->OwnerSlot;
	OutView.Choices = Session->CurrentChoices;
	return true;
}

bool UARDialogueSubsystem::HasActiveDialogueSession() const
{
	return GetRuntimeState().ActiveSessions.Num() > 0;
}

float UARDialogueSubsystem::GetRelationshipPointsForSpeaker(FGameplayTag SpeakerTag) const
{
	const UARSaveGame* SaveGame = GetCurrentSave(this);
	if (!SaveGame || !SpeakerTag.IsValid())
	{
		return 0.0f;
	}
	for (const FDialogueRelationshipState& State : SaveGame->DialogueRelationshipStates)
	{
		if (State.SpeakerTag.MatchesTagExact(SpeakerTag))
		{
			return State.RelationshipPoints;
		}
	}
	return 0.0f;
}

int32 UARDialogueSubsystem::GetRelationshipLevelForSpeaker(FGameplayTag SpeakerTag) const
{
	const float Points = GetRelationshipPointsForSpeaker(SpeakerTag);
	if (!SpeakerTag.IsValid())
	{
		return 0;
	}

	const FARDialogueRuntimeState& Runtime = GetRuntimeState();
	FGameplayTag CandidateSpeakerTag = SpeakerTag;
	while (CandidateSpeakerTag.IsValid())
	{
		if (const FARDialogueSpeakerRow* SpeakerRow = Runtime.SpeakerRowsByTag.Find(CandidateSpeakerTag))
		{
			return ResolveRelationshipLevelFromThresholds(Points, SpeakerRow->RelationshipThresholds);
		}
		CandidateSpeakerTag = StripLeafGameplayTag(CandidateSpeakerTag);
	}
	return 0;
}
