#include "ARDialogueSubsystem.h"

#include "ARDialogueSettings.h"
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
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameplayTagsManager.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	struct FARActiveDialogueSession
	{
		FString SessionId;
		FGameplayTag NpcTag;
		FGameplayTag CurrentNodeTag;
		EARPlayerSlot InitiatorSlot = EARPlayerSlot::Unknown;
		EARPlayerSlot OwnerSlot = EARPlayerSlot::Unknown;
		bool bIsSharedSession = false;
		bool bWaitingForChoice = false;
		EARDialogueChoiceParticipation ChoiceParticipation = EARDialogueChoiceParticipation::InitiatorOnly;
		bool bForceEavesdropForImportantDecision = false;
		TArray<FARDialogueChoiceDef> CurrentChoices;
		TMap<EARPlayerSlot, FGameplayTag> ChoiceVotes;
		TSet<EARPlayerSlot> Participants;
		FARDialogueNodeRow ActiveRow;
	};

	static bool IsAuthorityWorld(const UWorld* World)
	{
		if (!World)
		{
			return false;
		}
		return World->GetNetMode() == NM_Standalone || World->GetAuthGameMode() != nullptr;
	}

	static bool IsGameplayTagContainerExactSuperset(const FGameplayTagContainer& Container, const FGameplayTagContainer& Required)
	{
		for (const FGameplayTag Tag : Required)
		{
			if (!Container.HasTag(Tag))
			{
				return false;
			}
		}
		return true;
	}

	static bool HasAnyBlockedTag(const FGameplayTagContainer& Container, const FGameplayTagContainer& Blocked)
	{
		for (const FGameplayTag Tag : Blocked)
		{
			if (Container.HasTag(Tag))
			{
				return true;
			}
		}
		return false;
	}

	static FString BuildSessionId()
	{
		return FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensInBraces);
	}

	static bool SortRowsByPriorityThenTag(const FARDialogueNodeRow& A, const FARDialogueNodeRow& B)
	{
		if (A.Priority != B.Priority)
		{
			return A.Priority > B.Priority;
		}

		return A.NodeTag.ToString() < B.NodeTag.ToString();
	}
}

class FARDialogueRuntimeState
{
public:
	TArray<FARActiveDialogueSession> Sessions;
	TMap<EARPlayerSlot, EARPlayerSlot> ShopEavesdropTargetByViewer;
};

static FARDialogueRuntimeState GDialogueRuntimeState;

void UARDialogueSubsystem::Deinitialize()
{
	GDialogueRuntimeState.Sessions.Reset();
	GDialogueRuntimeState.ShopEavesdropTargetByViewer.Reset();
	Super::Deinitialize();
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

static bool BuildDialogueNodeTagFromRootAndLeaf(const FGameplayTag& RootTag, FName LeafRow, FGameplayTag& OutTag)
{
	OutTag = FGameplayTag();
	if (!RootTag.IsValid() || LeafRow.IsNone())
	{
		return false;
	}

	const FString Combined = FString::Printf(TEXT("%s.%s"), *RootTag.ToString(), *LeafRow.ToString());
	OutTag = FGameplayTag::RequestGameplayTag(FName(*Combined), false);
	return OutTag.IsValid();
}

static bool ResolveDialogueRow(UContentLookupSubsystem* Lookup, const FGameplayTag& NodeTag, FARDialogueNodeRow& OutRow)
{
	if (!Lookup || !NodeTag.IsValid())
	{
		return false;
	}

	FInstancedStruct RowData;
	FString Error;
	if (!Lookup->LookupWithGameplayTag(NodeTag, RowData, Error))
	{
		UE_LOG(ARLog, Verbose, TEXT("[Dialogue] Resolve row failed for '%s': %s"), *NodeTag.ToString(), *Error);
		return false;
	}

	if (const FARDialogueNodeRow* Typed = RowData.GetPtr<FARDialogueNodeRow>())
	{
		OutRow = *Typed;
		if (!OutRow.NodeTag.IsValid())
		{
			OutRow.NodeTag = NodeTag;
		}
		return true;
	}

	return false;
}

static bool GetSaveSubsystem(const UARDialogueSubsystem* Subsystem, UARSaveSubsystem*& OutSave)
{
	OutSave = nullptr;
	if (!Subsystem)
	{
		return false;
	}
	if (UGameInstance* GI = Subsystem->GetGameInstance())
	{
		OutSave = GI->GetSubsystem<UARSaveSubsystem>();
	}
	return OutSave != nullptr;
}

static bool GetLookupSubsystem(const UARDialogueSubsystem* Subsystem, UContentLookupSubsystem*& OutLookup)
{
	OutLookup = nullptr;
	if (!Subsystem)
	{
		return false;
	}
	if (UGameInstance* GI = Subsystem->GetGameInstance())
	{
		OutLookup = GI->GetSubsystem<UContentLookupSubsystem>();
	}
	return OutLookup != nullptr;
}

static UARSaveGame* GetMutableCurrentSave(const UARDialogueSubsystem* Subsystem)
{
	UARSaveSubsystem* SaveSubsystem = nullptr;
	if (!GetSaveSubsystem(Subsystem, SaveSubsystem))
	{
		return nullptr;
	}
	return SaveSubsystem->GetCurrentSaveGame();
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
	}
	return Identity;
}

