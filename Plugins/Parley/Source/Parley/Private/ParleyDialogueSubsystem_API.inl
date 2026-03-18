// Split from ParleyDialogueSubsystem.cpp for maintainability.

static FGameplayTag ResolvePreviewOwnerCharacterTag(const FDialogueRuntimeContext& PreviewContext)
{
	if (PreviewContext.ResolvedPlayerSpeakerTag.IsValid())
	{
		return PreviewContext.ResolvedPlayerSpeakerTag;
	}

	if (PreviewContext.SourceSpeakerTag.IsValid())
	{
		return PreviewContext.SourceSpeakerTag;
	}

	if (PreviewContext.ActivePawn)
	{
		if (const UParleySpeakerComponent* SpeakerComponent = PreviewContext.ActivePawn->FindComponentByClass<UParleySpeakerComponent>())
		{
			return SpeakerComponent->GetSpeakerTag();
		}
	}

	return FGameplayTag();
}

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

	FParleyActiveDialogueSession PreviewSession;
	PreviewSession.SessionId = TEXT("PreviewTrace");
	PreviewSession.ConversationTag = ConversationAsset->Header.ConversationTag;
	PreviewSession.PrimarySpeakerTag = ConversationAsset->Header.PrimarySpeakerTag;
	PreviewSession.ConversationAsset = ConversationAsset;
	PreviewSession.CurrentNodeId = ConversationAsset->CompiledData.EnterNodeId;
	PreviewSession.bConversationImportant = ConversationAsset->Header.bImportant;
	PreviewSession.bConversationPrivate = ConversationAsset->Header.bPrivateConversation;
	const FGameplayTag PreviewCharacterTag = ResolvePreviewOwnerCharacterTag(PreviewContext);
	PreviewSession.OwnerCharacterTag = PreviewCharacterTag;
	PreviewSession.InitiatorCharacterTag = PreviewCharacterTag;
	if (PreviewCharacterTag.IsValid())
	{
		PreviewSession.Participants.Add(PreviewCharacterTag);
	}
	PreviewSession.TransientConversationTags = PreviewContext.TransientConversationTags;

	TMap<FGameplayTag, FGameplayTagContainer> PreviewSeenByPlayer;
	FGameplayTagContainer PreviewSeenByGame;
	if (PreviewContext.bSeenByPlayer && PreviewSession.ConversationTag.IsValid())
	{
		PreviewSeenByPlayer.FindOrAdd(PreviewCharacterTag).AddTag(PreviewSession.ConversationTag);
	}
	if (PreviewContext.bSeenByGame && PreviewSession.ConversationTag.IsValid())
	{
		PreviewSeenByGame.AddTag(PreviewSession.ConversationTag);
	}

	const FParleyDialogueRuntimeState& Runtime = GetRuntimeState();
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
			FillClientViewForCharacter(PreviewSession, PreviewCharacterTag, View);
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

