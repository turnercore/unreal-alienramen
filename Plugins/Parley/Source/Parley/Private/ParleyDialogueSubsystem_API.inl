// Split from ParleyDialogueSubsystem.cpp for maintainability.

bool UParleyDialogueSubsystem::PreviewConversationTrace(
	UParleyConversationAsset* ConversationAsset,
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
	PreviewSession.bConversationPrivate = ConversationAsset->Header.bPrivateConversation;
	PreviewSession.OwnerSlot = EParleyPlayerSlot::P1;
	PreviewSession.InitiatorSlot = EParleyPlayerSlot::P1;
	PreviewSession.Participants.Add(EParleyPlayerSlot::P1);
	PreviewSession.TransientConversationTags = PreviewContext.TransientConversationTags;

	TMap<EParleyPlayerSlot, FGameplayTagContainer> PreviewSeenByPlayer;
	FGameplayTagContainer PreviewSeenByGame;
	if (PreviewContext.bSeenByPlayer && PreviewSession.ConversationTag.IsValid())
	{
		PreviewSeenByPlayer.FindOrAdd(EParleyPlayerSlot::P1).AddTag(PreviewSession.ConversationTag);
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
			const_cast<UParleyDialogueSubsystem*>(this),
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
			FillClientViewForSlot(PreviewSession, EParleyPlayerSlot::P1, View);
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

bool UParleyDialogueSubsystem::PreviewConversation(UParleyConversationAsset* ConversationAsset, const FDialogueRuntimeContext& PreviewContext, FDialogueClientView& OutFirstView, FDialogueValidationReport& OutReport) const
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

bool UParleyDialogueSubsystem::HasUnlockedDialogueForSpeakerForSlot(FGameplayTag PrimarySpeakerTag, FGameplayTag PlayerSlotTag) const
{
	const EParleyPlayerSlot PlayerSlot = ParleyPlayerSlot::TagToSlot(PlayerSlotTag);
	if (!PrimarySpeakerTag.IsValid() || PlayerSlot == EParleyPlayerSlot::Unknown)
	{
		return false;
	}

	APlayerController* PC = FindPlayerControllerBySlot(GetWorld(), PlayerSlot);
	if (!PC)
	{
		return false;
	}

	FDialogueConversationOffer Offer;
	return const_cast<UParleyDialogueSubsystem*>(this)->GetAvailableConversationForSpeaker(PC, PrimarySpeakerTag, Offer, /*bSpeakerLocalStateAllowsDialogue=*/ true);
}

void UParleyDialogueSubsystem::GetRegisteredPrimarySpeakerTags(TArray<FGameplayTag>& OutSpeakerTags) const
{
	OutSpeakerTags.Reset();

	const FARDialogueRuntimeState& Runtime = GetRuntimeState();
	TSet<FGameplayTag> UniqueTags;
	UniqueTags.Reserve(Runtime.SpeakerRowsByTag.Num() + Runtime.ConversationsByTag.Num());

	for (const TPair<FGameplayTag, FARDialogueSpeakerRow>& Pair : Runtime.SpeakerRowsByTag)
	{
		if (Pair.Key.IsValid())
		{
			UniqueTags.Add(Pair.Key);
		}
	}

	for (const TPair<FGameplayTag, TObjectPtr<UParleyConversationAsset>>& Pair : Runtime.ConversationsByTag)
	{
		const UParleyConversationAsset* Conversation = Pair.Value;
		if (Conversation && Conversation->Header.PrimarySpeakerTag.IsValid())
		{
			UniqueTags.Add(Conversation->Header.PrimarySpeakerTag);
		}
	}

	OutSpeakerTags.Reserve(UniqueTags.Num());
	for (const FGameplayTag& Tag : UniqueTags)
	{
		OutSpeakerTags.Add(Tag);
	}
}

bool UParleyDialogueSubsystem::HasUnlockedDialogueForSpeakerForAnyPlayer(FGameplayTag PrimarySpeakerTag) const
{
	if (!PrimarySpeakerTag.IsValid())
	{
		return false;
	}
	const UWorld* World = GetWorld();
	const AGameStateBase* GS = World ? World->GetGameState<AGameStateBase>() : nullptr;
	if (!GS)
	{
		return false;
	}

	for (APlayerState* PS : GS->PlayerArray)
	{
		if (!PS)
		{
			continue;
		}

		const EParleyPlayerSlot Slot = GetPlayerSlotFromPlayerState(PS);
		if (Slot == EParleyPlayerSlot::Unknown)
		{
			continue;
		}
		if (HasUnlockedDialogueForSpeakerForSlot(PrimarySpeakerTag, ParleyPlayerSlot::SlotToTag(Slot)))
		{
			return true;
		}
	}

	return false;
}

bool UParleyDialogueSubsystem::IsPrimarySpeakerInActiveSession(FGameplayTag PrimarySpeakerTag) const
{
	if (!PrimarySpeakerTag.IsValid())
	{
		return false;
	}

	return FindPerPlayerSessionByPrimarySpeaker(GetRuntimeState().ActiveSessions, PrimarySpeakerTag, EParleyPlayerSlot::Unknown) != nullptr;
}

bool UParleyDialogueSubsystem::GetPrimarySpeakerForConversation(FGameplayTag ConversationTag, FGameplayTag& OutPrimarySpeakerTag) const
{
	OutPrimarySpeakerTag = FGameplayTag();
	if (!ConversationTag.IsValid())
	{
		return false;
	}

	const FARDialogueRuntimeState& Runtime = GetRuntimeState();
	const TObjectPtr<UParleyConversationAsset>* ConversationPtr = Runtime.ConversationsByTag.Find(ConversationTag);
	const UParleyConversationAsset* Conversation = ConversationPtr ? ConversationPtr->Get() : nullptr;
	if (!Conversation || !Conversation->Header.PrimarySpeakerTag.IsValid())
	{
		return false;
	}

	OutPrimarySpeakerTag = Conversation->Header.PrimarySpeakerTag;
	return true;
}

bool UParleyDialogueSubsystem::IsSpeakerBusyForController(const APlayerController* RequestingController, FGameplayTag PrimarySpeakerTag) const
{
	if (!RequestingController || !PrimarySpeakerTag.IsValid())
	{
		return false;
	}

	const EParleyPlayerSlot RequesterSlot = GetSlotFromController(RequestingController);
	if (RequesterSlot == EParleyPlayerSlot::Unknown)
	{
		return false;
	}

	const UParleyDialogueSettings* Settings = GetDefault<UParleyDialogueSettings>();
	const FGameplayTag ModeTag = GetCurrentModeTag(this, GetWorld());
	if (!IsBusySpeakerLockEnabled(Settings, ModeTag))
	{
		return false;
	}

	return FindPerPlayerSessionByPrimarySpeaker(GetRuntimeState().ActiveSessions, PrimarySpeakerTag, RequesterSlot) != nullptr;
}

bool UParleyDialogueSubsystem::GetLocalViewForController(const APlayerController* RequestingController, FDialogueClientView& OutView) const
{
	OutView = FDialogueClientView();
	if (!RequestingController)
	{
		return false;
	}
	const EParleyPlayerSlot Slot = GetSlotFromController(RequestingController);
	if (Slot == EParleyPlayerSlot::Unknown)
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
	OutView.SpeakerLineFontStyleTag = Session->CurrentSpeakerLineFontStyleTag;
	OutView.SpeakerLineFont = Session->CurrentSpeakerLineFont;
	OutView.LineText = Session->CurrentLineText;
	OutView.SpeakerPortrait = Session->CurrentSpeakerPortrait;
	OutView.bWaitingForChoice = Session->bWaitingForChoice;
	OutView.bConversationImportant = Session->bConversationImportant;
	OutView.bIsEavesdropping = !Session->bIsSharedSession && Slot != Session->OwnerSlot;
	OutView.InitiatorSlotTag = ParleyPlayerSlot::SlotToTag(Session->InitiatorSlot);
	OutView.OwnerSlotTag = ParleyPlayerSlot::SlotToTag(Session->OwnerSlot);
	OutView.Choices = Session->CurrentChoices;
	return true;
}

bool UParleyDialogueSubsystem::HasActiveDialogueSession() const
{
	return GetRuntimeState().ActiveSessions.Num() > 0;
}

void UParleyDialogueSubsystem::ClearConversationCycleOfferState(const FGameplayTag PlayerSlotTag)
{
	const EParleyPlayerSlot PlayerSlot = ParleyPlayerSlot::TagToSlot(PlayerSlotTag);
	FARDialogueRuntimeState& Runtime = GetRuntimeState();
	FParleyProgressionStore* ProgressionStore = GetProgressionStore(this);
	FParleyProgressionMutator* ProgressionMutator = GetProgressionMutator(this);
	bool bProgressionChanged = false;

	if (PlayerSlot == EParleyPlayerSlot::Unknown)
	{
		Runtime.SeenByPlayerTransient.Reset();
		Runtime.SkippedByPlayerTransient.Reset();
		Runtime.SpeakerOfferCountsByPlayerTransient.Reset();

		if (ProgressionStore)
		{
			for (FParleyCharacterProgressionData& CharacterState : ProgressionStore->CharacterStates)
			{
				FDialoguePlayerPersistentState& PlayerState = CharacterState.DialogueState;
				if (!PlayerState.SeenConversationTagsThisCycle.IsEmpty() || !PlayerState.SkippedConversationTagsThisCycle.IsEmpty())
				{
					PlayerState.SeenConversationTagsThisCycle.Reset();
					PlayerState.SkippedConversationTagsThisCycle.Reset();
					bProgressionChanged = true;
				}
				if (!PlayerState.SpeakerOfferCountsThisCycle.IsEmpty())
				{
					PlayerState.SpeakerOfferCountsThisCycle.Reset();
					bProgressionChanged = true;
				}
			}
		}

		if (bProgressionChanged && ProgressionMutator)
		{
			ProgressionMutator->MarkStateDirty();
		}

		UE_LOG(ParleyLog, Log, TEXT("[Dialogue] Cleared conversation cycle offer state for all player slots."));
		return;
	}

	Runtime.SeenByPlayerTransient.Remove(PlayerSlot);
	Runtime.SkippedByPlayerTransient.Remove(PlayerSlot);
	Runtime.SpeakerOfferCountsByPlayerTransient.Remove(PlayerSlot);

	if (ProgressionStore)
	{
		const APlayerState* PlayerState = FindPlayerStateBySlot(GetWorld(), PlayerSlot);
		const FGameplayTag CharacterTag = PlayerState
			? ResolveDialogueCharacterTagFromPlayerState(PlayerState)
			: GetDefaultCharacterTagForSlot(PlayerSlot);
		int32 CharacterIndex = INDEX_NONE;
		if (FParleyCharacterProgressionData* CharacterState = ProgressionStore->FindCharacterStateDataMutable(CharacterTag, CharacterIndex))
		{
			FDialoguePlayerPersistentState& CharacterDialogueState = CharacterState->DialogueState;
			if (!CharacterDialogueState.SeenConversationTagsThisCycle.IsEmpty() || !CharacterDialogueState.SkippedConversationTagsThisCycle.IsEmpty())
			{
				CharacterDialogueState.SeenConversationTagsThisCycle.Reset();
				CharacterDialogueState.SkippedConversationTagsThisCycle.Reset();
				bProgressionChanged = true;
			}
			if (!CharacterDialogueState.SpeakerOfferCountsThisCycle.IsEmpty())
			{
				CharacterDialogueState.SpeakerOfferCountsThisCycle.Reset();
				bProgressionChanged = true;
			}
		}
	}

	if (bProgressionChanged && ProgressionMutator)
	{
		ProgressionMutator->MarkStateDirty();
	}

	UE_LOG(ParleyLog, Log, TEXT("[Dialogue] Cleared conversation cycle offer state for slot %s."),
		*StaticEnum<EParleyPlayerSlot>()->GetNameStringByValue(static_cast<int64>(PlayerSlot)));
}

float UParleyDialogueSubsystem::GetRelationshipPointsForSpeaker(FGameplayTag SpeakerTag) const
{
	const FParleyProgressionStore* ProgressionStore = GetProgressionStore(this);
	if (!ProgressionStore || !SpeakerTag.IsValid())
	{
		return 0.0f;
	}
	for (const FDialogueRelationshipState& State : ProgressionStore->DialogueRelationshipStates)
	{
		if (State.SpeakerTag.MatchesTagExact(SpeakerTag))
		{
			return State.RelationshipPoints;
		}
	}
	return 0.0f;
}

int32 UParleyDialogueSubsystem::GetRelationshipLevelForSpeaker(FGameplayTag SpeakerTag) const
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

void UParleyDialogueSubsystem::SetProgressionStateForPlayer(FGameplayTag PlayerSlotTag, const FParleyProgressionState& State)
{
	const EParleyPlayerSlot PlayerSlot = ParleyPlayerSlot::TagToSlot(PlayerSlotTag);
	if (PlayerSlot == EParleyPlayerSlot::Unknown)
	{
		UE_LOG(ParleyLog, Verbose, TEXT("[Dialogue] SetProgressionStateForPlayer ignored: unresolved slot tag '%s'."), *PlayerSlotTag.ToString());
		return;
	}

	FParleyProgressionStore* ProgressionStore = GetProgressionStore(this);
	if (!ProgressionStore)
	{
		return;
	}

	const FGameplayTag CharacterTag = State.CharacterTag.IsValid()
		? State.CharacterTag
		: GetDefaultCharacterTagForSlot(PlayerSlot);
	if (!CharacterTag.IsValid())
	{
		UE_LOG(ParleyLog, Verbose, TEXT("[Dialogue] SetProgressionStateForPlayer ignored: unresolved character tag for slot %s."),
			*StaticEnum<EParleyPlayerSlot>()->GetNameStringByValue(static_cast<int64>(PlayerSlot)));
		return;
	}

	ProgressionStore->CharacterTagBySlot.FindOrAdd(PlayerSlot) = CharacterTag;
	FParleyCharacterProgressionData& CharacterState = ProgressionStore->FindOrAddCharacterStateData(CharacterTag);
	FDialoguePlayerPersistentState& DialogueState = CharacterState.DialogueState;
	DialogueState.OwnerPlayerSlotTag = ParleyPlayerSlot::SlotToTag(PlayerSlot);
	DialogueState.CharacterTag = CharacterTag;
	DialogueState.ProgressionTags = State.ProgressionTags;
	DialogueState.CompletedConversationTags = State.CompletedConversationTags;
	DialogueState.CompletedChoiceRecords = State.CompletedChoiceRecords;
	DialogueState.SeenConversationTagsThisCycle = State.SeenConversationTagsThisCycle;
	DialogueState.SkippedConversationTagsThisCycle = State.SkippedConversationTagsThisCycle;
	DialogueState.SpeakerOfferCountsThisCycle = State.SpeakerOfferCountsThisCycle;

	FARDialogueRuntimeState& Runtime = GetRuntimeState();
	Runtime.SeenByPlayerTransient.FindOrAdd(PlayerSlot) = State.SeenConversationTagsThisCycle;
	Runtime.SkippedByPlayerTransient.FindOrAdd(PlayerSlot) = State.SkippedConversationTagsThisCycle;
	TMap<FGameplayTag, int32> OfferCountMap;
	BuildSpeakerOfferCountMap(State.SpeakerOfferCountsThisCycle, OfferCountMap);
	Runtime.SpeakerOfferCountsByPlayerTransient.FindOrAdd(PlayerSlot) = MoveTemp(OfferCountMap);
}

void UParleyDialogueSubsystem::SetGameProgressionTags(const FGameplayTagContainer& Tags)
{
	if (FParleyProgressionStore* ProgressionStore = GetProgressionStore(this))
	{
		ProgressionStore->ProgressionTags = Tags;
	}
}

void UParleyDialogueSubsystem::SetCompletedConversationTagsByGame(const FGameplayTagContainer& Tags)
{
	if (FParleyProgressionStore* ProgressionStore = GetProgressionStore(this))
	{
		ProgressionStore->DialogueCompletedConversationTagsByGame = Tags;
	}
}

void UParleyDialogueSubsystem::GetCompletedConversationTagsByGame(FGameplayTagContainer& OutTags) const
{
	OutTags.Reset();
	if (const FParleyProgressionStore* ProgressionStore = GetProgressionStore(this))
	{
		OutTags = ProgressionStore->DialogueCompletedConversationTagsByGame;
	}
}

void UParleyDialogueSubsystem::SetRelationshipStates(const TArray<FDialogueRelationshipState>& States)
{
	if (FParleyProgressionStore* ProgressionStore = GetProgressionStore(this))
	{
		ProgressionStore->DialogueRelationshipStates.Reset();
		for (const FDialogueRelationshipState& State : States)
		{
			if (!State.SpeakerTag.IsValid())
			{
				continue;
			}

			FDialogueRelationshipState& Added = ProgressionStore->DialogueRelationshipStates.AddDefaulted_GetRef();
			Added.SpeakerTag = State.SpeakerTag;
			Added.RelationshipPoints = State.RelationshipPoints;
		}
	}
}