static FARPlayerDialogueHistoryState* FindOrAddHistoryForIdentity(UARSaveGame* SaveGame, const FARPlayerIdentity& Identity)
{
	if (!SaveGame)
	{
		return nullptr;
	}

	for (FARPlayerDialogueHistoryState& Entry : SaveGame->PlayerDialogueHistoryStates)
	{
		if (Entry.Identity.Matches(Identity))
		{
			return &Entry;
		}
	}

	FARPlayerDialogueHistoryState& NewEntry = SaveGame->PlayerDialogueHistoryStates.AddDefaulted_GetRef();
	NewEntry.Identity = Identity;
	return &NewEntry;
}

static FARDialogueCanonicalChoiceState* FindCanonicalChoice(UARSaveGame* SaveGame, const FGameplayTag NodeTag)
{
	if (!SaveGame || !NodeTag.IsValid())
	{
		return nullptr;
	}

	for (FARDialogueCanonicalChoiceState& State : SaveGame->DialogueCanonicalChoiceStates)
	{
		if (State.NodeTag.MatchesTagExact(NodeTag))
		{
			return &State;
		}
	}

	return nullptr;
}

static FARDialogueCanonicalChoiceState* FindOrAddCanonicalChoice(UARSaveGame* SaveGame, const FGameplayTag NodeTag)
{
	if (!SaveGame || !NodeTag.IsValid())
	{
		return nullptr;
	}

	if (FARDialogueCanonicalChoiceState* Existing = FindCanonicalChoice(SaveGame, NodeTag))
	{
		return Existing;
	}

	FARDialogueCanonicalChoiceState& Added = SaveGame->DialogueCanonicalChoiceStates.AddDefaulted_GetRef();
	Added.NodeTag = NodeTag;
	return &Added;
}

static bool IsNodeSeenBySpeaker(const UARDialogueSubsystem* Subsystem, const AARPlayerStateBase* SpeakerState, const FGameplayTag NodeTag)
{
	if (!Subsystem || !SpeakerState || !NodeTag.IsValid())
	{
		return false;
	}

	const UARSaveGame* SaveGame = GetMutableCurrentSave(Subsystem);
	if (!SaveGame)
	{
		return false;
	}

	const FARPlayerIdentity Identity = BuildPlayerIdentityFromState(SpeakerState);
	for (const FARPlayerDialogueHistoryState& Entry : SaveGame->PlayerDialogueHistoryStates)
	{
		if (Entry.Identity.Matches(Identity) && Entry.SeenNodeTags.HasTagExact(NodeTag))
		{
			return true;
		}
	}
	return false;
}

static void MarkNodeSeenForSpeaker(const UARDialogueSubsystem* Subsystem, const AARPlayerStateBase* SpeakerState, const FGameplayTag NodeTag)
{
	if (!Subsystem || !SpeakerState || !NodeTag.IsValid())
	{
		return;
	}

	UARSaveSubsystem* SaveSubsystem = nullptr;
	if (!GetSaveSubsystem(Subsystem, SaveSubsystem))
	{
		return;
	}

	UARSaveGame* SaveGame = SaveSubsystem->GetCurrentSaveGame();
	if (!SaveGame)
	{
		return;
	}

	const FARPlayerIdentity Identity = BuildPlayerIdentityFromState(SpeakerState);
	FARPlayerDialogueHistoryState* History = FindOrAddHistoryForIdentity(SaveGame, Identity);
	if (!History)
	{
		return;
	}

	if (!History->SeenNodeTags.HasTagExact(NodeTag))
	{
		History->SeenNodeTags.AddTag(NodeTag);
		SaveSubsystem->MarkSaveDirty();
	}
}