bool UParleyDialogueSubsystem::HasUnlockedDialogueForSpeakerForCharacter(FGameplayTag PrimarySpeakerTag, FGameplayTag OwnerCharacterTag) const
{
	const UWorld* World = GetWorld();
	if (!IsAuthorityWorld_Dialogue(World))
	{
		UE_LOG(ParleyLog, Verbose, TEXT("[Dialogue] HasUnlockedDialogueForSpeakerForCharacter requires authority runtime."));
		return false;
	}

	const FGameplayTag PlayerCharacterTag = NormalizeCharacterTagForDialogue(OwnerCharacterTag);
	if (!PrimarySpeakerTag.IsValid() || !PlayerCharacterTag.IsValid())
	{
		return false;
	}

	APlayerController* PC = FindPlayerControllerByCharacter(World, PlayerCharacterTag);
	if (!PC)
	{
		return false;
	}

	APlayerState* RequesterPS = PC->GetPlayerState<APlayerState>();
	if (!RequesterPS)
	{
		return false;
	}

	const FParleyPlayerIdentity PlayerIdentity = BuildPlayerIdentityFromState(RequesterPS);
	const FParleyProgressionStore* ProgressionStore = GetProgressionStore(this);
	FGameplayTagContainer SeenThisCycle;
	FGameplayTagContainer SkippedThisCycle;
	TMap<FGameplayTag, int32> SpeakerOfferCountMap;
	if (const FDialoguePlayerPersistentState* PlayerState = FindPlayerDialogueState(ProgressionStore, PlayerIdentity, World))
	{
		SeenThisCycle = PlayerState->SeenConversationTagsThisCycle;
		SkippedThisCycle = PlayerState->SkippedConversationTagsThisCycle;
		BuildSpeakerOfferCountMap(PlayerState->SpeakerOfferCountsThisCycle, SpeakerOfferCountMap);
	}

	const FParleyDialogueRuntimeState& Runtime = GetRuntimeState();
	const FParleySpeakerRow* SpeakerRow = Runtime.SpeakerRowsByTag.Find(PrimarySpeakerTag);
	const int32 MaxOffersPerCycle = SpeakerRow ? FMath::Max(0, SpeakerRow->MaxOffersPerCycle) : 0;
	const int32 ExistingOfferCount = SpeakerOfferCountMap.FindRef(PrimarySpeakerTag);
	if (MaxOffersPerCycle > 0 && ExistingOfferCount >= MaxOffersPerCycle)
	{
		return false;
	}

	const UParleyDialogueSettings* Settings = GetDefault<UParleyDialogueSettings>();
	const FGameplayTag ModeTag = GetCurrentModeTag(this, World);
	if (!IsModeDialogueEnabled(Settings, ModeTag))
	{
		return false;
	}

	if (IsBusySpeakerLockEnabled(Settings, ModeTag)
		&& FindPerPlayerSessionByPrimarySpeaker(Runtime.ActiveSessions, PrimarySpeakerTag, PlayerCharacterTag) != nullptr)
	{
		return false;
	}

	for (const TPair<FGameplayTag, TObjectPtr<UParleyConversationAsset>>& Pair : Runtime.ConversationsByTag)
	{
		UParleyConversationAsset* Conversation = Pair.Value;
		if (!Conversation || !Conversation->Header.PrimarySpeakerTag.MatchesTagExact(PrimarySpeakerTag))
		{
			continue;
		}

		FDialogueValidationReport Validation;
		if (!ValidateConversation(Conversation, Validation))
		{
			continue;
		}

		FDialogueRuntimeContext Context = BuildOfferContext(this, Conversation, RequesterPS, PlayerIdentity);
		Context.bSeenByGame = Runtime.SeenByGameTransient.HasTagExact(Conversation->Header.ConversationTag);
		Context.bSeenByPlayer = SeenThisCycle.HasTagExact(Conversation->Header.ConversationTag);
		const bool bSkippedForCycle = SkippedThisCycle.HasTagExact(Conversation->Header.ConversationTag);

		if (!EvaluateConversationOfferRules(this, Context, Conversation->Header, nullptr))
		{
			continue;
		}

		if (!Conversation->Header.bRepeatable && Context.bCompletedByPlayer)
		{
			continue;
		}
		if (Conversation->Header.bCompletedByGameBlocksReoffer && Context.bCompletedByGame)
		{
			continue;
		}
		if (Conversation->Header.bSeenByGameBlocksReoffer && Context.bSeenByGame)
		{
			continue;
		}
		if (Conversation->Header.bSeenByPlayerBlocksReoffer && Context.bSeenByPlayer)
		{
			continue;
		}
		if (Conversation->Header.bBlockOfferPerCycle && (Context.bSeenByPlayer || bSkippedForCycle))
		{
			continue;
		}

		// Predicate queries intentionally avoid chance rolls and side effects.
		return true;
	}

	return false;
}

void UParleyDialogueSubsystem::GetRegisteredPrimarySpeakerTags(TArray<FGameplayTag>& OutSpeakerTags) const
{
	OutSpeakerTags.Reset();

	const FParleyDialogueRuntimeState& Runtime = GetRuntimeState();
	TSet<FGameplayTag> UniqueTags;
	UniqueTags.Reserve(Runtime.SpeakerRowsByTag.Num() + Runtime.ConversationsByTag.Num());

	for (const TPair<FGameplayTag, FParleySpeakerRow>& Pair : Runtime.SpeakerRowsByTag)
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

		const FGameplayTag Slot = GetCharacterTagFromPlayerState(PS);
		if (!Slot.IsValid())
		{
			continue;
		}
		if (HasUnlockedDialogueForSpeakerForCharacter(PrimarySpeakerTag, GetDefaultCharacterTagForSlot(Slot)))
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

	return FindPerPlayerSessionByPrimarySpeaker(GetRuntimeState().ActiveSessions, PrimarySpeakerTag, FGameplayTag()) != nullptr;
}

