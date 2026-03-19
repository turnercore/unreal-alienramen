// Split from ParleyDialogueSubsystem.cpp for maintainability.

bool UParleyDialogueSubsystem::GetAvailableConversationForSpeaker(
	APlayerController* RequestingController,
	FGameplayTag PrimarySpeakerTag,
	FDialogueConversationOffer& OutOffer,
	const bool bSpeakerLocalStateAllowsDialogue)
{
	return GetAvailableConversationForSpeakerInternal(
		RequestingController,
		PrimarySpeakerTag,
		OutOffer,
		bSpeakerLocalStateAllowsDialogue,
		FGameplayTag(),
		/*bPersistChanceSkipFailures=*/ false);
}

bool UParleyDialogueSubsystem::GetAvailableConversationForSpeakerInternal(
	APlayerController* RequestingController,
	FGameplayTag PrimarySpeakerTag,
	FDialogueConversationOffer& OutOffer,
	const bool bSpeakerLocalStateAllowsDialogue,
	const FGameplayTag SourceSpeakerTagOverride,
	const bool bPersistChanceSkipFailures)
{
	OutOffer = FDialogueConversationOffer();
	const UWorld* World = GetWorld();
	if (!IsAuthorityWorld_Dialogue(World))
	{
		UE_LOG(ParleyLog, Verbose, TEXT("[Dialogue] Offer request ignored: authority required."));
		return false;
	}
	if (!RequestingController)
	{
		UE_LOG(ParleyLog, Verbose, TEXT("[Dialogue] Offer request ignored: RequestingController is null."));
		return false;
	}
	if (!PrimarySpeakerTag.IsValid())
	{
		UE_LOG(ParleyLog, Verbose, TEXT("[Dialogue] Offer request ignored: PrimarySpeakerTag is invalid."));
		return false;
	}

	if (!bSpeakerLocalStateAllowsDialogue)
	{
		UE_LOG(ParleyLog, Verbose, TEXT("[Dialogue] Offer blocked: speaker local state disallows dialogue for speaker '%s'."), *PrimarySpeakerTag.ToString());
		return false;
	}

	APlayerState* RequesterPS = RequestingController->GetPlayerState<APlayerState>();
	if (!RequesterPS)
	{
		UE_LOG(ParleyLog, Verbose, TEXT("[Dialogue] Offer blocked: RequestingController has no PlayerState for speaker '%s'."), *PrimarySpeakerTag.ToString());
		return false;
	}

	const FParleyPlayerIdentity PlayerIdentity = BuildPlayerIdentityFromState(RequesterPS);
	const FGameplayTag RequesterSlot = GetCharacterTagFromPlayerState(RequesterPS);
	if (!RequesterSlot.IsValid())
	{
		UE_LOG(ParleyLog, Verbose, TEXT("[Dialogue] Offer blocked: Requester slot is Unknown for speaker '%s'."), *PrimarySpeakerTag.ToString());
		return false;
	}
	FParleyDialogueRuntimeState& Runtime = GetRuntimeState();
	SyncCycleOfferStateFromProgressionStoreForCharacter(this, RequesterSlot, Runtime.SeenByPlayerTransient, Runtime.SkippedByPlayerTransient);
	SyncSpeakerOfferCountsFromProgressionStoreForCharacter(this, RequesterSlot, Runtime.SpeakerOfferCountsByPlayerTransient);

	const FParleySpeakerRow* SpeakerRow = Runtime.SpeakerRowsByTag.Find(PrimarySpeakerTag);
	const int32 MaxOffersPerCycle = SpeakerRow ? FMath::Max(0, SpeakerRow->MaxOffersPerCycle) : 0;
	const int32 ExistingOfferCount = Runtime.SpeakerOfferCountsByPlayerTransient.FindOrAdd(RequesterSlot).FindRef(PrimarySpeakerTag);
	if (MaxOffersPerCycle > 0 && ExistingOfferCount >= MaxOffersPerCycle)
	{
		UE_LOG(
			ParleyLog,
			Verbose,
			TEXT("[Dialogue] Offer blocked for speaker '%s': cycle offer cap reached (%d/%d) for slot %s."),
			*PrimarySpeakerTag.ToString(),
			ExistingOfferCount,
			MaxOffersPerCycle,
			LexToStringParleySlot(RequesterSlot));
		return false;
	}
	const UParleyDialogueSettings* Settings = GetDefault<UParleyDialogueSettings>();
	const FGameplayTag ModeTag = GetCurrentModeTag(this, World);
	if (IsBusySpeakerLockEnabled(Settings, ModeTag))
	{
		if (const FParleyActiveDialogueSession* BusySession = FindPerPlayerSessionByPrimarySpeaker(Runtime.ActiveSessions, PrimarySpeakerTag, RequesterSlot))
		{
			UE_LOG(ParleyLog, Verbose,
				TEXT("[Dialogue] Offer blocked: speaker '%s' is busy in active session '%s' owned by slot %s."),
				*PrimarySpeakerTag.ToString(),
				*BusySession->SessionId,
				LexToStringParleySlot(BusySession->OwnerCharacterTag));
			return false;
		}
	}

	TArray<FDialogueCandidateEval> Unseen;
	TArray<FDialogueCandidateEval> Catchup;
	TArray<FDialogueCandidateEval> Repeatable;
	FGameplayTagContainer& SkippedForPlayer = Runtime.SkippedByPlayerTransient.FindOrAdd(RequesterSlot);

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
			int32 ErrorCount = 0;
			for (const FDialogueValidationIssue& Issue : Validation.Issues)
			{
				if (Issue.Severity == EDialogueValidationSeverity::Error)
				{
					++ErrorCount;
				}
			}
			UE_LOG(ParleyLog, Warning,
				TEXT("[Dialogue] Offer skipped: conversation '%s' is invalid (%d issues, %d errors)."),
				*Conversation->Header.ConversationTag.ToString(),
				Validation.Issues.Num(),
				ErrorCount);
			continue;
		}

		FDialogueRuntimeContext Context = BuildOfferContext(this, Conversation, RequesterPS, PlayerIdentity, SourceSpeakerTagOverride);
		Context.bSeenByGame = Runtime.SeenByGameTransient.HasTagExact(Conversation->Header.ConversationTag);
		if (const FGameplayTagContainer* SeenForPlayer = Runtime.SeenByPlayerTransient.Find(RequesterSlot))
		{
			Context.bSeenByPlayer = SeenForPlayer->HasTagExact(Conversation->Header.ConversationTag);
		}
		const bool bSkippedThisCycle = SkippedForPlayer.HasTagExact(Conversation->Header.ConversationTag);
		FString OfferGateFailure;
		if (!EvaluateConversationOfferRules(this, Context, Conversation->Header, &OfferGateFailure))
		{
			UE_LOG(ParleyLog, Verbose,
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
		Candidate.bSeenThisCycle = Context.bSeenByPlayer;
		Candidate.bSkippedThisCycle = bSkippedThisCycle;
		Candidate.OfferWeight = FMath::Max(1, Conversation->Header.OfferWeight);
		Candidate.ChanceOffered = FMath::Clamp(Conversation->Header.ChanceOffered, 0.0f, 1.0f);
		Candidate.EffectivePriority = Candidate.Priority;

		if (!Conversation->Header.bRepeatable && Candidate.bCompletedByPlayer)
		{
			UE_LOG(ParleyLog, Verbose,
				TEXT("[Dialogue] Offer skipped '%s': non-repeatable and already completed by requesting player."),
				*Conversation->Header.ConversationTag.ToString());
			continue;
		}
		if (Conversation->Header.bCompletedByGameBlocksReoffer && Candidate.bCompletedByGame)
		{
			UE_LOG(ParleyLog, Verbose,
				TEXT("[Dialogue] Offer skipped '%s': bCompletedByGameBlocksReoffer is true and conversation is completed by game."),
				*Conversation->Header.ConversationTag.ToString());
			continue;
		}
		if (Conversation->Header.bSeenByGameBlocksReoffer && Candidate.bSeenByGame)
		{
			UE_LOG(ParleyLog, Verbose,
				TEXT("[Dialogue] Offer skipped '%s': bSeenByGameBlocksReoffer is true and conversation is seen by game."),
				*Conversation->Header.ConversationTag.ToString());
			continue;
		}
		if (Conversation->Header.bSeenByPlayerBlocksReoffer && Candidate.bSeenByPlayer)
		{
			UE_LOG(ParleyLog, Verbose,
				TEXT("[Dialogue] Offer skipped '%s': bSeenByPlayerBlocksReoffer is true and conversation is seen by requesting player."),
				*Conversation->Header.ConversationTag.ToString());
			continue;
		}

		if (Conversation->Header.bBlockOfferPerCycle && (Candidate.bSeenThisCycle || Candidate.bSkippedThisCycle))
		{
			UE_LOG(ParleyLog, Verbose,
				TEXT("[Dialogue] Offer skipped '%s': blocked for requester this cycle (seen=%d skipped=%d)."),
				*Conversation->Header.ConversationTag.ToString(),
				Candidate.bSeenThisCycle ? 1 : 0,
				Candidate.bSkippedThisCycle ? 1 : 0);
			continue;
		}

		if (Candidate.bSeenThisCycle || Candidate.bSkippedThisCycle || (Candidate.bRepeatable && Candidate.bCompletedByPlayer))
		{
			Candidate.EffectivePriority = 1;
		}

		if (Candidate.ChanceOffered < 1.0f)
		{
			const float ChanceRoll = FMath::FRand();
			if (ChanceRoll > Candidate.ChanceOffered)
			{
				if (bPersistChanceSkipFailures && Conversation->Header.ConversationTag.IsValid())
				{
					SkippedForPlayer.AddTag(Conversation->Header.ConversationTag);
					PersistCycleOfferStateForCharacter(this, RequesterSlot, Runtime.SeenByPlayerTransient, Runtime.SkippedByPlayerTransient, true);
				}
				UE_LOG(ParleyLog, Verbose,
					TEXT("[Dialogue] Offer chance skipped '%s': roll %.3f > chance %.3f."),
					*Conversation->Header.ConversationTag.ToString(),
					ChanceRoll,
					Candidate.ChanceOffered);
				continue;
			}
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
		const int32 BestPriority = Bucket[0].EffectivePriority;
		TArray<int32> Tied;
		int32 TotalWeight = 0;
		for (int32 Index = 0; Index < Bucket.Num(); ++Index)
		{
			if (Bucket[Index].EffectivePriority != BestPriority)
			{
				break;
			}
			Tied.Add(Index);
			TotalWeight += FMath::Max(1, Bucket[Index].OfferWeight);
		}

		int32 WeightRoll = FMath::RandRange(1, FMath::Max(1, TotalWeight));
		for (const int32 CandidateIndex : Tied)
		{
			WeightRoll -= FMath::Max(1, Bucket[CandidateIndex].OfferWeight);
			if (WeightRoll <= 0)
			{
				OutCandidate = Bucket[CandidateIndex];
				return true;
			}
		}

		OutCandidate = Bucket[Tied.Last()];
		return true;
	};

	FDialogueCandidateEval Picked;
	if (!SelectCandidate(Unseen, Picked) && !SelectCandidate(Catchup, Picked) && !SelectCandidate(Repeatable, Picked))
	{
		UE_LOG(ParleyLog, Verbose, TEXT("[Dialogue] Offer: no valid conversations for speaker '%s'."), *PrimarySpeakerTag.ToString());
		return false;
	}

	OutOffer.ConversationTag = Picked.Conversation ? Picked.Conversation->Header.ConversationTag : FGameplayTag();
	OutOffer.Priority = Picked.EffectivePriority;
	OutOffer.bUnseenByGame = !Picked.bSeenByGame;
	OutOffer.bUnseenByPlayer = !Picked.bSeenByPlayer;
	OutOffer.bCatchUpCandidate = Picked.bSeenByGame && !Picked.bSeenByPlayer;
	OutOffer.bRepeatableCandidate = Picked.bRepeatable;

	const FString SlotString = LexToStringParleySlot(RequesterSlot);
	const FString BucketLabel = OutOffer.bUnseenByGame && OutOffer.bUnseenByPlayer
		? TEXT("Unseen")
		: (OutOffer.bCatchUpCandidate ? TEXT("CatchUp") : (OutOffer.bRepeatableCandidate ? TEXT("Repeatable") : TEXT("Seen")));
	UE_LOG(ParleyLog, Verbose,
		TEXT("[Dialogue] Offer resolved: slot %s speaker '%s' -> conversation '%s' (priority %d effective %d, weight %d, bucket %s)."),
		*SlotString,
		*PrimarySpeakerTag.ToString(),
		*OutOffer.ConversationTag.ToString(),
		Picked.Priority,
		OutOffer.Priority,
		Picked.OfferWeight,
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

static void FillClientViewForCharacter(const FParleyActiveDialogueSession& Session, const FGameplayTag Slot, FDialogueClientView& OutView)
{
	OutView = FDialogueClientView();
	OutView.SessionId = Session.SessionId;
	OutView.ConversationTag = Session.ConversationTag;
	OutView.CurrentNodeId = Session.CurrentNodeId;
	OutView.SpeakerTag = Session.CurrentSpeakerTag;
	OutView.SpeakerLineFontStyleTag = Session.CurrentSpeakerLineFontStyleTag;
	OutView.SpeakerLineFont = Session.CurrentSpeakerLineFont;
	OutView.LineText = Session.CurrentLineText;
	OutView.SpeakerPortrait = Session.CurrentSpeakerPortrait;
	OutView.Choices = Session.CurrentChoices;
	OutView.bWaitingForChoice = Session.bWaitingForChoice;
	OutView.bConversationImportant = Session.bConversationImportant;
	OutView.bIsEavesdropping = !Session.bIsSharedSession && Slot != Session.OwnerCharacterTag;
	OutView.InitiatorCharacterTag = GetDefaultCharacterTagForSlot(Session.InitiatorCharacterTag);
	OutView.OwnerCharacterTag = GetDefaultCharacterTagForSlot(Session.OwnerCharacterTag);
}

static void BroadcastSessionUpdated(UParleyDialogueSubsystem* DialogueSubsystem, const FParleyActiveDialogueSession& Session)
{
	if (!DialogueSubsystem)
	{
		return;
	}

	for (const FGameplayTag Slot : Session.Participants)
	{
		if (!Slot.IsValid())
		{
			continue;
		}

		FDialogueClientView View;
		FillClientViewForCharacter(Session, Slot, View);
		DialogueSubsystem->OnDialogueSessionUpdated.Broadcast(View);

		if (APlayerController* TargetController = FindPlayerControllerByCharacter(DialogueSubsystem->GetWorld(), Slot))
		{
			if (IParleyPlayerControllerInterface* ControllerInterface = Cast<IParleyPlayerControllerInterface>(TargetController))
			{
				ControllerInterface->NotifyDialogueViewUpdated(View);
			}
		}
	}
}

static void DispatchDialogueAudioRequestToParticipants(
	UParleyDialogueSubsystem* DialogueSubsystem,
	const FParleyActiveDialogueSession& Session,
	const FDialogueAudioRequest& Request)
{
	if (!DialogueSubsystem)
	{
		return;
	}

	// Subsystem-level listeners get one authority-side notification per resolved line.
	// Per-local-player routing happens below via controller interface dispatch.
	DialogueSubsystem->OnDialogueAudioRequested.Broadcast(Request);

	for (const FGameplayTag Slot : Session.Participants)
	{
		if (!Slot.IsValid())
		{
			continue;
		}

		APlayerController* TargetController = FindPlayerControllerByCharacter(DialogueSubsystem->GetWorld(), Slot);
		IParleyPlayerControllerInterface* ControllerInterface = Cast<IParleyPlayerControllerInterface>(TargetController);
		if (!ControllerInterface)
		{
			continue;
		}

		FDialogueAudioRequest PerPlayerRequest = Request;
		// ListenerCharacterTag is local-delivery context and must be rewritten per participant.
		PerPlayerRequest.ListenerCharacterTag = GetDefaultCharacterTagForSlot(Slot);
		ControllerInterface->NotifyDialogueAudioRequested(PerPlayerRequest);
	}
}

static int32 FindSessionIndexForCharacter(const TArray<FParleyActiveDialogueSession>& Sessions, const FGameplayTag Slot)
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

static void ClearSessionPresentationState(FParleyActiveDialogueSession& Session)
{
	Session.bWaitingForChoice = false;
	Session.bWaitingForAdvanceInput = false;
	Session.bChoiceRequiresAllViewers = false;
	Session.WaitingChoiceNodeId.Invalidate();
	Session.WaitingLineNodeId.Invalidate();
	Session.WaitingMultiLineEntryIndex = INDEX_NONE;
	Session.CurrentChoices.Reset();
	Session.CurrentSpeakerTag = FGameplayTag();
	Session.CurrentSpeakerLineFontStyleTag = NAME_None;
	Session.CurrentSpeakerLineFont = TSoftObjectPtr<UFont>();
	Session.CurrentLineText = FText::GetEmpty();
	Session.CurrentSpeakerPortrait = FSpeakerPortraitData();
}

static FParleyPlayerIdentity BuildOwnerIdentityForSession(const UWorld* World, const FParleyActiveDialogueSession& Session)
{
	const APlayerState* OwnerPlayerState = FindPlayerStateByCharacterTag(World, Session.OwnerCharacterTag);
	return BuildPlayerIdentityFromState(OwnerPlayerState);
}

static FDialogueRuntimeContext BuildSessionContext(
	const UParleyDialogueSubsystem* DialogueSubsystem,
	const FParleyActiveDialogueSession& Session,
	const TMap<FGameplayTag, FGameplayTagContainer>& SeenByPlayerTransient,
	const FGameplayTagContainer& SeenByGameTransient)
{
	FDialogueRuntimeContext Context;
	Context.ConversationTag = Session.ConversationTag;
	Context.ConversationAsset = Session.ConversationAsset;
	Context.PrimarySpeakerTag = Session.PrimarySpeakerTag;
	Context.World = DialogueSubsystem ? DialogueSubsystem->GetWorld() : nullptr;
	Context.GameState = Context.World ? Context.World->GetGameState() : nullptr;
	Context.ActivePlayerController = FindPlayerControllerByCharacter(Context.World, Session.OwnerCharacterTag);
	Context.ActivePlayerState = FindPlayerStateByCharacterTag(Context.World, Session.OwnerCharacterTag);
	Context.ActivePawn = Context.ActivePlayerController ? Context.ActivePlayerController->GetPawn() : nullptr;

	FGameplayTag OtherSlot = FGameplayTag();
	for (const FGameplayTag Candidate : Session.Participants)
	{
		if (Candidate != Session.OwnerCharacterTag && Candidate.IsValid())
		{
			OtherSlot = Candidate;
			break;
		}
	}
	Context.OtherPlayerController = FindPlayerControllerByCharacter(Context.World, OtherSlot);
	Context.OtherPlayerState = FindPlayerStateByCharacterTag(Context.World, OtherSlot);
	Context.OtherPawn = Context.OtherPlayerController ? Context.OtherPlayerController->GetPawn() : nullptr;

	const APlayerState* ActiveARPS = Cast<APlayerState>(Context.ActivePlayerState);
	if (ActiveARPS)
	{
		Context.ResolvedPlayerSpeakerTag = ResolvePlayerSpeakerTag(ActiveARPS);
		GetLoadoutTagsFromPlayerState(ActiveARPS, Context.LoadoutView.LoadoutTags);
	}
	Context.SourceSpeakerTag = Session.SourceSpeakerTag.IsValid()
		? ResolveSpeakerTagForContext(Session.SourceSpeakerTag, Context, Context.ResolvedPlayerSpeakerTag)
		: Context.ResolvedPlayerSpeakerTag;

	const FParleyProgressionStore* ProgressionStore = GetProgressionStore(DialogueSubsystem);
	if (ProgressionStore)
	{
		Context.GameOnlyProgressionTags = ProgressionStore->ProgressionTags;
		const FParleyPlayerIdentity Identity = BuildOwnerIdentityForSession(Context.World, Session);
		GetProgressionTagsForIdentity(ProgressionStore, Identity, Context.PlayerOnlyProgressionTags, Context.World);
		Context.CombinedProgressionTags = DialogueSubsystem->GetCombinedDialogueTags(Context.PlayerOnlyProgressionTags, Context.GameOnlyProgressionTags);
		Context.bCompletedByGame = ProgressionStore->DialogueCompletedConversationTagsByGame.HasTagExact(Session.ConversationTag);
		if (const FDialoguePlayerPersistentState* PlayerState = FindPlayerDialogueState(ProgressionStore, Identity, Context.World))
		{
			Context.bCompletedByPlayer = PlayerState->CompletedConversationTags.HasTagExact(Session.ConversationTag);
		}
	}

	Context.TransientConversationTags = Session.TransientConversationTags;
	Context.bSeenByGame = SeenByGameTransient.HasTagExact(Session.ConversationTag);
	if (const FGameplayTagContainer* SeenForOwner = SeenByPlayerTransient.Find(Session.OwnerCharacterTag))
	{
		Context.bSeenByPlayer = SeenForOwner->HasTagExact(Session.ConversationTag);
	}

	Context.RelationshipPointsForPrimarySpeaker = DialogueSubsystem->GetRelationshipPointsForSpeakerPair(Context.SourceSpeakerTag, Session.PrimarySpeakerTag);
	Context.RelationshipLevelForPrimarySpeaker = DialogueSubsystem->GetRelationshipLevelForSpeakerPair(Context.SourceSpeakerTag, Session.PrimarySpeakerTag);
	return Context;
}

static UParleySpeakerComponent* FindSpeakerComponentForSpeakerTag(UWorld* World, const FGameplayTag& SpeakerTag)
{
	if (!World || !SpeakerTag.IsValid())
	{
		return nullptr;
	}

	const auto MatchesSpeakerTag = [&SpeakerTag](const FGameplayTag& CandidateTag) -> bool
	{
		return CandidateTag.IsValid()
			&& (SpeakerTag.MatchesTag(CandidateTag) || CandidateTag.MatchesTag(SpeakerTag));
	};

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (AActor* Actor = *It)
		{
			if (UParleySpeakerComponent* SpeakerComponent = Actor->FindComponentByClass<UParleySpeakerComponent>())
			{
				if (MatchesSpeakerTag(SpeakerComponent->GetSpeakerTag()))
				{
					return SpeakerComponent;
				}
			}
		}
	}

	return nullptr;
}

static void ApplyDialogueEmotionForPresentedSpeaker(
	FParleyActiveDialogueSession& Session,
	const TMap<FGameplayTag, FParleySpeakerRow>& SpeakerRowsByTag,
	const FDialogueRuntimeContext& Context,
	const FGameplayTag& ResolvedSpeakerTag)
{
	if (!ResolvedSpeakerTag.IsValid())
	{
		return;
	}

	UParleySpeakerComponent* SpeakerComponent = FindSpeakerComponentForSpeakerTag(Context.World, ResolvedSpeakerTag);
	if (!SpeakerComponent)
	{
		UE_LOG(
			ParleyLog,
			Verbose,
			TEXT("[Dialogue] Emotion request skipped: no speaker component found for '%s'."),
			*ResolvedSpeakerTag.ToString());
		return;
	}

	FGameplayTag SpeakerRowTag;
	ResolveSpeakerRowForPresentation(SpeakerRowsByTag, ResolvedSpeakerTag, SpeakerRowTag);
	const FGameplayTag PresentationEmotionTag = ResolvePresentationEmotionTagFromSpeakerTag(ResolvedSpeakerTag, SpeakerRowTag);
	if (!PresentationEmotionTag.IsValid())
	{
		return;
	}

	UE_LOG(
		ParleyLog,
		Verbose,
		TEXT("[Dialogue] Requesting presented-speaker emotion: SpeakerTag=%s OwnerCharacter=%s EmotionTag=%s"),
		*ResolvedSpeakerTag.ToString(),
		*GetDefaultCharacterTagForSlot(Session.OwnerCharacterTag).ToString(),
		*PresentationEmotionTag.ToString());
	SpeakerComponent->OnSpeakerEmotionRequested.Broadcast(
		PresentationEmotionTag,
		GetDefaultCharacterTagForSlot(Session.OwnerCharacterTag),
		true);
	Session.SpeakerComponentsWithEmotionOverride.Add(SpeakerComponent);
}

static void ClearDialogueEmotionOverridesForSession(FParleyActiveDialogueSession& Session, const bool bResetTrackedComponents)
{
	for (const TWeakObjectPtr<UParleySpeakerComponent>& WeakSpeakerComponent : Session.SpeakerComponentsWithEmotionOverride)
	{
		if (UParleySpeakerComponent* SpeakerComponent = WeakSpeakerComponent.Get())
		{
			SpeakerComponent->OnSpeakerEmotionCleared.Broadcast(GetDefaultCharacterTagForSlot(Session.OwnerCharacterTag));
		}
	}

	if (bResetTrackedComponents)
	{
		Session.SpeakerComponentsWithEmotionOverride.Reset();
	}
}

static void RefreshBusyEmotionForSpeaker(
	UParleyDialogueSubsystem* DialogueSubsystem,
	const FGameplayTag& SpeakerTag,
	const TArray<FParleyActiveDialogueSession>& Sessions)
{
	if (!DialogueSubsystem || !SpeakerTag.IsValid())
	{
		return;
	}

	UWorld* World = DialogueSubsystem->GetWorld();
	UParleySpeakerComponent* SpeakerComponent = FindSpeakerComponentForSpeakerTag(World, SpeakerTag);
	if (!SpeakerComponent)
	{
		return;
	}

	const UParleyDialogueSettings* DialogueSettings = GetDefault<UParleyDialogueSettings>();
	const FGameplayTag ModeTag = GetCurrentModeTag(DialogueSubsystem, World);
	const bool bBusyLockEnabled = IsBusySpeakerLockEnabled(DialogueSettings, ModeTag);
	const bool bSpeakerBusy = bBusyLockEnabled
		&& FindPerPlayerSessionByPrimarySpeaker(Sessions, SpeakerTag, FGameplayTag()) != nullptr;

	if (bSpeakerBusy && DialogueSettings && DialogueSettings->BusySpeakerEmotionTag.IsValid())
	{
		SpeakerComponent->OnSpeakerEmotionRequested.Broadcast(
			DialogueSettings->BusySpeakerEmotionTag,
			FGameplayTag(),
			false);
	}
	else
	{
		SpeakerComponent->OnSpeakerEmotionCleared.Broadcast(FGameplayTag());
	}
}

static void BroadcastChoiceLookaheadPreviewForSpeaker(
	UWorld* World,
	const FGameplayTag& SpeakerTag,
	const FGameplayTag& ViewerSlotTag,
	const FGuid& ChoiceBranchId,
	const FGameplayTag& PreviewEmotionTag)
{
	if (!World || !SpeakerTag.IsValid() || !ChoiceBranchId.IsValid())
	{
		return;
	}

	if (UParleySpeakerComponent* SpeakerComponent = FindSpeakerComponentForSpeakerTag(World, SpeakerTag))
	{
		SpeakerComponent->OnSpeakerEmotionPreviewRequested.Broadcast(
			PreviewEmotionTag,
			ViewerSlotTag,
			ChoiceBranchId);
	}
}

static void ClearChoiceLookaheadPreviewForSpeaker(
	UWorld* World,
	const FGameplayTag& SpeakerTag,
	const FGameplayTag& ViewerSlotTag)
{
	if (!World || !SpeakerTag.IsValid())
	{
		return;
	}

	if (UParleySpeakerComponent* SpeakerComponent = FindSpeakerComponentForSpeakerTag(World, SpeakerTag))
	{
		SpeakerComponent->OnSpeakerEmotionPreviewRequested.Broadcast(
			FGameplayTag(),
			ViewerSlotTag,
			FGuid());
	}
}

static FGameplayTag ResolveLineSpeakerTagForContext(const FDialogueConversationLine& Line, const FDialogueRuntimeContext& Context)
{
	return ResolveSpeakerTagForContext(Line.SpeakerTag, Context, Line.SpeakerTag);
}

static bool DoesSpeakerTagMatchPrimarySpeaker(const FGameplayTag& CandidateSpeakerTag, const FGameplayTag& PrimarySpeakerTag)
{
	if (!CandidateSpeakerTag.IsValid() || !PrimarySpeakerTag.IsValid())
	{
		return false;
	}

	return CandidateSpeakerTag.MatchesTag(PrimarySpeakerTag) || PrimarySpeakerTag.MatchesTag(CandidateSpeakerTag);
}

static bool DoesSpeakerTagMatchActivePlayerLookahead(const FGameplayTag& CandidateSpeakerTag, const FDialogueRuntimeContext& Context)
{
	if (!CandidateSpeakerTag.IsValid())
	{
		return false;
	}

	if (IsRequesterPlaceholderSpeakerTag(CandidateSpeakerTag))
	{
		return true;
	}

	if (!Context.ResolvedPlayerSpeakerTag.IsValid())
	{
		return false;
	}

	return CandidateSpeakerTag.MatchesTag(Context.ResolvedPlayerSpeakerTag)
		|| Context.ResolvedPlayerSpeakerTag.MatchesTag(CandidateSpeakerTag);
}

static bool ShouldShowLineForLookahead(
	const UParleyDialogueSubsystem* DialogueSubsystem,
	const FDialogueLineNodeData& LineData,
	const FDialogueRuntimeContext& Context)
{
	if (!PassesCharacterRestriction(LineData.CharacterRestrictionTag, Context.ResolvedPlayerSpeakerTag))
	{
		return false;
	}

	return PassesLockedConditions(DialogueSubsystem, LineData.SkipLockedConditions, Context)
		&& PassesBlockedConditions(DialogueSubsystem, LineData.SkipBlockedConditions, Context);
}

static bool TryResolveChoiceLookaheadEmotion(
	const UParleyDialogueSubsystem* DialogueSubsystem,
	const FParleyActiveDialogueSession& Session,
	const TMap<FGameplayTag, FGameplayTagContainer>& SeenByPlayerTransient,
	const FGameplayTagContainer& SeenByGameTransient,
	const FGuid StartNodeId,
	FGameplayTag& OutPreviewEmotionTag)
{
	OutPreviewEmotionTag = FGameplayTag();
	if (!DialogueSubsystem || !Session.ConversationAsset || !StartNodeId.IsValid())
	{
		return false;
	}

	const UParleyDialogueSettings* Settings = GetDefault<UParleyDialogueSettings>();
	const int32 MaxSteps = Settings ? FMath::Max(16, Settings->MaxLookaheadSteps) : 1024;

	FGuid CurrentNodeId = StartNodeId;
	for (int32 StepIndex = 0; StepIndex < MaxSteps; ++StepIndex)
	{
		if (!CurrentNodeId.IsValid())
		{
			return false;
		}

		const FDialogueCompiledNode* Node = FindNodeById(Session, CurrentNodeId);
		if (!Node)
		{
			return false;
		}

		FDialogueRuntimeContext Context = BuildSessionContext(
			DialogueSubsystem,
			Session,
			SeenByPlayerTransient,
			SeenByGameTransient);
		Context.ConversationTag = Session.ConversationTag;
		Context.ConversationAsset = Session.ConversationAsset;
		Context.PrimarySpeakerTag = Session.PrimarySpeakerTag;
		Context.TransientConversationTags = Session.TransientConversationTags;

		switch (Node->NodeType)
		{
		case EDialogueNodeType::Enter:
		case EDialogueNodeType::Route:
			CurrentNodeId = Node->NextNodeId;
			break;

		case EDialogueNodeType::Line:
		{
			const FDialogueLineNodeData* LineData = Node->NodeData.GetPtr<FDialogueLineNodeData>();
			if (!LineData)
			{
				return false;
			}

			if (ShouldShowLineForLookahead(DialogueSubsystem, *LineData, Context))
			{
				const FGameplayTag ResolvedSpeakerTag = ResolveLineSpeakerTagForContext(LineData->Line, Context);
				if (DoesSpeakerTagMatchPrimarySpeaker(ResolvedSpeakerTag, Session.PrimarySpeakerTag))
				{
					FGameplayTag SpeakerRowTag;
					ResolveSpeakerRowForPresentation(DialogueSubsystem->GetRuntimeState().SpeakerRowsByTag, ResolvedSpeakerTag, SpeakerRowTag);
					OutPreviewEmotionTag = ResolvePresentationEmotionTagFromSpeakerTag(ResolvedSpeakerTag, SpeakerRowTag);
					return OutPreviewEmotionTag.IsValid();
				}
			}

			CurrentNodeId = Node->NextNodeId;
			break;
		}

		case EDialogueNodeType::MultiLine:
		{
			const FDialogueMultiLineNodeData* MultiLineData = Node->NodeData.GetPtr<FDialogueMultiLineNodeData>();
			if (!MultiLineData)
			{
				return false;
			}

			for (const FDialogueMultiLineEntry& Entry : MultiLineData->Lines)
			{
				if (!ShouldShowLineForLookahead(DialogueSubsystem, Entry.LineData, Context))
				{
					continue;
				}

				const FGameplayTag ResolvedSpeakerTag = ResolveLineSpeakerTagForContext(Entry.LineData.Line, Context);
				if (DoesSpeakerTagMatchPrimarySpeaker(ResolvedSpeakerTag, Session.PrimarySpeakerTag))
				{
					FGameplayTag SpeakerRowTag;
					ResolveSpeakerRowForPresentation(DialogueSubsystem->GetRuntimeState().SpeakerRowsByTag, ResolvedSpeakerTag, SpeakerRowTag);
					OutPreviewEmotionTag = ResolvePresentationEmotionTagFromSpeakerTag(ResolvedSpeakerTag, SpeakerRowTag);
					return OutPreviewEmotionTag.IsValid();
				}
			}

			CurrentNodeId = Node->NextNodeId;
			break;
		}

		case EDialogueNodeType::SplitLine:
		{
			const FDialogueMultiLineNodeData* SplitLineData = Node->NodeData.GetPtr<FDialogueMultiLineNodeData>();
			if (!SplitLineData)
			{
				return false;
			}

			for (const FDialogueMultiLineEntry& Entry : SplitLineData->Lines)
			{
				if (!ShouldShowLineForLookahead(DialogueSubsystem, Entry.LineData, Context))
				{
					continue;
				}

				if (!DoesSpeakerTagMatchActivePlayerLookahead(Entry.LineData.Line.SpeakerTag, Context))
				{
					continue;
				}

				const FGameplayTag ResolvedSpeakerTag = ResolveLineSpeakerTagForContext(Entry.LineData.Line, Context);
				if (DoesSpeakerTagMatchPrimarySpeaker(ResolvedSpeakerTag, Session.PrimarySpeakerTag))
				{
					FGameplayTag SpeakerRowTag;
					ResolveSpeakerRowForPresentation(DialogueSubsystem->GetRuntimeState().SpeakerRowsByTag, ResolvedSpeakerTag, SpeakerRowTag);
					OutPreviewEmotionTag = ResolvePresentationEmotionTagFromSpeakerTag(ResolvedSpeakerTag, SpeakerRowTag);
					return OutPreviewEmotionTag.IsValid();
				}
			}

			CurrentNodeId = Node->NextNodeId;
			break;
		}

		case EDialogueNodeType::Bool:
		{
			const FDialogueBoolNodeData* BoolData = Node->NodeData.GetPtr<FDialogueBoolNodeData>();
			if (!BoolData)
			{
				return false;
			}

			CurrentNodeId = DialogueSubsystem->EvaluateDialogueCondition(BoolData->Condition, Context)
				? Node->TrueNodeId
				: Node->FalseNodeId;
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
					CurrentNodeId = Branch.NextNodeId;
					bMatched = true;
					break;
				}
			}

			if (!bMatched)
			{
				if (!Node->bSwitchHasDefaultOutput)
				{
					return false;
				}

				CurrentNodeId = Node->SwitchDefaultNodeId;
			}
			break;
		}

		case EDialogueNodeType::Random:
		{
			float BestWeight = -1.0f;
			FGuid BestNextNodeId;
			for (const FDialogueCompiledRandomBranch& Branch : Node->RandomBranches)
			{
				if (!Branch.NextNodeId.IsValid() || Branch.Weight <= 0.0f)
				{
					continue;
				}

				if (Branch.Weight > BestWeight)
				{
					BestWeight = Branch.Weight;
					BestNextNodeId = Branch.NextNodeId;
				}
			}

			CurrentNodeId = BestNextNodeId;
			break;
		}

		case EDialogueNodeType::RouteByCharacter:
		{
			bool bMatched = false;
			for (const FDialogueCompiledCharacterRouteBranch& Branch : Node->CharacterRouteBranches)
			{
				if (!DoesSpeakerTagMatchActivePlayerLookahead(Branch.SpeakerTag, Context))
				{
					continue;
				}

				CurrentNodeId = Branch.NextNodeId;
				bMatched = true;
				break;
			}

			if (!bMatched)
			{
				return false;
			}
			break;
		}

		case EDialogueNodeType::Sequence:
		{
			FGuid FirstLinkedBranch;
			for (const FDialogueCompiledSequenceBranch& Branch : Node->SequenceBranches)
			{
				if (Branch.NextNodeId.IsValid())
				{
					FirstLinkedBranch = Branch.NextNodeId;
					break;
				}
			}

			CurrentNodeId = FirstLinkedBranch;
			break;
		}

		case EDialogueNodeType::TagMutation:
		case EDialogueNodeType::RelationshipMutation:
		case EDialogueNodeType::FactionMutation:
		case EDialogueNodeType::Signal:
		case EDialogueNodeType::Completed:
		case EDialogueNodeType::Choice:
		default:
			return false;
		}
	}

	return false;
}