static bool EvaluateRowUnlocked(const UARDialogueSubsystem* Subsystem, const FARDialogueNodeRow& Row, const AARPlayerStateBase* SpeakerState, const FARNpcRelationshipState* NpcState)
{
	if (!Subsystem || !SpeakerState)
	{
		return false;
	}

	const UWorld* World = Subsystem->GetWorld();
	const AARGameStateBase* GS = World ? World->GetGameState<AARGameStateBase>() : nullptr;
	UARSaveSubsystem* SaveSubsystem = nullptr;
	GetSaveSubsystem(Subsystem, SaveSubsystem);

	const FGameplayTagContainer ProgressionTags = SaveSubsystem ? SaveSubsystem->GetProgressionTags() : FGameplayTagContainer();
	if (!IsGameplayTagContainerExactSuperset(ProgressionTags, Row.RequiredProgressionTags))
	{
		return false;
	}

	if (HasAnyBlockedTag(ProgressionTags, Row.BlockedProgressionTags))
	{
		return false;
	}

	const FGameplayTagContainer Unlocks = GS ? GS->GetUnlocks() : FGameplayTagContainer();
	if (!IsGameplayTagContainerExactSuperset(Unlocks, Row.RequiredUnlockTags))
	{
		return false;
	}

	if (HasAnyBlockedTag(Unlocks, Row.BlockedUnlockTags))
	{
		return false;
	}

	if (NpcState)
	{
		if (NpcState->LoveRating < Row.MinLoveRating)
		{
			return false;
		}

		if (Row.bRequiresWantSatisfied && !NpcState->bCurrentWantSatisfied)
		{
			return false;
		}
	}
	else if (Row.MinLoveRating > 0 || Row.bRequiresWantSatisfied)
	{
		return false;
	}

	if (!Row.bAllowRepeatAfterSeen && IsNodeSeenBySpeaker(Subsystem, SpeakerState, Row.NodeTag))
	{
		return false;
	}

	return true;
}

static bool ResolveNpcState(const UARDialogueSubsystem* Subsystem, const FGameplayTag NpcTag, FARNpcRelationshipState& OutState)
{
	OutState = FARNpcRelationshipState();
	if (!NpcTag.IsValid())
	{
		return false;
	}

	if (UGameInstance* GI = Subsystem ? Subsystem->GetGameInstance() : nullptr)
	{
		if (UARNPCSubsystem* NpcSubsystem = GI->GetSubsystem<UARNPCSubsystem>())
		{
			return NpcSubsystem->TryGetNpcRelationshipState(NpcTag, OutState);
		}
	}

	return false;
}