bool UParleyDialogueSubsystem::GetPrimarySpeakerForConversation(FGameplayTag ConversationTag, FGameplayTag& OutPrimarySpeakerTag) const
{
	OutPrimarySpeakerTag = FGameplayTag();
	if (!ConversationTag.IsValid())
	{
		return false;
	}

	const FParleyDialogueRuntimeState& Runtime = GetRuntimeState();
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

	const FGameplayTag RequesterSlot = GetCharacterTagFromController(RequestingController);
	if (!RequesterSlot.IsValid())
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
	const FGameplayTag Slot = GetCharacterTagFromController(RequestingController);
	if (!Slot.IsValid())
	{
		return false;
	}
	const FParleyActiveDialogueSession* Session = FindSessionForCharacter(GetRuntimeState().ActiveSessions, Slot);
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
	OutView.bIsEavesdropping = !Session->bIsSharedSession && Slot != Session->OwnerCharacterTag;
	OutView.InitiatorCharacterTag = GetDefaultCharacterTagForSlot(Session->InitiatorCharacterTag);
	OutView.OwnerCharacterTag = GetDefaultCharacterTagForSlot(Session->OwnerCharacterTag);
	OutView.Choices = Session->CurrentChoices;
	return true;
}

bool UParleyDialogueSubsystem::HasActiveDialogueSession() const
{
	return GetRuntimeState().ActiveSessions.Num() > 0;
}

void UParleyDialogueSubsystem::ClearConversationCycleOfferState(const FGameplayTag OwnerCharacterTag)
{
	UWorld* World = GetWorld();
	if (!IsAuthorityWorld_Dialogue(World))
	{
		UE_LOG(ParleyLog, Verbose, TEXT("[Dialogue] ClearConversationCycleOfferState ignored: authority required."));
		return;
	}

	const FGameplayTag PlayerCharacterTag = NormalizeCharacterTagForDialogue(OwnerCharacterTag);
	FParleyDialogueRuntimeState& Runtime = GetRuntimeState();
	FParleyProgressionStore* ProgressionStore = GetProgressionStore(this);
	FParleyProgressionMutator* ProgressionMutator = GetProgressionMutator(this);
	bool bProgressionChanged = false;

	if (!PlayerCharacterTag.IsValid())
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

	Runtime.SeenByPlayerTransient.Remove(PlayerCharacterTag);
	Runtime.SkippedByPlayerTransient.Remove(PlayerCharacterTag);
	Runtime.SpeakerOfferCountsByPlayerTransient.Remove(PlayerCharacterTag);

	if (ProgressionStore)
	{
		const APlayerState* PlayerState = FindPlayerStateByCharacterTag(World, PlayerCharacterTag);
		const FGameplayTag CharacterTag = PlayerState
			? ResolveDialogueCharacterTagFromPlayerState(PlayerState)
			: GetDefaultCharacterTagForSlot(PlayerCharacterTag);
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
		LexToStringParleySlot(PlayerCharacterTag));
}

float UParleyDialogueSubsystem::GetRelationshipPointsForSpeakerPair(
	const FGameplayTag SourceSpeakerTag,
	const FGameplayTag TargetSpeakerTag) const
{
	const FParleyProgressionStore* ProgressionStore = GetProgressionStore(this);
	if (!ProgressionStore || !SourceSpeakerTag.IsValid() || !TargetSpeakerTag.IsValid())
	{
		return 0.0f;
	}

	for (const FDialogueSpeakerRelationshipState& State : ProgressionStore->DialogueSpeakerRelationshipStates)
	{
		if (State.SourceSpeakerTag.MatchesTagExact(SourceSpeakerTag)
			&& State.TargetSpeakerTag.MatchesTagExact(TargetSpeakerTag))
		{
			return State.RelationshipPoints;
		}
	}

	return 0.0f;
}

int32 UParleyDialogueSubsystem::GetRelationshipLevelForSpeakerPair(
	const FGameplayTag SourceSpeakerTag,
	const FGameplayTag TargetSpeakerTag) const
{
	const float Points = GetRelationshipPointsForSpeakerPair(SourceSpeakerTag, TargetSpeakerTag);
	if (!TargetSpeakerTag.IsValid())
	{
		return 0;
	}

	const FParleyDialogueRuntimeState& Runtime = GetRuntimeState();
	FGameplayTag CandidateSpeakerTag = TargetSpeakerTag;
	while (CandidateSpeakerTag.IsValid())
	{
		if (const FParleySpeakerRow* SpeakerRow = Runtime.SpeakerRowsByTag.Find(CandidateSpeakerTag))
		{
			return ResolveRelationshipLevelFromThresholds(Points, SpeakerRow->RelationshipThresholds);
		}
		CandidateSpeakerTag = StripLeafGameplayTag(CandidateSpeakerTag);
	}

	return 0;
}