static void AddSessionParticipant(
	FParleyActiveDialogueSession& Session,
	TMap<FGameplayTag, FGameplayTagContainer>& SeenByPlayerTransient,
	const FGameplayTag Slot)
{
	if (!Slot.IsValid())
	{
		return;
	}

	Session.Participants.Add(Slot);
	SeenByPlayerTransient.FindOrAdd(Slot).AddTag(Session.ConversationTag);
}

static bool PersistCompletedConversation(
	UParleyDialogueSubsystem* DialogueSubsystem,
	const FParleyActiveDialogueSession& Session)
{
	FParleyProgressionStore* ProgressionStore = GetProgressionStore(DialogueSubsystem);
	if (!ProgressionStore)
	{
		UE_LOG(
			ParleyLog,
			Warning,
			TEXT("[Dialogue] PersistCompletedConversation failed: runtime progression state unavailable for '%s'."),
			*Session.ConversationTag.ToString());
		DialogueSubsystem->OnConversationCompleted.Broadcast(Session.ConversationTag);
		for (const FGameplayTag Slot : Session.Participants)
		{
			if (!Slot.IsValid())
			{
				continue;
			}

			if (APlayerController* TargetController = FindPlayerControllerByCharacter(DialogueSubsystem->GetWorld(), Slot))
			{
				if (IParleyPlayerControllerInterface* ControllerInterface = Cast<IParleyPlayerControllerInterface>(TargetController))
				{
					ControllerInterface->NotifyDialogueConversationCompleted(
						Session.ConversationTag,
						GetDefaultCharacterTagForSlot(Session.OwnerCharacterTag),
						FGameplayTag());
				}
			}
		}
		return false;
	}

	ProgressionStore->DialogueCompletedConversationTagsByGame.AddTag(Session.ConversationTag);

	const FParleyPlayerIdentity OwnerIdentity = BuildOwnerIdentityForSession(DialogueSubsystem->GetWorld(), Session);
	if (FDialoguePlayerPersistentState* PlayerState = FindOrAddPlayerDialogueState(
		ProgressionStore,
		OwnerIdentity,
		DialogueSubsystem ? DialogueSubsystem->GetWorld() : nullptr))
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

	const FString OwnerCharacterTagString = LexToStringParleySlot(OwnerIdentity.PlayerCharacterTag);
	UE_LOG(ParleyLog, Verbose, TEXT("[Dialogue] Conversation '%s' completed and persisted (PlayerCharacterTag=%s)."),
		*Session.ConversationTag.ToString(),
		*OwnerCharacterTagString);
	DialogueSubsystem->OnConversationCompleted.Broadcast(Session.ConversationTag);
	DialogueSubsystem->OnParleyConversationCompleted.Broadcast(
		Session.ConversationTag,
		GetDefaultCharacterTagForSlot(Session.OwnerCharacterTag),
		ProgressionStore->CharacterTagByIdentity.FindRef(Session.OwnerCharacterTag));
	for (const FGameplayTag Slot : Session.Participants)
	{
		if (!Slot.IsValid())
		{
			continue;
		}

		if (APlayerController* TargetController = FindPlayerControllerByCharacter(DialogueSubsystem->GetWorld(), Slot))
		{
			if (IParleyPlayerControllerInterface* ControllerInterface = Cast<IParleyPlayerControllerInterface>(TargetController))
			{
				ControllerInterface->NotifyDialogueConversationCompleted(
					Session.ConversationTag,
					GetDefaultCharacterTagForSlot(Session.OwnerCharacterTag),
					ProgressionStore->CharacterTagByIdentity.FindRef(Session.OwnerCharacterTag));
			}
		}
	}

	// Keep speaker talkable icons/state in sync after completion changes offer availability.
	if (UGameInstance* GI = DialogueSubsystem->GetGameInstance())
	{
		if (UParleySpeakerSubsystem* SpeakerSubsystem = GI->GetSubsystem<UParleySpeakerSubsystem>())
		{
			SpeakerSubsystem->RefreshAllSpeakerTalkableStates();
		}
	}

	return true;
}