static bool BuildCandidateRowsForNpc(const UARDialogueSubsystem* Subsystem, const FGameplayTag NpcTag, TArray<FARDialogueNodeRow>& OutRows)
{
	OutRows.Reset();
	if (!Subsystem || !NpcTag.IsValid())
	{
		return false;
	}

	UContentLookupSubsystem* Lookup = nullptr;
	if (!GetLookupSubsystem(Subsystem, Lookup))
	{
		return false;
	}

	const UARDialogueSettings* DialogueSettings = GetDefault<UARDialogueSettings>();
	const FGameplayTag RootTag = DialogueSettings ? DialogueSettings->DialogueNodeRootTag : FGameplayTag();
	if (!RootTag.IsValid())
	{
		return false;
	}

	TArray<FName> RowNames;
	FString Error;
	if (!Lookup->GetAllRowNamesForRootTag(RootTag, RowNames, Error))
	{
		UE_LOG(ARLog, Verbose, TEXT("[Dialogue] GetAllRowNamesForRootTag failed: %s"), *Error);
		return false;
	}

	for (const FName RowName : RowNames)
	{
		FGameplayTag CandidateTag;
		if (!BuildDialogueNodeTagFromRootAndLeaf(RootTag, RowName, CandidateTag))
		{
			continue;
		}

		FARDialogueNodeRow Row;
		if (!ResolveDialogueRow(Lookup, CandidateTag, Row))
		{
			continue;
		}

		if (Row.NpcTag.MatchesTagExact(NpcTag))
		{
			OutRows.Add(MoveTemp(Row));
		}
	}

	OutRows.Sort(&SortRowsByPriorityThenTag);
	return OutRows.Num() > 0;
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

static void NotifyNpcSubsystemTalkableRefresh(const UARDialogueSubsystem* Subsystem, const FGameplayTag NpcTag)
{
	if (!Subsystem || !NpcTag.IsValid())
	{
		return;
	}

	if (UGameInstance* GI = Subsystem->GetGameInstance())
	{
		if (UARNPCSubsystem* NpcSubsystem = GI->GetSubsystem<UARNPCSubsystem>())
		{
			NpcSubsystem->RefreshNpcTalkableState(NpcTag);
		}
	}
}

static FARActiveDialogueSession* FindSessionByOwnerSlot(const EARPlayerSlot Slot)
{
	for (FARActiveDialogueSession& Session : GDialogueRuntimeState.Sessions)
	{
		if (!Session.bIsSharedSession && Session.OwnerSlot == Slot)
		{
			return &Session;
		}
	}
	return nullptr;
}

static FARActiveDialogueSession* FindSharedSession()
{
	for (FARActiveDialogueSession& Session : GDialogueRuntimeState.Sessions)
	{
		if (Session.bIsSharedSession)
		{
			return &Session;
		}
	}
	return nullptr;
}

static FARActiveDialogueSession* FindSessionForSlot(const EARPlayerSlot Slot)
{
	for (FARActiveDialogueSession& Session : GDialogueRuntimeState.Sessions)
	{
		if (Session.Participants.Contains(Slot))
		{
			return &Session;
		}
	}
	return nullptr;
}

static void ApplyProgressionGrants(const UARDialogueSubsystem* Subsystem, const FGameplayTagContainer& Tags)
{
	UARSaveSubsystem* SaveSubsystem = nullptr;
	if (!GetSaveSubsystem(Subsystem, SaveSubsystem))
	{
		return;
	}

	for (const FGameplayTag Tag : Tags)
	{
		SaveSubsystem->AddProgressionTag(Tag);
	}
}

static bool AreAllSlottedPlayersViewingSession(const UARDialogueSubsystem* Subsystem, const FARActiveDialogueSession& Session)
{
	const UWorld* World = Subsystem ? Subsystem->GetWorld() : nullptr;
	const AARGameStateBase* GS = World ? World->GetGameState<AARGameStateBase>() : nullptr;
	if (!GS)
	{
		return false;
	}

	for (APlayerState* PS : GS->PlayerArray)
	{
		const AARPlayerStateBase* ARPS = Cast<AARPlayerStateBase>(PS);
		if (!ARPS)
		{
			continue;
		}

		const EARPlayerSlot Slot = ARPS->GetPlayerSlot();
		if (Slot == EARPlayerSlot::Unknown)
		{
			continue;
		}

		if (!Session.Participants.Contains(Slot))
		{
			return false;
		}
	}

	return true;
}

static FARDialogueClientView BuildViewForSlot(const FARActiveDialogueSession& Session, const EARPlayerSlot Slot)
{
	FARDialogueClientView View;
	View.SessionId = Session.SessionId;
	View.NpcTag = Session.NpcTag;
	View.CurrentNodeTag = Session.CurrentNodeTag;
	View.SpeakerTag = Session.ActiveRow.SpeakerTag;
	View.LineText = Session.ActiveRow.LineText;
	View.SpeakerPortrait = Session.ActiveRow.SpeakerPortrait;
	View.bWaitingForChoice = Session.bWaitingForChoice;
	View.bIsSharedSession = Session.bIsSharedSession;
	View.InitiatorSlot = Session.InitiatorSlot;
	View.OwnerSlot = Session.OwnerSlot;
	View.bIsEavesdropping = !Session.bIsSharedSession && Slot != Session.OwnerSlot;

	for (const FARDialogueChoiceDef& Choice : Session.CurrentChoices)
	{
		FARDialogueChoiceView ChoiceView;
		ChoiceView.ChoiceTag = Choice.ChoiceTag;
		ChoiceView.ChoiceText = Choice.ChoiceText;
		ChoiceView.bCanChoose = true;
		View.Choices.Add(ChoiceView);
	}

	return View;
}

void DispatchSessionUpdate(UARDialogueSubsystem* Subsystem, FARActiveDialogueSession& Session)
{
	if (!Subsystem)
	{
		return;
	}

	for (const EARPlayerSlot Slot : Session.Participants)
	{
		AARPlayerController* PC = FindPlayerControllerBySlot(Subsystem->GetWorld(), Slot);
		if (!PC)
		{
			continue;
		}

		FARDialogueClientView View = BuildViewForSlot(Session, Slot);

		if (Session.bWaitingForChoice)
		{
			const bool bInitiatorOnly = Session.ChoiceParticipation == EARDialogueChoiceParticipation::InitiatorOnly;
			const bool bGroupChoice = Session.ChoiceParticipation == EARDialogueChoiceParticipation::GroupChoice;
			const bool bIsInitiator = Slot == Session.InitiatorSlot;
			bool bCanChoose = bInitiatorOnly ? bIsInitiator : bGroupChoice;
			if (Session.bForceEavesdropForImportantDecision && bGroupChoice)
			{
				bCanChoose = bCanChoose && AreAllSlottedPlayersViewingSession(Subsystem, Session);
			}

			for (FARDialogueChoiceView& ChoiceView : View.Choices)
			{
				ChoiceView.bCanChoose = bCanChoose;
			}
		}

		Subsystem->OnDialogueSessionUpdated.Broadcast(View);
		PC->ClientDialogueSessionUpdated(View);
	}
}

void EndSession(UARDialogueSubsystem* Subsystem, FARActiveDialogueSession& Session)
{
	if (!Subsystem)
	{
		return;
	}

	if (Session.bIsSharedSession)
	{
		if (const UARDialogueSettings* Settings = GetDefault<UARDialogueSettings>())
		{
			if (IsModeInContainer(GetCurrentModeTag(Subsystem->GetWorld()), Settings->PauseOnDialogueModeTags))
			{
				UGameplayStatics::SetGamePaused(Subsystem->GetWorld(), false);
			}
		}
	}

	for (const EARPlayerSlot Slot : Session.Participants)
	{
		if (AARPlayerController* PC = FindPlayerControllerBySlot(Subsystem->GetWorld(), Slot))
		{
			PC->ClientDialogueSessionEnded(Session.SessionId);
		}
	}
	Subsystem->OnDialogueSessionEnded.Broadcast(Session.SessionId);

	NotifyNpcSubsystemTalkableRefresh(Subsystem, Session.NpcTag);
}

static bool TrySelectBestRowForSpeaker(const UARDialogueSubsystem* Subsystem, const FGameplayTag NpcTag, const AARPlayerStateBase* SpeakerState, FARDialogueNodeRow& OutRow)
{
	OutRow = FARDialogueNodeRow();
	if (!Subsystem || !SpeakerState || !NpcTag.IsValid())
	{
		return false;
	}

	TArray<FARDialogueNodeRow> Rows;
	if (!BuildCandidateRowsForNpc(Subsystem, NpcTag, Rows))
	{
		return false;
	}

	FARNpcRelationshipState NpcState;
	const bool bHasNpcState = ResolveNpcState(Subsystem, NpcTag, NpcState);

	const UARSaveGame* SaveGame = GetMutableCurrentSave(Subsystem);
	for (const FARDialogueNodeRow& Row : Rows)
	{
		if (!EvaluateRowUnlocked(Subsystem, Row, SpeakerState, bHasNpcState ? &NpcState : nullptr))
		{
			continue;
		}

		// If a canonical choice exists, this row remains valid and will follow the stored branch on advance.
		if (SaveGame && Row.Choices.Num() > 0)
		{
			for (const FARDialogueCanonicalChoiceState& Canonical : SaveGame->DialogueCanonicalChoiceStates)
			{
				if (Canonical.NodeTag.MatchesTagExact(Row.NodeTag))
				{
					OutRow = Row;
					return true;
				}
			}
		}

		OutRow = Row;
		return true;
	}

	return false;
}

bool UARDialogueSubsystem::TryStartDialogueWithNpc(AARPlayerController* RequestingController, FGameplayTag NpcTag)
{
	UWorld* World = GetWorld();
	if (!IsAuthorityWorld(World) || !RequestingController || !NpcTag.IsValid())
	{
		return false;
	}

	AARPlayerStateBase* RequesterPS = GetARPlayerStateFromController(RequestingController);
	if (!RequesterPS)
	{
		return false;
	}

	const EARPlayerSlot RequesterSlot = RequesterPS->GetPlayerSlot();
	if (RequesterSlot == EARPlayerSlot::Unknown)
	{
		return false;
	}

	const UARDialogueSettings* Settings = GetDefault<UARDialogueSettings>();
	const FGameplayTag ModeTag = GetCurrentModeTag(World);
	const bool bSharedMode = Settings && IsModeInContainer(ModeTag, Settings->SharedDialogueModeTags);

	if (bSharedMode)
	{
		if (FARActiveDialogueSession* ExistingShared = FindSharedSession())
		{
			if (ExistingShared->NpcTag.MatchesTagExact(NpcTag))
			{
				ExistingShared->Participants.Add(RequesterSlot);
				DispatchSessionUpdate(this, *ExistingShared);
				return true;
			}
			return false;
		}
	}
	else if (FindSessionByOwnerSlot(RequesterSlot))
	{
		return false;
	}

	FARDialogueNodeRow SelectedRow;
	if (!TrySelectBestRowForSpeaker(this, NpcTag, RequesterPS, SelectedRow))
	{
		return false;
	}

	FARActiveDialogueSession Session;
	Session.SessionId = BuildSessionId();
	Session.NpcTag = NpcTag;
	Session.InitiatorSlot = RequesterSlot;
	Session.OwnerSlot = RequesterSlot;
	Session.bIsSharedSession = bSharedMode;
	Session.CurrentNodeTag = SelectedRow.NodeTag;
	Session.ActiveRow = SelectedRow;
	Session.bWaitingForChoice = SelectedRow.Choices.Num() > 0 && FindCanonicalChoice(GetMutableCurrentSave(this), SelectedRow.NodeTag) == nullptr;
	Session.ChoiceParticipation = SelectedRow.ChoiceParticipation;
	Session.bForceEavesdropForImportantDecision = SelectedRow.bForceEavesdropForImportantDecision;
	Session.CurrentChoices = SelectedRow.Choices;
	Session.Participants.Add(RequesterSlot);

	if (bSharedMode)
	{
		if (const AARGameStateBase* GS = World->GetGameState<AARGameStateBase>())
		{
			for (APlayerState* PS : GS->PlayerArray)
			{
				if (const AARPlayerStateBase* ARPS = Cast<AARPlayerStateBase>(PS))
				{
					if (ARPS->GetPlayerSlot() != EARPlayerSlot::Unknown)
					{
						Session.Participants.Add(ARPS->GetPlayerSlot());
					}
				}
			}
		}

		if (Settings && IsModeInContainer(ModeTag, Settings->PauseOnDialogueModeTags))
		{
			UGameplayStatics::SetGamePaused(World, true);
		}
	}
	else
	{
		if (const EARPlayerSlot* ExistingTarget = GDialogueRuntimeState.ShopEavesdropTargetByViewer.Find(RequesterSlot))
		{
			Session.Participants.Add(*ExistingTarget);
		}

		// Important decisions force partner visibility in Shop before any vote can resolve.
		if (Session.bWaitingForChoice && Session.ChoiceParticipation == EARDialogueChoiceParticipation::GroupChoice && Session.bForceEavesdropForImportantDecision)
		{
			const EARPlayerSlot PartnerSlot = RequesterSlot == EARPlayerSlot::P1 ? EARPlayerSlot::P2 : EARPlayerSlot::P1;
			Session.Participants.Add(PartnerSlot);
			GDialogueRuntimeState.ShopEavesdropTargetByViewer.Add(PartnerSlot, RequesterSlot);
		}
	}

	MarkNodeSeenForSpeaker(this, RequesterPS, SelectedRow.NodeTag);
	ApplyProgressionGrants(this, SelectedRow.GrantProgressionTagsOnEnter);

	GDialogueRuntimeState.Sessions.Add(MoveTemp(Session));
	DispatchSessionUpdate(this, GDialogueRuntimeState.Sessions.Last());
	NotifyNpcSubsystemTalkableRefresh(this, NpcTag);
	return true;
}

bool UARDialogueSubsystem::AdvanceDialogue(AARPlayerController* RequestingController)
{
	UWorld* World = GetWorld();
	if (!IsAuthorityWorld(World) || !RequestingController)
	{
		return false;
	}

	AARPlayerStateBase* RequesterPS = GetARPlayerStateFromController(RequestingController);
	if (!RequesterPS)
	{
		return false;
	}

	const EARPlayerSlot Slot = RequesterPS->GetPlayerSlot();
	FARActiveDialogueSession* Session = FindSessionForSlot(Slot);
	if (!Session)
	{
		return false;
	}

	if (Session->bWaitingForChoice)
	{
		return false;
	}

	FGameplayTag NextNode = Session->ActiveRow.NextNodeTag;

	// If this is a choice node with canonical state already resolved, follow the canonical branch.
	if (Session->CurrentChoices.Num() > 0)
	{
		if (UARSaveGame* SaveGame = GetMutableCurrentSave(this))
		{
			if (FARDialogueCanonicalChoiceState* Canonical = FindCanonicalChoice(SaveGame, Session->CurrentNodeTag))
			{
				for (const FARDialogueChoiceDef& Choice : Session->CurrentChoices)
				{
					if (Choice.ChoiceTag.MatchesTagExact(Canonical->ChoiceTag))
					{
						NextNode = Choice.NextNodeTag;
						break;
					}
				}
			}
		}
	}

	if (!NextNode.IsValid())
	{
		FARActiveDialogueSession EndCopy = *Session;
		EndSession(this, EndCopy);
		GDialogueRuntimeState.Sessions.RemoveAll([&EndCopy](const FARActiveDialogueSession& Item)
		{
			return Item.SessionId == EndCopy.SessionId;
		});
		return true;
	}

	UContentLookupSubsystem* Lookup = nullptr;
	if (!GetLookupSubsystem(this, Lookup))
	{
		return false;
	}

	FARDialogueNodeRow NextRow;
	if (!ResolveDialogueRow(Lookup, NextNode, NextRow))
	{
		return false;
	}

	Session->CurrentNodeTag = NextRow.NodeTag;
	Session->ActiveRow = NextRow;
	Session->ChoiceParticipation = NextRow.ChoiceParticipation;
	Session->bForceEavesdropForImportantDecision = NextRow.bForceEavesdropForImportantDecision;
	Session->CurrentChoices = NextRow.Choices;
	Session->ChoiceVotes.Reset();

	if (!Session->bIsSharedSession && Session->bForceEavesdropForImportantDecision && Session->ChoiceParticipation == EARDialogueChoiceParticipation::GroupChoice && Session->CurrentChoices.Num() > 0)
	{
		const EARPlayerSlot PartnerSlot = Session->OwnerSlot == EARPlayerSlot::P1 ? EARPlayerSlot::P2 : EARPlayerSlot::P1;
		Session->Participants.Add(PartnerSlot);
		GDialogueRuntimeState.ShopEavesdropTargetByViewer.Add(PartnerSlot, Session->OwnerSlot);
	}

	Session->bWaitingForChoice = NextRow.Choices.Num() > 0 && FindCanonicalChoice(GetMutableCurrentSave(this), NextRow.NodeTag) == nullptr;

	MarkNodeSeenForSpeaker(this, RequesterPS, NextRow.NodeTag);
	ApplyProgressionGrants(this, NextRow.GrantProgressionTagsOnEnter);
	DispatchSessionUpdate(this, *Session);
	NotifyNpcSubsystemTalkableRefresh(this, Session->NpcTag);
	return true;
}

bool UARDialogueSubsystem::SubmitDialogueChoice(AARPlayerController* RequestingController, FGameplayTag ChoiceTag)
{
	UWorld* World = GetWorld();
	if (!IsAuthorityWorld(World) || !RequestingController || !ChoiceTag.IsValid())
	{
		return false;
	}

	AARPlayerStateBase* RequesterPS = GetARPlayerStateFromController(RequestingController);
	if (!RequesterPS)
	{
		return false;
	}

	const EARPlayerSlot Slot = RequesterPS->GetPlayerSlot();
	FARActiveDialogueSession* Session = FindSessionForSlot(Slot);
	if (!Session || !Session->bWaitingForChoice)
	{
		return false;
	}

	bool bChoiceExists = false;
	for (const FARDialogueChoiceDef& Choice : Session->CurrentChoices)
	{
		if (Choice.ChoiceTag.MatchesTagExact(ChoiceTag))
		{
			bChoiceExists = true;
			break;
		}
	}

	if (!bChoiceExists)
	{
		return false;
	}

	const bool bIsInitiator = Slot == Session->InitiatorSlot;
	if (Session->ChoiceParticipation == EARDialogueChoiceParticipation::InitiatorOnly && !bIsInitiator)
	{
		return false;
	}

	if (Session->bForceEavesdropForImportantDecision && Session->ChoiceParticipation == EARDialogueChoiceParticipation::GroupChoice)
	{
		if (!AreAllSlottedPlayersViewingSession(this, *Session))
		{
			UE_LOG(ARLog, Verbose, TEXT("[Dialogue] Important choice locked until all players are viewing."));
			return false;
		}
	}

	Session->ChoiceVotes.Add(Slot, ChoiceTag);

	FGameplayTag ResolvedChoiceTag;
	if (Session->ChoiceParticipation == EARDialogueChoiceParticipation::InitiatorOnly)
	{
		ResolvedChoiceTag = ChoiceTag;
	}
	else
	{
		// Group choice in v1: choose initiator's vote as canonical tie-break.
		if (const FGameplayTag* InitiatorVote = Session->ChoiceVotes.Find(Session->InitiatorSlot))
		{
			ResolvedChoiceTag = *InitiatorVote;
		}
		else
		{
			// Wait until initiator votes.
			DispatchSessionUpdate(this, *Session);
			return true;
		}
	}

	UARSaveSubsystem* SaveSubsystem = nullptr;
	if (!GetSaveSubsystem(this, SaveSubsystem))
	{
		return false;
	}

	UARSaveGame* SaveGame = SaveSubsystem->GetCurrentSaveGame();
	if (!SaveGame)
	{
		return false;
	}

	FARDialogueCanonicalChoiceState* Canonical = FindOrAddCanonicalChoice(SaveGame, Session->CurrentNodeTag);
	if (!Canonical)
	{
		return false;
	}
	Canonical->ChoiceTag = ResolvedChoiceTag;
	SaveSubsystem->MarkSaveDirty();

	for (const FARDialogueChoiceDef& Choice : Session->CurrentChoices)
	{
		if (Choice.ChoiceTag.MatchesTagExact(ResolvedChoiceTag))
		{
			ApplyProgressionGrants(this, Choice.GrantProgressionTags);
			break;
		}
	}

	Session->bWaitingForChoice = false;
	Session->ChoiceVotes.Reset();
	DispatchSessionUpdate(this, *Session);
	NotifyNpcSubsystemTalkableRefresh(this, Session->NpcTag);
	return true;
}

bool UARDialogueSubsystem::SetShopEavesdropTarget(AARPlayerController* RequestingController, EARPlayerSlot TargetSlot, bool bEnable)
{
	UWorld* World = GetWorld();
	if (!IsAuthorityWorld(World) || !RequestingController)
	{
		return false;
	}

	const UARDialogueSettings* Settings = GetDefault<UARDialogueSettings>();
	const FGameplayTag ModeTag = GetCurrentModeTag(World);
	if (!Settings || !IsModeInContainer(ModeTag, Settings->PerPlayerDialogueModeTags))
	{
		return false;
	}

	AARPlayerStateBase* RequesterPS = GetARPlayerStateFromController(RequestingController);
	if (!RequesterPS)
	{
		return false;
	}

	const EARPlayerSlot ViewerSlot = RequesterPS->GetPlayerSlot();
	if (ViewerSlot == EARPlayerSlot::Unknown)
	{
		return false;
	}

	if (bEnable)
	{
		if (TargetSlot == EARPlayerSlot::Unknown || TargetSlot == ViewerSlot)
		{
			return false;
		}

		GDialogueRuntimeState.ShopEavesdropTargetByViewer.Add(ViewerSlot, TargetSlot);
		if (FARActiveDialogueSession* TargetSession = FindSessionByOwnerSlot(TargetSlot))
		{
			TargetSession->Participants.Add(ViewerSlot);
			DispatchSessionUpdate(this, *TargetSession);
		}
		return true;
	}

	GDialogueRuntimeState.ShopEavesdropTargetByViewer.Remove(ViewerSlot);
	for (FARActiveDialogueSession& Session : GDialogueRuntimeState.Sessions)
	{
		if (!Session.bIsSharedSession && Session.OwnerSlot != ViewerSlot)
		{
			Session.Participants.Remove(ViewerSlot);
			DispatchSessionUpdate(this, Session);
		}
	}
	return true;
}

bool UARDialogueSubsystem::HasUnlockedDialogueForNpcForSlot(FGameplayTag NpcTag, EARPlayerSlot PlayerSlot) const
{
	if (!NpcTag.IsValid() || PlayerSlot == EARPlayerSlot::Unknown)
	{
		return false;
	}

	AARPlayerController* PC = FindPlayerControllerBySlot(GetWorld(), PlayerSlot);
	const AARPlayerStateBase* PS = PC ? PC->GetPlayerState<AARPlayerStateBase>() : nullptr;
	if (!PS)
	{
		return false;
	}

	FARDialogueNodeRow Row;
	return TrySelectBestRowForSpeaker(this, NpcTag, PS, Row);
}

bool UARDialogueSubsystem::HasUnlockedDialogueForNpcForAnyPlayer(FGameplayTag NpcTag) const
{
	if (!NpcTag.IsValid())
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

		FARDialogueNodeRow Row;
		if (TrySelectBestRowForSpeaker(this, NpcTag, ARPS, Row))
		{
			return true;
		}
	}

	return false;
}

bool UARDialogueSubsystem::GetLocalViewForController(const AARPlayerController* RequestingController, FARDialogueClientView& OutView) const
{
	OutView = FARDialogueClientView();
	if (!RequestingController)
	{
		return false;
	}

	const EARPlayerSlot Slot = GetSlotFromController(RequestingController);
	if (Slot == EARPlayerSlot::Unknown)
	{
		return false;
	}

	const FARActiveDialogueSession* Session = FindSessionForSlot(Slot);
	if (!Session)
	{
		return false;
	}

	OutView = BuildViewForSlot(*Session, Slot);
	return true;
}