void UParleyDialogueSubsystem::SetProgressionStateForCharacter(FGameplayTag OwnerCharacterTag, const FParleyProgressionState& State)
{
	const FGameplayTag PlayerCharacterTag = NormalizeCharacterTagForDialogue(OwnerCharacterTag);
	if (!PlayerCharacterTag.IsValid())
	{
		UE_LOG(ParleyLog, Verbose, TEXT("[Dialogue] SetProgressionStateForCharacter ignored: unresolved slot tag '%s'."), *OwnerCharacterTag.ToString());
		return;
	}

	FParleyProgressionStore* ProgressionStore = GetProgressionStore(this);
	if (!ProgressionStore)
	{
		return;
	}

	const FGameplayTag CharacterTag = State.CharacterTag.IsValid()
		? State.CharacterTag
		: GetDefaultCharacterTagForSlot(PlayerCharacterTag);
	if (!CharacterTag.IsValid())
	{
		UE_LOG(ParleyLog, Verbose, TEXT("[Dialogue] SetProgressionStateForCharacter ignored: unresolved character tag for slot %s."),
			LexToStringParleySlot(PlayerCharacterTag));
		return;
	}

	ProgressionStore->CharacterTagByIdentity.FindOrAdd(PlayerCharacterTag) = CharacterTag;
	FParleyCharacterProgressionData& CharacterState = ProgressionStore->FindOrAddCharacterStateData(CharacterTag);
	FDialoguePlayerPersistentState& DialogueState = CharacterState.DialogueState;
	DialogueState.OwnerCharacterTag = GetDefaultCharacterTagForSlot(PlayerCharacterTag);
	DialogueState.CharacterTag = CharacterTag;
	DialogueState.ProgressionTags = State.ProgressionTags;
	DialogueState.CompletedConversationTags = State.CompletedConversationTags;
	DialogueState.CompletedChoiceRecords = State.CompletedChoiceRecords;
	DialogueState.SeenConversationTagsThisCycle = State.SeenConversationTagsThisCycle;
	DialogueState.SkippedConversationTagsThisCycle = State.SkippedConversationTagsThisCycle;
	DialogueState.SpeakerOfferCountsThisCycle = State.SpeakerOfferCountsThisCycle;

	FParleyDialogueRuntimeState& Runtime = GetRuntimeState();
	Runtime.SeenByPlayerTransient.FindOrAdd(PlayerCharacterTag) = State.SeenConversationTagsThisCycle;
	Runtime.SkippedByPlayerTransient.FindOrAdd(PlayerCharacterTag) = State.SkippedConversationTagsThisCycle;
	TMap<FGameplayTag, int32> OfferCountMap;
	BuildSpeakerOfferCountMap(State.SpeakerOfferCountsThisCycle, OfferCountMap);
	Runtime.SpeakerOfferCountsByPlayerTransient.FindOrAdd(PlayerCharacterTag) = MoveTemp(OfferCountMap);
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

void UParleyDialogueSubsystem::SetSpeakerRelationshipStates(const TArray<FDialogueSpeakerRelationshipState>& States)
{
	if (FParleyProgressionStore* ProgressionStore = GetProgressionStore(this))
	{
		ProgressionStore->DialogueSpeakerRelationshipStates.Reset();
		TMap<FString, int32> PairIndexByKey;
		for (const FDialogueSpeakerRelationshipState& State : States)
		{
			if (!State.SourceSpeakerTag.IsValid() || !State.TargetSpeakerTag.IsValid())
			{
				continue;
			}

			const FString PairKey = FString::Printf(TEXT("%s|%s"), *State.SourceSpeakerTag.ToString(), *State.TargetSpeakerTag.ToString());
			if (const int32* ExistingIndex = PairIndexByKey.Find(PairKey))
			{
				if (ProgressionStore->DialogueSpeakerRelationshipStates.IsValidIndex(*ExistingIndex))
				{
					ProgressionStore->DialogueSpeakerRelationshipStates[*ExistingIndex].RelationshipPoints = State.RelationshipPoints;
				}
				continue;
			}

			const int32 AddedIndex = ProgressionStore->DialogueSpeakerRelationshipStates.Num();
			FDialogueSpeakerRelationshipState& Added = ProgressionStore->DialogueSpeakerRelationshipStates.AddDefaulted_GetRef();
			Added.SourceSpeakerTag = State.SourceSpeakerTag;
			Added.TargetSpeakerTag = State.TargetSpeakerTag;
			Added.RelationshipPoints = State.RelationshipPoints;
			PairIndexByKey.Add(PairKey, AddedIndex);
		}
	}
}