static bool IsBranchConnected(const FGuid& NodeId)
{
	return NodeId.IsValid();
}

static EDialogueExecutionResult ExecuteSessionUntilWait(
	UParleyDialogueSubsystem* DialogueSubsystem,
	FParleyActiveDialogueSession& Session,
	const TMap<FGameplayTag, FParleySpeakerRow>& SpeakerRowsByTag,
	TMap<FGameplayTag, FGameplayTagContainer>& SeenByPlayerTransient,
	const FGameplayTagContainer& SeenByGameTransient,
	const bool bAdvanceLineInput,
	const bool bPreviewMode = false,
	const FDialogueRuntimeContext* PreviewContextOverride = nullptr)
{
	if (!DialogueSubsystem || !Session.ConversationAsset)
	{
		return EDialogueExecutionResult::Failed;
	}

	const UParleyDialogueSettings* Settings = GetDefault<UParleyDialogueSettings>();
	const int32 MaxSteps = Settings ? FMath::Max(16, Settings->MaxExecutionStepsPerAdvance) : 1024;

	auto LogRuntimeError = [&](const FString& Message)
	{
		UE_LOG(ParleyLog, Error, TEXT("[Dialogue] %s (Conversation=%s Session=%s Node=%s)."),
			*Message,
			*Session.ConversationTag.ToString(),
			*Session.SessionId,
			*Session.CurrentNodeId.ToString(EGuidFormats::DigitsWithHyphensInBraces));
	};

	auto LogRuntimeWarning = [&](const FString& Message)
	{
		UE_LOG(ParleyLog, Warning, TEXT("[Dialogue] %s (Conversation=%s Session=%s Node=%s)."),
			*Message,
			*Session.ConversationTag.ToString(),
			*Session.SessionId,
			*Session.CurrentNodeId.ToString(EGuidFormats::DigitsWithHyphensInBraces));
	};

	auto ContinuePendingSequenceBranch = [&Session]() -> bool
	{
		while (!Session.PendingSequenceBranchNodeIds.IsEmpty())
		{
			const FGuid NextNodeId = Session.PendingSequenceBranchNodeIds[0];
			Session.PendingSequenceBranchNodeIds.RemoveAt(0, 1, EAllowShrinking::No);
			if (!NextNodeId.IsValid())
			{
				continue;
			}

			ClearSessionPresentationState(Session);
			Session.CurrentNodeId = NextNodeId;
			return true;
		}

		return false;
	};

	auto PresentLineAndWait = [&](const FDialogueConversationLine& Line, const FDialogueRuntimeContext& Context, const FGuid& WaitingNodeId, const int32 MultiLineEntryIndex) -> EDialogueExecutionResult
	{
		ClearDialogueEmotionOverridesForSession(Session, /*bResetTrackedComponents=*/ true);
		ClearSessionPresentationState(Session);

		const FGameplayTag ResolvedSpeakerTag = ResolveSpeakerTagForContext(Line.SpeakerTag, Context, Line.SpeakerTag);

		Session.CurrentSpeakerTag = ResolvedSpeakerTag;
		Session.CurrentLineText = BuildFormattedDialogueLineText(
			DialogueSubsystem,
			SpeakerRowsByTag,
			Context,
			ResolvedSpeakerTag,
			Line.Text,
			Session.CurrentSpeakerLineFontStyleTag,
			Session.CurrentSpeakerLineFont);
		Session.CurrentSpeakerPortrait = ResolvePortraitForSpeaker(SpeakerRowsByTag, ResolvedSpeakerTag);
		ApplyDialogueEmotionForPresentedSpeaker(Session, SpeakerRowsByTag, Context, ResolvedSpeakerTag);
		Session.bWaitingForAdvanceInput = true;
		Session.WaitingLineNodeId = WaitingNodeId;
		Session.WaitingMultiLineEntryIndex = MultiLineEntryIndex;

		const UParleyDialogueSettings* DialogueSettings = GetDefault<UParleyDialogueSettings>();
		const bool bUseAudioSignals = DialogueSettings
			&& DialogueSettings->DialogueAudioMode == EParleyDialogueAudioMode::AudioSignals;

		USoundBase* EmotionFallbackNativeSound = nullptr;
		FGameplayTag EmotionFallbackCueTag;
		ResolveSpeakerEmotionFallbackAudioForSpeaker(
			SpeakerRowsByTag,
			ResolvedSpeakerTag,
			EmotionFallbackNativeSound,
			EmotionFallbackCueTag);

		USoundBase* ResolvedNativeSound = Line.Sound;
		FGameplayTag ResolvedCueTag = Line.AudioCueTag;
		EDialogueAudioRequestSource AudioRequestSource = EDialogueAudioRequestSource::None;

		// Authoring priority: line payload first, speaker-emotion fallback second.
		if (!ResolvedNativeSound && EmotionFallbackNativeSound)
		{
			ResolvedNativeSound = EmotionFallbackNativeSound;
		}
		if (!ResolvedCueTag.IsValid() && EmotionFallbackCueTag.IsValid())
		{
			ResolvedCueTag = EmotionFallbackCueTag;
		}

		if (bUseAudioSignals)
		{
			// Signal mode intentionally suppresses native audio payloads at source.
			ResolvedNativeSound = nullptr;
			if (Line.AudioCueTag.IsValid())
			{
				AudioRequestSource = EDialogueAudioRequestSource::Line;
			}
			else if (EmotionFallbackCueTag.IsValid())
			{
				AudioRequestSource = EDialogueAudioRequestSource::EmotionFallback;
			}
		}
		else
		{
			// Native mode intentionally suppresses cue-tag payloads at source.
			ResolvedCueTag = FGameplayTag();
			if (Line.Sound)
			{
				AudioRequestSource = EDialogueAudioRequestSource::Line;
			}
			else if (EmotionFallbackNativeSound)
			{
				AudioRequestSource = EDialogueAudioRequestSource::EmotionFallback;
			}
		}

		FDialogueAudioRequest AudioRequest;
		AudioRequest.SessionId = Session.SessionId;
		AudioRequest.LineGuid = Line.LocalLineGuid;
		AudioRequest.ConversationTag = Session.ConversationTag;
		AudioRequest.SpeakerTag = ResolvedSpeakerTag;
		AudioRequest.ListenerCharacterTag = GetDefaultCharacterTagForSlot(Session.OwnerCharacterTag);
		AudioRequest.Source = AudioRequestSource;
		AudioRequest.NativeSound = ResolvedNativeSound;
		AudioRequest.AudioCueTag = ResolvedCueTag;

		const bool bHasAudioPayload = bUseAudioSignals
			? ResolvedCueTag.IsValid()
			: (ResolvedNativeSound != nullptr);
		if (!bPreviewMode && bHasAudioPayload)
		{
			DispatchDialogueAudioRequestToParticipants(DialogueSubsystem, Session, AudioRequest);
		}

		const APlayerState* ActiveARPlayerState = Cast<APlayerState>(Context.ActivePlayerState);
		const bool bAutoAdvanceEnabledForOwner = !bPreviewMode
			&& ActiveARPlayerState
			&& IsDialogueAutoAdvanceEnabledForPlayerState(ActiveARPlayerState);
		if (bAutoAdvanceEnabledForOwner && Context.World)
		{
			float DelaySeconds = Line.LengthSeconds;
			if (ResolvedNativeSound)
			{
				const float SoundDuration = ResolvedNativeSound->GetDuration();
				if (SoundDuration > 0.0f)
				{
					DelaySeconds = SoundDuration;
				}
			}
			DelaySeconds = FMath::Max(0.05f, DelaySeconds);
			FTimerDelegate AutoAdvanceDelegate = FTimerDelegate::CreateLambda(
				[WeakSubsystem = TWeakObjectPtr<UParleyDialogueSubsystem>(DialogueSubsystem), OwnerCharacterTag = Session.OwnerCharacterTag]()
				{
					if (UParleyDialogueSubsystem* Pinned = WeakSubsystem.Get())
					{
						if (APlayerController* OwnerController = FindPlayerControllerByCharacter(Pinned->GetWorld(), OwnerCharacterTag))
						{
							Pinned->AdvanceConversation(OwnerController);
						}
					}
				});

			Context.World->GetTimerManager().SetTimer(Session.AutoAdvanceTimerHandle, AutoAdvanceDelegate, DelaySeconds, false);
		}

		if (!bPreviewMode)
		{
			DialogueSubsystem->OnLineDelivered.Broadcast(
				ResolvedSpeakerTag,
				Session.ConversationTag,
				GetDefaultCharacterTagForSlot(Session.OwnerCharacterTag));
			for (const FGameplayTag Slot : Session.Participants)
			{
				if (!Slot.IsValid())
				{
					continue;
				}

				if (APlayerController* TargetController = FindPlayerControllerByCharacter(DialogueSubsystem->GetWorld(), Slot))
				{
					if (IParleyPlayerControllerInterface* ControllerInterface = Cast<IParleyPlayerControllerInterface>(TargetController))
					{
						ControllerInterface->NotifyDialogueLineDelivered(
							ResolvedSpeakerTag,
							Session.ConversationTag,
							GetDefaultCharacterTagForSlot(Session.OwnerCharacterTag));
					}
				}
			}
		}

		return EDialogueExecutionResult::Waiting;
	};

	auto ShouldShowLineEntry = [&](const FDialogueLineNodeData& LineData, const FDialogueRuntimeContext& Context) -> bool
	{
		if (!PassesCharacterRestriction(LineData.CharacterRestrictionTag, Context.ResolvedPlayerSpeakerTag))
		{
			return false;
		}

		return PassesLockedConditions(DialogueSubsystem, LineData.SkipLockedConditions, Context)
			&& PassesBlockedConditions(DialogueSubsystem, LineData.SkipBlockedConditions, Context);
	};

	auto DoesSpeakerTagMatchActivePlayer = [&](const FGameplayTag& CandidateSpeakerTag, const FDialogueRuntimeContext& Context) -> bool
	{
		if (!CandidateSpeakerTag.IsValid())
		{
			return false;
		}

		if (IsRequesterPlaceholderSpeakerTag(CandidateSpeakerTag))
		{
			return true;
		}

		if (!Context.ResolvedPlayerSpeakerTag.IsValid())
		{
			return false;
		}

		return CandidateSpeakerTag.MatchesTag(Context.ResolvedPlayerSpeakerTag)
			|| Context.ResolvedPlayerSpeakerTag.MatchesTag(CandidateSpeakerTag);
	};

	auto RouteToNextOrPending = [&](const FGuid& NextNodeId, const TCHAR* MissingNextWarning, EDialogueExecutionResult& OutResult) -> bool
	{
		Session.CurrentNodeId = NextNodeId;
		if (Session.CurrentNodeId.IsValid())
		{
			return true;
		}
		if (ContinuePendingSequenceBranch())
		{
			return true;
		}

		LogRuntimeWarning(MissingNextWarning);
		OutResult = EDialogueExecutionResult::EndedNonCompleted;
		return false;
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

		const int32 WaitingMultiLineEntryIndex = Session.WaitingMultiLineEntryIndex;
		ClearSessionPresentationState(Session);
		if (WaitingLineNode->NodeType == EDialogueNodeType::MultiLine && WaitingMultiLineEntryIndex != INDEX_NONE)
		{
			Session.PendingMultiLineStartIndexByNode.Add(WaitingLineNode->NodeId, WaitingMultiLineEntryIndex + 1);
			Session.CurrentNodeId = WaitingLineNode->NodeId;
		}
		else
		{
			Session.PendingMultiLineStartIndexByNode.Remove(WaitingLineNode->NodeId);
			EDialogueExecutionResult RouteResult = EDialogueExecutionResult::Failed;
			if (!RouteToNextOrPending(
				WaitingLineNode->NextNodeId,
				TEXT("Line node had no Next link during advance; ending non-completed."),
				RouteResult))
			{
				return RouteResult;
			}
		}
	}

	for (int32 StepIndex = 0; StepIndex < MaxSteps; ++StepIndex)
	{
		if (!Session.CurrentNodeId.IsValid())
		{
			if (ContinuePendingSequenceBranch())
			{
				continue;
			}

			LogRuntimeWarning(TEXT("Dialogue execution reached an unlinked branch end; ending non-completed."));
			return EDialogueExecutionResult::EndedNonCompleted;
		}

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
		case EDialogueNodeType::Route:
		{
			if (!Node->NextNodeId.IsValid())
			{
				if (!ContinuePendingSequenceBranch())
				{
					LogRuntimeWarning(TEXT("Route node has no outgoing connection; ending non-completed."));
					return EDialogueExecutionResult::EndedNonCompleted;
				}
				break;
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

			const bool bShowLine = ShouldShowLineEntry(*LineData, Context);
			if (!bShowLine)
			{
				EDialogueExecutionResult RouteResult = EDialogueExecutionResult::Failed;
				if (!RouteToNextOrPending(
					Node->NextNodeId,
					TEXT("Line node skip had no Next link; ending non-completed."),
					RouteResult))
				{
					return RouteResult;
				}
				break;
			}

			return PresentLineAndWait(LineData->Line, Context, Node->NodeId, INDEX_NONE);
		}
		case EDialogueNodeType::MultiLine:
		{
			const FDialogueMultiLineNodeData* MultiLineData = Node->NodeData.GetPtr<FDialogueMultiLineNodeData>();
			if (!MultiLineData)
			{
				LogRuntimeError(TEXT("Multi-line node payload missing."));
				return EDialogueExecutionResult::Failed;
			}

			int32 StartIndex = 0;
			if (const int32* PendingStartIndex = Session.PendingMultiLineStartIndexByNode.Find(Node->NodeId))
			{
				StartIndex = FMath::Max(0, *PendingStartIndex);
			}
			Session.PendingMultiLineStartIndexByNode.Remove(Node->NodeId);

			for (int32 EntryIndex = StartIndex; EntryIndex < MultiLineData->Lines.Num(); ++EntryIndex)
			{
				const FDialogueMultiLineEntry& Entry = MultiLineData->Lines[EntryIndex];
				const bool bShowLine = ShouldShowLineEntry(Entry.LineData, Context);
				if (!bShowLine)
				{
					continue;
				}

				return PresentLineAndWait(Entry.LineData.Line, Context, Node->NodeId, EntryIndex);
			}

			EDialogueExecutionResult RouteResult = EDialogueExecutionResult::Failed;
			if (!RouteToNextOrPending(
				Node->NextNodeId,
				TEXT("Multi-line node had no remaining visible lines and no Next link; ending non-completed."),
				RouteResult))
			{
				return RouteResult;
			}
			break;
		}
		case EDialogueNodeType::SplitLine:
		{
			const FDialogueMultiLineNodeData* SplitLineData = Node->NodeData.GetPtr<FDialogueMultiLineNodeData>();
			if (!SplitLineData)
			{
				LogRuntimeError(TEXT("Split-line node payload missing."));
				return EDialogueExecutionResult::Failed;
			}

			for (const FDialogueMultiLineEntry& Entry : SplitLineData->Lines)
			{
				if (!ShouldShowLineEntry(Entry.LineData, Context))
				{
					continue;
				}

				if (!DoesSpeakerTagMatchActivePlayer(Entry.LineData.Line.SpeakerTag, Context))
				{
					continue;
				}

				return PresentLineAndWait(Entry.LineData.Line, Context, Node->NodeId, INDEX_NONE);
			}

			EDialogueExecutionResult RouteResult = EDialogueExecutionResult::Failed;
			if (!RouteToNextOrPending(
				Node->NextNodeId,
				TEXT("Split-line node found no matching line for active character and no Next link; ending non-completed."),
				RouteResult))
			{
				return RouteResult;
			}
			break;
		}
		case EDialogueNodeType::Choice:
		{
			ClearSessionPresentationState(Session);

			const FParleyPlayerIdentity OwnerIdentity = BuildOwnerIdentityForSession(Context.World, Session);
			if (Node->CompletedChoicePolicy == EDialogueCompletedChoicePolicy::LockedToRecordedChoice)
			{
				const FParleyProgressionStore* ProgressionStore = GetProgressionStore(DialogueSubsystem);
				const FDialoguePlayerPersistentState* PlayerState = FindPlayerDialogueState(ProgressionStore, OwnerIdentity, Context.World);
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
				const TArray<FGameplayTag> SlottedPlayers = GetAllControlledCharacters(Context.World);
				for (const FGameplayTag Slot : SlottedPlayers)
				{
					AddSessionParticipant(Session, SeenByPlayerTransient, Slot);
					PersistSeenCycleTagsForCharacter(DialogueSubsystem, Slot, SeenByPlayerTransient, true);
				}
			}

			if (Session.CurrentChoices.IsEmpty())
			{
				if (!Node->FallbackNodeId.IsValid())
				{
					if (!ContinuePendingSequenceBranch())
					{
						UE_LOG(ParleyLog, Error, TEXT("[Dialogue] Choice node '%s' has no valid choices and no fallback branch."), *Node->NodeId.ToString(EGuidFormats::DigitsWithHyphensInBraces));
						return EDialogueExecutionResult::EndedNonCompleted;
					}
					break;
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
				if (!ContinuePendingSequenceBranch())
				{
					LogRuntimeWarning(TEXT("Bool node branch has no connection; ending non-completed."));
					return EDialogueExecutionResult::EndedNonCompleted;
				}
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
					if (!ContinuePendingSequenceBranch())
					{
						UE_LOG(ParleyLog, Error, TEXT("[Dialogue] Switch node '%s' has no matching branch and no default output."), *Node->NodeId.ToString(EGuidFormats::DigitsWithHyphensInBraces));
						return EDialogueExecutionResult::EndedNonCompleted;
					}
					break;
				}
				Session.CurrentNodeId = Node->SwitchDefaultNodeId;
			}
			break;
		}
		case EDialogueNodeType::RouteByCharacter:
		{
			bool bMatched = false;
			for (const FDialogueCompiledCharacterRouteBranch& Branch : Node->CharacterRouteBranches)
			{
				if (!DoesSpeakerTagMatchActivePlayer(Branch.SpeakerTag, Context))
				{
					continue;
				}

				Session.CurrentNodeId = Branch.NextNodeId;
				if (!Session.CurrentNodeId.IsValid())
				{
					LogRuntimeError(TEXT("Character route branch selected but has no connection."));
					return EDialogueExecutionResult::Failed;
				}

				bMatched = true;
				break;
			}

			if (!bMatched)
			{
				if (!ContinuePendingSequenceBranch())
				{
					LogRuntimeWarning(TEXT("Character route node has no matching branch for active player and no pending sequence branch; ending non-completed."));
					return EDialogueExecutionResult::EndedNonCompleted;
				}
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
				if (!ContinuePendingSequenceBranch())
				{
					LogRuntimeWarning(TEXT("Tag mutation node has no Next link; ending non-completed."));
					return EDialogueExecutionResult::EndedNonCompleted;
				}
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
				if (!ContinuePendingSequenceBranch())
				{
					LogRuntimeWarning(TEXT("Relationship mutation node has no Next link; ending non-completed."));
					return EDialogueExecutionResult::EndedNonCompleted;
				}
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
				if (!ContinuePendingSequenceBranch())
				{
					LogRuntimeWarning(TEXT("Faction mutation node has no Next link; ending non-completed."));
					return EDialogueExecutionResult::EndedNonCompleted;
				}
			}
			break;
		}
		case EDialogueNodeType::Signal:
		{
			const FDialogueSignalNodeData* SignalData = Node->NodeData.GetPtr<FDialogueSignalNodeData>();
			if (!SignalData)
			{
				LogRuntimeError(TEXT("Signal node payload missing."));
				return EDialogueExecutionResult::Failed;
			}

			if (!bPreviewMode && SignalData->SignalTag.IsValid())
			{
				DialogueSubsystem->OnDialogueSignalFired.Broadcast(
					SignalData->SignalTag,
					SignalData->PayloadTags,
					Context.ConversationTag,
					Context.PrimarySpeakerTag,
					GetDefaultCharacterTagForSlot(Session.OwnerCharacterTag));
				for (const FGameplayTag Slot : Session.Participants)
				{
					if (!Slot.IsValid())
					{
						continue;
					}

					if (APlayerController* TargetController = FindPlayerControllerByCharacter(DialogueSubsystem->GetWorld(), Slot))
					{
						if (IParleyPlayerControllerInterface* ControllerInterface = Cast<IParleyPlayerControllerInterface>(TargetController))
						{
							ControllerInterface->NotifyDialogueSignalFired(
								SignalData->SignalTag,
								SignalData->PayloadTags,
								Context.ConversationTag,
								Context.PrimarySpeakerTag,
								GetDefaultCharacterTagForSlot(Session.OwnerCharacterTag));
						}
					}
				}
			}

			Session.CurrentNodeId = Node->NextNodeId;
			if (!Session.CurrentNodeId.IsValid())
			{
				if (!ContinuePendingSequenceBranch())
				{
					LogRuntimeWarning(TEXT("Signal node has no Next link; ending non-completed."));
					return EDialogueExecutionResult::EndedNonCompleted;
				}
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
				if (!ContinuePendingSequenceBranch())
				{
					LogRuntimeWarning(TEXT("Random node has no positive-weight branches at runtime; ending non-completed."));
					return EDialogueExecutionResult::EndedNonCompleted;
				}
				break;
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
				if (!ContinuePendingSequenceBranch())
				{
					LogRuntimeWarning(TEXT("Random node failed to route despite positive weight; ending non-completed."));
					return EDialogueExecutionResult::EndedNonCompleted;
				}
			}
			break;
		}
		case EDialogueNodeType::Sequence:
		{
			TArray<FGuid> PreviouslyPendingSequenceBranches = MoveTemp(Session.PendingSequenceBranchNodeIds);
			Session.PendingSequenceBranchNodeIds.Reset();
			for (const FDialogueCompiledSequenceBranch& Branch : Node->SequenceBranches)
			{
				Session.PendingSequenceBranchNodeIds.Add(Branch.NextNodeId);
			}
			Session.PendingSequenceBranchNodeIds.Append(PreviouslyPendingSequenceBranches);

			if (!ContinuePendingSequenceBranch())
			{
				LogRuntimeWarning(TEXT("Sequence node has no linked branch outputs; ending non-completed."));
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

	UE_LOG(ParleyLog, Error, TEXT("[Dialogue] Execution exceeded max steps for conversation '%s'."), *Session.ConversationTag.ToString());
	return EDialogueExecutionResult::Failed;
}

static bool ApplyPreviewAutoChoice(FParleyActiveDialogueSession& Session, FGuid& OutSelectedChoiceBranchId)
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

static void RemoveSessionAt(UParleyDialogueSubsystem* DialogueSubsystem, TArray<FParleyActiveDialogueSession>& Sessions, const int32 SessionIndex, const bool bCompleted = false)
{
	if (!DialogueSubsystem || !Sessions.IsValidIndex(SessionIndex))
	{
		return;
	}

	const FString SessionId = Sessions[SessionIndex].SessionId;
	FParleyActiveDialogueSession SessionSnapshot = Sessions[SessionIndex];
	const TSet<FGameplayTag> ParticipantSlots = SessionSnapshot.Participants;
	if (UWorld* World = DialogueSubsystem->GetWorld())
	{
		World->GetTimerManager().ClearTimer(SessionSnapshot.AutoAdvanceTimerHandle);
	}

	ClearDialogueEmotionOverridesForSession(SessionSnapshot, /*bResetTrackedComponents=*/ true);

	UE_LOG(ParleyLog, Verbose, TEXT("[Dialogue] Session '%s' removed (Participants=%d)."), *SessionId, ParticipantSlots.Num());
	Sessions.RemoveAtSwap(SessionIndex, 1, EAllowShrinking::No);
	RefreshBusyEmotionForSpeaker(DialogueSubsystem, SessionSnapshot.PrimarySpeakerTag, Sessions);
	ClearChoiceLookaheadPreviewForSpeaker(
		DialogueSubsystem->GetWorld(),
		SessionSnapshot.PrimarySpeakerTag,
		GetDefaultCharacterTagForSlot(SessionSnapshot.OwnerCharacterTag));
	DialogueSubsystem->OnChoiceLookaheadCleared.Broadcast(GetDefaultCharacterTagForSlot(SessionSnapshot.OwnerCharacterTag));
	DialogueSubsystem->OnDialogueSessionEnded.Broadcast(SessionId);
	DialogueSubsystem->OnConversationEnded.Broadcast(
		SessionSnapshot.ConversationTag,
		SessionSnapshot.PrimarySpeakerTag,
		GetDefaultCharacterTagForSlot(SessionSnapshot.OwnerCharacterTag),
		bCompleted);
	for (const FGameplayTag Slot : ParticipantSlots)
	{
		if (!Slot.IsValid())
		{
			continue;
		}

		if (APlayerController* TargetController = FindPlayerControllerByCharacter(DialogueSubsystem->GetWorld(), Slot))
		{
			if (IParleyPlayerControllerInterface* ControllerInterface = Cast<IParleyPlayerControllerInterface>(TargetController))
			{
				ControllerInterface->NotifyDialogueChoiceLookaheadCleared(GetDefaultCharacterTagForSlot(SessionSnapshot.OwnerCharacterTag));
				ControllerInterface->NotifyDialogueConversationEnded(
					SessionSnapshot.ConversationTag,
					SessionSnapshot.PrimarySpeakerTag,
					GetDefaultCharacterTagForSlot(SessionSnapshot.OwnerCharacterTag),
					bCompleted);
			}
		}
	}
	for (const FGameplayTag Slot : ParticipantSlots)
	{
		if (APlayerController* TargetController = FindPlayerControllerByCharacter(DialogueSubsystem->GetWorld(), Slot))
		{
			if (IParleyPlayerControllerInterface* ControllerInterface = Cast<IParleyPlayerControllerInterface>(TargetController))
			{
				ControllerInterface->NotifyDialogueSessionEnded(SessionId);
			}
		}
	}

	// Session end can change offer availability even when conversation did not persist completion.
	// Keep speaker talkable indicators in sync with current offer state.
	if (UGameInstance* GI = DialogueSubsystem->GetGameInstance())
	{
		if (UParleySpeakerSubsystem* SpeakerSubsystem = GI->GetSubsystem<UParleySpeakerSubsystem>())
		{
			SpeakerSubsystem->RefreshAllSpeakerTalkableStates();
		}
	}
}

bool UParleyDialogueSubsystem::TryStartDialogueBetweenSpeakers(
	APlayerController* RequestingController,
	FGameplayTag SourceSpeakerTag,
	FGameplayTag TargetSpeakerTag)
{
	if (!RequestingController || !TargetSpeakerTag.IsValid())
	{
		return false;
	}

	APlayerState* RequestingPlayerState = RequestingController->GetPlayerState<APlayerState>();
	if (!SourceSpeakerTag.IsValid())
	{
		UE_LOG(
			ParleyLog,
			Verbose,
			TEXT("[Dialogue] Start-with-speaker rejected: controller '%s' has no valid source speaker tag."),
			*GetNameSafe(RequestingController));
		return false;
	}
	if (RequestingPlayerState)
	{
		FParleyDialogueRuntimeState& Runtime = GetRuntimeState();
		const int32 ExistingSessionIndex = FindSessionIndexForCharacter(Runtime.ActiveSessions, GetCharacterTagFromPlayerState(RequestingPlayerState));
		if (Runtime.ActiveSessions.IsValidIndex(ExistingSessionIndex))
		{
			FParleyActiveDialogueSession& ExistingSession = Runtime.ActiveSessions[ExistingSessionIndex];
			if (!ExistingSession.ConversationAsset || !ExistingSession.CurrentNodeId.IsValid())
			{
				UE_LOG(
					ParleyLog,
					Warning,
					TEXT("[Dialogue] Removing stale session '%s' while starting speaker '%s'."),
					*ExistingSession.SessionId,
					*TargetSpeakerTag.ToString());
				RemoveSessionAt(this, Runtime.ActiveSessions, ExistingSessionIndex, false);
			}
			else if (ExistingSession.PrimarySpeakerTag.MatchesTagExact(TargetSpeakerTag))
			{
				// Re-broadcast the active speaker session so interaction can reopen the same dialogue UI.
				BroadcastSessionUpdated(this, ExistingSession);
				return true;
			}
		}
	}

	if (RequestingPlayerState)
	{
		const FGameplayTag RequesterSlot = GetCharacterTagFromPlayerState(RequestingPlayerState);
		const UParleyDialogueSettings* Settings = GetDefault<UParleyDialogueSettings>();
		const FGameplayTag ModeTag = GetCurrentModeTag(this, GetWorld());
		if (RequesterSlot.IsValid() && IsBusySpeakerLockEnabled(Settings, ModeTag))
		{
			FParleyDialogueRuntimeState& Runtime = GetRuntimeState();
			if (FParleyActiveDialogueSession* BusySession = FindPerPlayerSessionByPrimarySpeaker(Runtime.ActiveSessions, TargetSpeakerTag, RequesterSlot))
			{
				UE_LOG(
					ParleyLog,
					Verbose,
					TEXT("[Dialogue] Start-with-speaker blocked: speaker '%s' busy in session '%s' (owner=%s)."),
					*TargetSpeakerTag.ToString(),
					*BusySession->SessionId,
					LexToStringParleySlot(BusySession->OwnerCharacterTag));

				if (Settings && Settings->bAutoEavesdropOnBusySpeakerByDefault)
				{
					if (ForceEavesdrop(RequestingController, true, GetDefaultCharacterTagForSlot(BusySession->OwnerCharacterTag)))
					{
						return true;
					}
				}

				return false;
			}
		}
	}

	FDialogueConversationOffer Offer;
	if (!GetAvailableConversationForSpeakerInternal(
		RequestingController,
		TargetSpeakerTag,
		Offer,
		/*bSpeakerLocalStateAllowsDialogue=*/ true,
		SourceSpeakerTag,
		/*bPersistChanceSkipFailures=*/ true))
	{
		return false;
	}
	return StartConversationInternal(RequestingController, Offer.ConversationTag, TargetSpeakerTag, SourceSpeakerTag);
}

bool UParleyDialogueSubsystem::StartConversation(
	APlayerController* RequestingController,
	FGameplayTag ConversationTag,
	FGameplayTag PrimarySpeakerTag)
{
	return StartConversationInternal(
		RequestingController,
		ConversationTag,
		PrimarySpeakerTag,
		FGameplayTag(),
		FGameplayTag(),
		false);
}

bool UParleyDialogueSubsystem::StartConversationByTagForCharacters(
	FGameplayTag RequesterCharacterTag,
	FGameplayTag OwnerCharacterTag,
	FGameplayTag ConversationTag)
{
	UWorld* World = GetWorld();
	if (!IsAuthorityWorld_Dialogue(World))
	{
		UE_LOG(ParleyLog, Verbose, TEXT("[Dialogue] StartConversationByTagForCharacters rejected: authority required."));
		return false;
	}

	const FGameplayTag RequesterSlot = NormalizeCharacterTagForDialogue(RequesterCharacterTag);
	const FGameplayTag OwnerSlot = NormalizeCharacterTagForDialogue(OwnerCharacterTag);
	if (!RequesterSlot.IsValid() || !OwnerSlot.IsValid() || !ConversationTag.IsValid())
	{
		UE_LOG(
			ParleyLog,
			Verbose,
			TEXT("[Dialogue] StartConversationByTagForCharacters rejected: invalid tags (Requester=%s Owner=%s Conversation=%s)."),
			*RequesterCharacterTag.ToString(),
			*OwnerCharacterTag.ToString(),
			*ConversationTag.ToString());
		return false;
	}

	APlayerController* OwnerController = FindPlayerControllerByCharacter(World, OwnerSlot);
	if (!OwnerController)
	{
		UE_LOG(
			ParleyLog,
			Verbose,
			TEXT("[Dialogue] StartConversationByTagForCharacters rejected: no owner controller for slot %s."),
			LexToStringParleySlot(OwnerSlot));
		return false;
	}

	const APlayerState* RequesterPlayerState = FindPlayerStateByCharacterTag(World, RequesterSlot);
	if (!RequesterPlayerState)
	{
		UE_LOG(
			ParleyLog,
			Verbose,
			TEXT("[Dialogue] StartConversationByTagForCharacters rejected: no requester player state for slot %s."),
			LexToStringParleySlot(RequesterSlot));
		return false;
	}

	const FGameplayTag SourceSpeakerTagOverride = ResolvePlayerSpeakerTag(RequesterPlayerState);
	if (!SourceSpeakerTagOverride.IsValid())
	{
		UE_LOG(
			ParleyLog,
			Verbose,
			TEXT("[Dialogue] StartConversationByTagForCharacters rejected: requester slot %s has no resolved speaker tag."),
			LexToStringParleySlot(RequesterSlot));
		return false;
	}

	return StartConversationInternal(
		OwnerController,
		ConversationTag,
		FGameplayTag(),
		SourceSpeakerTagOverride,
		RequesterSlot,
		/*bBypassStandardStartGuards=*/ true);
}

bool UParleyDialogueSubsystem::StartConversationInternal(
	APlayerController* RequestingController,
	FGameplayTag ConversationTag,
	FGameplayTag PrimarySpeakerTag,
	const FGameplayTag SourceSpeakerTagOverride,
	const FGameplayTag InitiatorCharacterTagOverride,
	const bool bBypassStandardStartGuards)
{
	UWorld* World = GetWorld();
	if (!IsAuthorityWorld_Dialogue(World) || !RequestingController || !ConversationTag.IsValid())
	{
		UE_LOG(ParleyLog, Verbose, TEXT("[Dialogue] StartConversation rejected: invalid params or not authority (Controller=%s Tag=%s)."),
			*GetNameSafe(RequestingController),
			*ConversationTag.ToString());
		return false;
	}

	FParleyDialogueRuntimeState& Runtime = GetRuntimeState();
	UParleyConversationAsset* Conversation = Runtime.ConversationsByTag.FindRef(ConversationTag);
	if (!Conversation)
	{
		UE_LOG(ParleyLog, Warning, TEXT("[Dialogue] StartConversation failed: conversation '%s' not registered."), *ConversationTag.ToString());
		return false;
	}

	FDialogueValidationReport Validation;
	if (!ValidateConversation(Conversation, Validation))
	{
		UE_LOG(ParleyLog, Error,
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
		UE_LOG(ParleyLog, Verbose,
			TEXT("[Dialogue] StartConversation rejected: PrimarySpeaker mismatch (requested %s, asset %s)."),
			*PrimarySpeakerTag.ToString(),
			*Conversation->Header.PrimarySpeakerTag.ToString());
		return false;
	}

	APlayerState* RequesterPS = RequestingController->GetPlayerState<APlayerState>();
	if (!RequesterPS)
	{
		UE_LOG(ParleyLog, Verbose, TEXT("[Dialogue] StartConversation rejected: RequestingController has no PlayerState."));
		return false;
	}
	const FGameplayTag RequesterSlot = GetCharacterTagFromPlayerState(RequesterPS);
	if (!RequesterSlot.IsValid())
	{
		UE_LOG(ParleyLog, Verbose, TEXT("[Dialogue] StartConversation rejected: player slot is Unknown."));
		return false;
	}

	const FParleyPlayerIdentity RequesterIdentity = BuildPlayerIdentityFromState(RequesterPS);
	SyncCycleOfferStateFromProgressionStoreForCharacter(this, RequesterSlot, Runtime.SeenByPlayerTransient, Runtime.SkippedByPlayerTransient);
	SyncSpeakerOfferCountsFromProgressionStoreForCharacter(this, RequesterSlot, Runtime.SpeakerOfferCountsByPlayerTransient);
	FDialogueRuntimeContext StartContext = BuildOfferContext(this, Conversation, RequesterPS, RequesterIdentity, SourceSpeakerTagOverride);
	StartContext.bSeenByGame = Runtime.SeenByGameTransient.HasTagExact(ConversationTag);
	if (const FGameplayTagContainer* SeenTags = Runtime.SeenByPlayerTransient.Find(RequesterSlot))
	{
		StartContext.bSeenByPlayer = SeenTags->HasTagExact(ConversationTag);
	}
	const bool bSkippedThisCycle = Runtime.SkippedByPlayerTransient.FindOrAdd(RequesterSlot).HasTagExact(ConversationTag);

	if (!bBypassStandardStartGuards)
	{
		FString StartGateFailure;
		if (!EvaluateConversationOfferRules(this, StartContext, Conversation->Header, &StartGateFailure))
		{
			UE_LOG(ParleyLog, Verbose, TEXT("[Dialogue] StartConversation gated out for '%s': %s"), *ConversationTag.ToString(), *StartGateFailure);
			return false;
		}

		if (!Conversation->Header.bRepeatable && StartContext.bCompletedByPlayer)
		{
			UE_LOG(ParleyLog, Verbose, TEXT("[Dialogue] StartConversation blocked: non-repeatable already completed by player '%s'."), *ConversationTag.ToString());
			return false;
		}
		if (Conversation->Header.bCompletedByGameBlocksReoffer && StartContext.bCompletedByGame)
		{
			UE_LOG(ParleyLog, Verbose, TEXT("[Dialogue] StartConversation blocked: completed-by-game suppression for '%s'."), *ConversationTag.ToString());
			return false;
		}
		if (Conversation->Header.bSeenByGameBlocksReoffer && StartContext.bSeenByGame)
		{
			UE_LOG(ParleyLog, Verbose, TEXT("[Dialogue] StartConversation blocked: seen-by-game suppression for '%s'."), *ConversationTag.ToString());
			return false;
		}
		if (Conversation->Header.bSeenByPlayerBlocksReoffer && StartContext.bSeenByPlayer)
		{
			UE_LOG(ParleyLog, Verbose, TEXT("[Dialogue] StartConversation blocked: seen-by-player suppression for '%s'."), *ConversationTag.ToString());
			return false;
		}
		if (Conversation->Header.bBlockOfferPerCycle && (StartContext.bSeenByPlayer || bSkippedThisCycle))
		{
			UE_LOG(ParleyLog, Verbose,
				TEXT("[Dialogue] StartConversation blocked: per-cycle blocker active for '%s' (seen=%d skipped=%d)."),
				*ConversationTag.ToString(),
				StartContext.bSeenByPlayer ? 1 : 0,
				bSkippedThisCycle ? 1 : 0);
			return false;
		}

		const FParleySpeakerRow* SpeakerRow = Runtime.SpeakerRowsByTag.Find(PrimarySpeakerTag);
		const int32 MaxOffersPerCycle = SpeakerRow ? FMath::Max(0, SpeakerRow->MaxOffersPerCycle) : 0;
		const int32 ExistingOfferCount = Runtime.SpeakerOfferCountsByPlayerTransient.FindOrAdd(RequesterSlot).FindRef(PrimarySpeakerTag);
		if (MaxOffersPerCycle > 0 && ExistingOfferCount >= MaxOffersPerCycle)
		{
			UE_LOG(
				ParleyLog,
				Verbose,
				TEXT("[Dialogue] StartConversation blocked: speaker '%s' cycle offer cap reached (%d/%d) for slot %s."),
				*PrimarySpeakerTag.ToString(),
				ExistingOfferCount,
				MaxOffersPerCycle,
				LexToStringParleySlot(RequesterSlot));
			return false;
		}
	}
	else
	{
		UE_LOG(
			ParleyLog,
			Verbose,
			TEXT("[Dialogue] StartConversation bypassing standard offer guards for scripted start (Conversation=%s Owner=%s Initiator=%s)."),
			*ConversationTag.ToString(),
			*GetDefaultCharacterTagForSlot(RequesterSlot).ToString(),
			*GetDefaultCharacterTagForSlot(InitiatorCharacterTagOverride.IsValid() ? InitiatorCharacterTagOverride : RequesterSlot).ToString());
	}

	const UParleyDialogueSettings* Settings = GetDefault<UParleyDialogueSettings>();
	const FGameplayTag ModeTag = GetCurrentModeTag(this, World);
	const bool bSharedMode = Settings && IsModeInContainer(ModeTag, Settings->SharedDialogueModeTags);

	if (bSharedMode)
	{
		if (FParleyActiveDialogueSession* Existing = FindSharedSession(Runtime.ActiveSessions))
		{
			if (Existing->ConversationTag.MatchesTagExact(ConversationTag))
			{
				AddSessionParticipant(*Existing, Runtime.SeenByPlayerTransient, RequesterSlot);
				Runtime.SkippedByPlayerTransient.FindOrAdd(RequesterSlot).RemoveTag(ConversationTag);
				PersistCycleOfferStateForCharacter(this, RequesterSlot, Runtime.SeenByPlayerTransient, Runtime.SkippedByPlayerTransient, true);
				BroadcastSessionUpdated(this, *Existing);
				UE_LOG(ParleyLog, Verbose, TEXT("[Dialogue] StartConversation: player joined existing shared session for '%s'."), *ConversationTag.ToString());
				return true;
			}
			UE_LOG(ParleyLog, Verbose, TEXT("[Dialogue] StartConversation rejected: shared session already running different conversation '%s'."), *Existing->ConversationTag.ToString());
			return false;
		}
	}
	else if (IsBusySpeakerLockEnabled(Settings, ModeTag))
	{
		if (FParleyActiveDialogueSession* BusySession = FindPerPlayerSessionByPrimarySpeaker(Runtime.ActiveSessions, PrimarySpeakerTag, RequesterSlot))
		{
			UE_LOG(
				ParleyLog,
				Verbose,
				TEXT("[Dialogue] StartConversation blocked: speaker '%s' busy in session '%s' (owner=%s)."),
				*PrimarySpeakerTag.ToString(),
				*BusySession->SessionId,
				LexToStringParleySlot(BusySession->OwnerCharacterTag));

			if (Settings && Settings->bAutoEavesdropOnBusySpeakerByDefault)
			{
				if (ForceEavesdrop(RequestingController, true, GetDefaultCharacterTagForSlot(BusySession->OwnerCharacterTag)))
				{
					return true;
				}
			}

			return false;
		}
	}

	if (!bSharedMode && FindSessionByOwnerCharacter(Runtime.ActiveSessions, RequesterSlot))
	{
		UE_LOG(ParleyLog, Verbose, TEXT("[Dialogue] StartConversation rejected: slot already owns a session."));
		return false;
	}

	FParleyActiveDialogueSession Session;
	Session.SessionId = BuildSessionId();
	Session.ConversationTag = ConversationTag;
	Session.PrimarySpeakerTag = PrimarySpeakerTag;
	Session.SourceSpeakerTag = StartContext.SourceSpeakerTag;
	Session.ConversationAsset = Conversation;
	Session.CurrentNodeId = Conversation->CompiledData.EnterNodeId;
	Session.InitiatorCharacterTag = InitiatorCharacterTagOverride.IsValid()
		? InitiatorCharacterTagOverride
		: RequesterSlot;
	Session.OwnerCharacterTag = RequesterSlot;
	Session.bIsSharedSession = bSharedMode;
	Session.bConversationImportant = Conversation->Header.bImportant;
	Session.bConversationPrivate = Conversation->Header.bPrivateConversation;
	AddSessionParticipant(Session, Runtime.SeenByPlayerTransient, RequesterSlot);
	Runtime.SkippedByPlayerTransient.FindOrAdd(RequesterSlot).RemoveTag(ConversationTag);
	PersistCycleOfferStateForCharacter(this, RequesterSlot, Runtime.SeenByPlayerTransient, Runtime.SkippedByPlayerTransient, true);

	Runtime.SeenByGameTransient.AddTag(ConversationTag);

	// Important conversations force all slotted players into passive viewing.
	if (Session.bConversationImportant)
	{
		const TArray<FGameplayTag> SlottedPlayers = GetAllControlledCharacters(World);
		for (const FGameplayTag Slot : SlottedPlayers)
		{
			AddSessionParticipant(Session, Runtime.SeenByPlayerTransient, Slot);
			Runtime.SkippedByPlayerTransient.FindOrAdd(Slot).RemoveTag(ConversationTag);
			PersistCycleOfferStateForCharacter(this, Slot, Runtime.SeenByPlayerTransient, Runtime.SkippedByPlayerTransient, true);
		}
	}

	Runtime.ActiveSessions.Add(MoveTemp(Session));
	Runtime.SpeakerOfferCountsByPlayerTransient.FindOrAdd(RequesterSlot).FindOrAdd(PrimarySpeakerTag) += 1;
	PersistSpeakerOfferCountsForCharacter(this, RequesterSlot, Runtime.SpeakerOfferCountsByPlayerTransient, true);
	OnConversationStarted.Broadcast(
		ConversationTag,
		PrimarySpeakerTag,
		GetDefaultCharacterTagForSlot(RequesterSlot));
	const int32 NewSessionIndex = Runtime.ActiveSessions.Num() - 1;
	for (const FGameplayTag Slot : Runtime.ActiveSessions[NewSessionIndex].Participants)
	{
		if (!Slot.IsValid())
		{
			continue;
		}

		if (APlayerController* TargetController = FindPlayerControllerByCharacter(World, Slot))
		{
			if (IParleyPlayerControllerInterface* ControllerInterface = Cast<IParleyPlayerControllerInterface>(TargetController))
			{
				ControllerInterface->NotifyDialogueConversationStarted(
					ConversationTag,
					PrimarySpeakerTag,
					GetDefaultCharacterTagForSlot(RequesterSlot));
			}
		}
	}
	RefreshBusyEmotionForSpeaker(this, PrimarySpeakerTag, Runtime.ActiveSessions);
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
		UE_LOG(ParleyLog, Verbose, TEXT("[Dialogue] StartConversation: session '%s' started for '%s' (slot %s)."),
			*Runtime.ActiveSessions[NewSessionIndex].SessionId,
			*ConversationTag.ToString(),
			LexToStringParleySlot(RequesterSlot));
		return true;
	}

	UE_LOG(ParleyLog, Verbose, TEXT("[Dialogue] StartConversation auto-ended during initial execution for '%s' (Result=%d)."),
		*ConversationTag.ToString(),
		static_cast<int32>(Result));
	RemoveSessionAt(this, Runtime.ActiveSessions, NewSessionIndex, Result == EDialogueExecutionResult::EndedCompleted);
	return Result == EDialogueExecutionResult::EndedCompleted || Result == EDialogueExecutionResult::EndedNonCompleted;
}

bool UParleyDialogueSubsystem::AdvanceConversation(APlayerController* RequestingController)
{
	UWorld* World = GetWorld();
	if (!IsAuthorityWorld_Dialogue(World) || !RequestingController)
	{
		UE_LOG(ParleyLog, Verbose, TEXT("[Dialogue] Advance rejected: not authority or invalid controller."));
		return false;
	}

	APlayerState* RequesterPS = RequestingController->GetPlayerState<APlayerState>();
	if (!RequesterPS)
	{
		UE_LOG(ParleyLog, Verbose, TEXT("[Dialogue] Advance rejected: controller has no PlayerState."));
		return false;
	}

	FParleyDialogueRuntimeState& Runtime = GetRuntimeState();
	const int32 SessionIndex = FindSessionIndexForCharacter(Runtime.ActiveSessions, GetCharacterTagFromPlayerState(RequesterPS));
	if (!Runtime.ActiveSessions.IsValidIndex(SessionIndex))
	{
		UE_LOG(ParleyLog, Verbose, TEXT("[Dialogue] Advance rejected: no active session for slot %s."),
			LexToStringParleySlot(GetCharacterTagFromPlayerState(RequesterPS)));
		return false;
	}

	FParleyActiveDialogueSession& Session = Runtime.ActiveSessions[SessionIndex];
	if (GetCharacterTagFromPlayerState(RequesterPS) != Session.OwnerCharacterTag)
	{
		UE_LOG(ParleyLog, Verbose, TEXT("[Dialogue] Advance rejected: slot %s is not the owner of session '%s'."),
			LexToStringParleySlot(GetCharacterTagFromPlayerState(RequesterPS)),
			*Session.SessionId);
		return false;
	}

	if (Session.bWaitingForChoice)
	{
		UE_LOG(ParleyLog, Verbose, TEXT("[Dialogue] Advance rejected: session '%s' waiting for choice."), *Session.SessionId);
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

	UE_LOG(ParleyLog, Verbose, TEXT("[Dialogue] Session '%s' ended on advance (Result=%d)."),
		*Session.SessionId,
		static_cast<int32>(Result));
	RemoveSessionAt(this, Runtime.ActiveSessions, SessionIndex, Result == EDialogueExecutionResult::EndedCompleted);
	return Result == EDialogueExecutionResult::EndedCompleted || Result == EDialogueExecutionResult::EndedNonCompleted;
}

bool UParleyDialogueSubsystem::SubmitChoice(APlayerController* RequestingController, FGuid ChoiceBranchId)
{
	UWorld* World = GetWorld();
	if (!IsAuthorityWorld_Dialogue(World) || !RequestingController || !ChoiceBranchId.IsValid())
	{
		UE_LOG(ParleyLog, Verbose, TEXT("[Dialogue] SubmitChoice rejected: invalid params or not authority (Controller=%s Branch=%s)."),
			*GetNameSafe(RequestingController),
			*ChoiceBranchId.ToString(EGuidFormats::DigitsWithHyphensInBraces));
		return false;
	}

	APlayerState* RequesterPS = RequestingController->GetPlayerState<APlayerState>();
	if (!RequesterPS)
	{
		UE_LOG(ParleyLog, Verbose, TEXT("[Dialogue] SubmitChoice rejected: controller has no PlayerState."));
		return false;
	}

	FParleyDialogueRuntimeState& Runtime = GetRuntimeState();
	const int32 SessionIndex = FindSessionIndexForCharacter(Runtime.ActiveSessions, GetCharacterTagFromPlayerState(RequesterPS));
	if (!Runtime.ActiveSessions.IsValidIndex(SessionIndex))
	{
		UE_LOG(ParleyLog, Verbose, TEXT("[Dialogue] SubmitChoice rejected: no active session for slot %s."),
			LexToStringParleySlot(GetCharacterTagFromPlayerState(RequesterPS)));
		return false;
	}

	FParleyActiveDialogueSession& Session = Runtime.ActiveSessions[SessionIndex];
	if (!Session.bWaitingForChoice || !Session.WaitingChoiceNodeId.IsValid())
	{
		UE_LOG(ParleyLog, Verbose, TEXT("[Dialogue] SubmitChoice rejected: session '%s' not waiting for choice."), *Session.SessionId);
		return false;
	}

	if (GetCharacterTagFromPlayerState(RequesterPS) != Session.OwnerCharacterTag)
	{
		UE_LOG(ParleyLog, Verbose, TEXT("[Dialogue] SubmitChoice rejected: slot %s is not the owner of session '%s'."),
			LexToStringParleySlot(GetCharacterTagFromPlayerState(RequesterPS)),
			*Session.SessionId);
		return false;
	}

	const FDialogueCompiledNode* ChoiceNode = FindNodeById(Session, Session.WaitingChoiceNodeId);
	if (!ChoiceNode)
	{
		UE_LOG(ParleyLog, Error, TEXT("[Dialogue] SubmitChoice failed: missing choice node '%s' in session '%s'."),
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
		UE_LOG(ParleyLog, Warning, TEXT("[Dialogue] SubmitChoice rejected: branch '%s' not found or unlinked in session '%s'."),
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
		UE_LOG(ParleyLog, Verbose, TEXT("[Dialogue] SubmitChoice rejected: branch '%s' not visible to player."), *ChoiceBranchId.ToString(EGuidFormats::DigitsWithHyphensInBraces));
		return false;
	}

	if (SelectedBranch->bImportant)
	{
		OnImportantChoiceMade.Broadcast(
			ChoiceBranchId,
			Session.ConversationTag,
			Session.PrimarySpeakerTag,
			GetDefaultCharacterTagForSlot(Session.OwnerCharacterTag));
		for (const FGameplayTag Slot : Session.Participants)
		{
			if (!Slot.IsValid())
			{
				continue;
			}

			if (APlayerController* TargetController = FindPlayerControllerByCharacter(World, Slot))
			{
				if (IParleyPlayerControllerInterface* ControllerInterface = Cast<IParleyPlayerControllerInterface>(TargetController))
				{
					ControllerInterface->NotifyDialogueImportantChoiceMade(
						ChoiceBranchId,
						Session.ConversationTag,
						Session.PrimarySpeakerTag,
						GetDefaultCharacterTagForSlot(Session.OwnerCharacterTag));
				}
			}
		}
	}

	ClearChoiceLookaheadPreviewForSpeaker(
		World,
		Session.PrimarySpeakerTag,
		GetDefaultCharacterTagForSlot(Session.OwnerCharacterTag));
	OnChoiceLookaheadCleared.Broadcast(GetDefaultCharacterTagForSlot(Session.OwnerCharacterTag));
	for (const FGameplayTag Slot : Session.Participants)
	{
		if (!Slot.IsValid())
		{
			continue;
		}

		if (APlayerController* TargetController = FindPlayerControllerByCharacter(World, Slot))
		{
			if (IParleyPlayerControllerInterface* ControllerInterface = Cast<IParleyPlayerControllerInterface>(TargetController))
			{
				ControllerInterface->NotifyDialogueChoiceLookaheadCleared(GetDefaultCharacterTagForSlot(Session.OwnerCharacterTag));
			}
		}
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

	UE_LOG(ParleyLog, Verbose, TEXT("[Dialogue] Session '%s' ended after choice (Result=%d)."),
		*Session.SessionId,
		static_cast<int32>(Result));
	RemoveSessionAt(this, Runtime.ActiveSessions, SessionIndex, Result == EDialogueExecutionResult::EndedCompleted);
	return Result == EDialogueExecutionResult::EndedCompleted || Result == EDialogueExecutionResult::EndedNonCompleted;
}

bool UParleyDialogueSubsystem::HighlightDialogueChoice(APlayerController* RequestingController, FGuid ChoiceBranchId)
{
	UWorld* World = GetWorld();
	if (!IsAuthorityWorld_Dialogue(World) || !RequestingController)
	{
		return false;
	}

	APlayerState* RequesterPS = RequestingController->GetPlayerState<APlayerState>();
	if (!RequesterPS)
	{
		return false;
	}

	const FGameplayTag ViewerSlot = GetCharacterTagFromPlayerState(RequesterPS);
	if (!ViewerSlot.IsValid())
	{
		return false;
	}

	const FGameplayTag ViewerSlotTag = GetDefaultCharacterTagForSlot(ViewerSlot);
	IParleyPlayerControllerInterface* RequestingControllerInterface = Cast<IParleyPlayerControllerInterface>(RequestingController);
	if (!ChoiceBranchId.IsValid())
	{
		const int32 SessionIndex = FindSessionIndexForCharacter(GetRuntimeState().ActiveSessions, ViewerSlot);
		if (GetRuntimeState().ActiveSessions.IsValidIndex(SessionIndex))
		{
			const FParleyActiveDialogueSession& Session = GetRuntimeState().ActiveSessions[SessionIndex];
			ClearChoiceLookaheadPreviewForSpeaker(World, Session.PrimarySpeakerTag, ViewerSlotTag);
		}
		OnChoiceLookaheadCleared.Broadcast(ViewerSlotTag);
		if (RequestingControllerInterface)
		{
			RequestingControllerInterface->NotifyDialogueChoiceLookaheadCleared(ViewerSlotTag);
		}
		return true;
	}

	FParleyDialogueRuntimeState& Runtime = GetRuntimeState();
	const int32 SessionIndex = FindSessionIndexForCharacter(Runtime.ActiveSessions, ViewerSlot);
	if (!Runtime.ActiveSessions.IsValidIndex(SessionIndex))
	{
		return false;
	}

	const FParleyActiveDialogueSession& Session = Runtime.ActiveSessions[SessionIndex];
	if (Session.OwnerCharacterTag != ViewerSlot)
	{
		return false;
	}

	if (!Session.bWaitingForChoice || !Session.WaitingChoiceNodeId.IsValid())
	{
		return false;
	}

	const FDialogueCompiledNode* ChoiceNode = FindNodeById(Session, Session.WaitingChoiceNodeId);
	if (!ChoiceNode)
	{
		BroadcastChoiceLookaheadPreviewForSpeaker(
			World,
			Session.PrimarySpeakerTag,
			ViewerSlotTag,
			ChoiceBranchId,
			FGameplayTag());
		OnChoiceLookaheadEmotion.Broadcast(Session.PrimarySpeakerTag, FGameplayTag(), ChoiceBranchId);
		if (RequestingControllerInterface)
		{
			RequestingControllerInterface->NotifyDialogueChoiceLookaheadEmotion(Session.PrimarySpeakerTag, FGameplayTag(), ChoiceBranchId);
		}
		return false;
	}

	const FDialogueCompiledChoiceBranch* HighlightedBranch = ChoiceNode->ChoiceBranches.FindByPredicate(
		[&ChoiceBranchId](const FDialogueCompiledChoiceBranch& Branch)
		{
			return Branch.ChoiceBranchId == ChoiceBranchId;
		});
	if (!HighlightedBranch || !HighlightedBranch->NextNodeId.IsValid())
	{
		BroadcastChoiceLookaheadPreviewForSpeaker(
			World,
			Session.PrimarySpeakerTag,
			ViewerSlotTag,
			ChoiceBranchId,
			FGameplayTag());
		OnChoiceLookaheadEmotion.Broadcast(Session.PrimarySpeakerTag, FGameplayTag(), ChoiceBranchId);
		if (RequestingControllerInterface)
		{
			RequestingControllerInterface->NotifyDialogueChoiceLookaheadEmotion(Session.PrimarySpeakerTag, FGameplayTag(), ChoiceBranchId);
		}
		return false;
	}

	FGameplayTag PreviewEmotionTag;
	const bool bResolvedPreview = TryResolveChoiceLookaheadEmotion(
		this,
		Session,
		Runtime.SeenByPlayerTransient,
		Runtime.SeenByGameTransient,
		HighlightedBranch->NextNodeId,
		PreviewEmotionTag);
	BroadcastChoiceLookaheadPreviewForSpeaker(
		World,
		Session.PrimarySpeakerTag,
		ViewerSlotTag,
		ChoiceBranchId,
		bResolvedPreview ? PreviewEmotionTag : FGameplayTag());
	OnChoiceLookaheadEmotion.Broadcast(
		Session.PrimarySpeakerTag,
		bResolvedPreview ? PreviewEmotionTag : FGameplayTag(),
		ChoiceBranchId);
	if (RequestingControllerInterface)
	{
		RequestingControllerInterface->NotifyDialogueChoiceLookaheadEmotion(
			Session.PrimarySpeakerTag,
			bResolvedPreview ? PreviewEmotionTag : FGameplayTag(),
			ChoiceBranchId);
	}
	return true;
}

bool UParleyDialogueSubsystem::ForceEavesdrop(APlayerController* RequestingController, bool bEnable, FGameplayTag TargetCharacterTag)
{
	UWorld* World = GetWorld();
	if (!IsAuthorityWorld_Dialogue(World) || !RequestingController)
	{
		UE_LOG(ParleyLog, Verbose, TEXT("[Dialogue] ForceEavesdrop rejected: not authority or invalid controller."));
		return false;
	}

	APlayerState* RequesterPS = RequestingController->GetPlayerState<APlayerState>();
	if (!RequesterPS)
	{
		UE_LOG(ParleyLog, Verbose, TEXT("[Dialogue] ForceEavesdrop rejected: controller has no PlayerState."));
		return false;
	}

	const FGameplayTag ViewerSlot = GetCharacterTagFromPlayerState(RequesterPS);
	if (!ViewerSlot.IsValid())
	{
		UE_LOG(ParleyLog, Verbose, TEXT("[Dialogue] ForceEavesdrop rejected: viewer slot is Unknown."));
		return false;
	}

	const FGameplayTag TargetSlot = NormalizeCharacterTagForDialogue(TargetCharacterTag);

	FParleyDialogueRuntimeState& Runtime = GetRuntimeState();
	if (bEnable)
	{
		if (!TargetSlot.IsValid() || TargetSlot == ViewerSlot)
		{
			UE_LOG(ParleyLog, Verbose, TEXT("[Dialogue] ForceEavesdrop rejected: target slot invalid (Viewer=%s Target=%s)."),
				LexToStringParleySlot(ViewerSlot),
				LexToStringParleySlot(TargetSlot));
			return false;
		}

		FParleyActiveDialogueSession* TargetSession = FindSessionByOwnerCharacter(Runtime.ActiveSessions, TargetSlot);
		if (!TargetSession)
		{
			UE_LOG(ParleyLog, Verbose, TEXT("[Dialogue] ForceEavesdrop rejected: target slot %s has no active dialogue session."),
				LexToStringParleySlot(TargetSlot));
			return false;
		}
		if (DoesSessionRejectEavesdrop(*TargetSession))
		{
			UE_LOG(
				ParleyLog,
				Verbose,
				TEXT("[Dialogue] ForceEavesdrop rejected: target session '%s' is private (owner=%s)."),
				*TargetSession->SessionId,
				LexToStringParleySlot(TargetSlot));
			return false;
		}

		Runtime.EavesdropTargetByViewer.Add(ViewerSlot, TargetSlot);
		AddSessionParticipant(*TargetSession, Runtime.SeenByPlayerTransient, ViewerSlot);
		PersistCycleOfferStateForCharacter(this, ViewerSlot, Runtime.SeenByPlayerTransient, Runtime.SkippedByPlayerTransient, true);
		BroadcastSessionUpdated(this, *TargetSession);

		UE_LOG(ParleyLog, Verbose, TEXT("[Dialogue] ForceEavesdrop: viewer %s now eavesdropping owner %s."),
			LexToStringParleySlot(ViewerSlot),
			LexToStringParleySlot(TargetSlot));
		return true;
	}

	Runtime.EavesdropTargetByViewer.Remove(ViewerSlot);
	APlayerController* ViewerController = FindPlayerControllerByCharacter(World, ViewerSlot);
	for (FParleyActiveDialogueSession& Session : Runtime.ActiveSessions)
	{
		if (!Session.bIsSharedSession && Session.OwnerCharacterTag != ViewerSlot && !Session.bChoiceRequiresAllViewers)
		{
			if (Session.Participants.Remove(ViewerSlot) > 0)
			{
				const FString RemovedSessionId = Session.SessionId;
				BroadcastSessionUpdated(this, Session);
				if (ViewerController)
				{
					// Viewer was removed before broadcast, so explicitly clear stale local UI/cache.
					if (IParleyPlayerControllerInterface* ControllerInterface = Cast<IParleyPlayerControllerInterface>(ViewerController))
					{
						ControllerInterface->NotifyDialogueSessionEnded(RemovedSessionId);
					}
				}
			}
		}
	}
	UE_LOG(ParleyLog, Verbose, TEXT("[Dialogue] ForceEavesdrop cleared for viewer %s."), LexToStringParleySlot(ViewerSlot));
	return true;
}
