// Split from ParleyDialogueSubsystem.cpp for maintainability.

FGameplayTagContainer UParleyDialogueSubsystem::GetCombinedDialogueTags(const FGameplayTagContainer& PlayerOnlyProgressionTags, const FGameplayTagContainer& GameOnlyProgressionTags) const
{
	FGameplayTagContainer Combined = PlayerOnlyProgressionTags;
	for (const FGameplayTag Tag : GameOnlyProgressionTags)
	{
		Combined.AddTag(Tag);
	}
	return Combined;
}

bool UParleyDialogueSubsystem::EvaluateDialogueCondition(const FDialogueCondition& Condition, const FDialogueRuntimeContext& Context) const
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
	{
		FGameplayTagContainer ActiveCharacterTags;
		if (Context.ResolvedPlayerSpeakerTag.IsValid())
		{
			ActiveCharacterTags.AddTag(Context.ResolvedPlayerSpeakerTag);
		}
		return EvaluateTagContainerCondition(ActiveCharacterTags, Condition);
	}
	case EDialogueConditionSource::RelationshipPoints:
	{
		const FGameplayTag DefaultSourceSpeakerTag = Context.SourceSpeakerTag.IsValid()
			? Context.SourceSpeakerTag
			: Context.ResolvedPlayerSpeakerTag;
		const FGameplayTag SourceSpeakerTag = ResolveSpeakerTagForContext(
			FGameplayTag(),
			Context,
			DefaultSourceSpeakerTag);
		const FGameplayTag TargetSpeakerTag = ResolveSpeakerTagForContext(
			Condition.TagValue,
			Context,
			Context.PrimarySpeakerTag);
		if (!SourceSpeakerTag.IsValid() || !TargetSpeakerTag.IsValid())
		{
			return false;
		}

		return CompareNumeric(GetRelationshipPointsForSpeakerPair(SourceSpeakerTag, TargetSpeakerTag), Condition.Operator, Condition.NumericValue);
	}
	case EDialogueConditionSource::RelationshipLevel:
	{
		const FGameplayTag DefaultSourceSpeakerTag = Context.SourceSpeakerTag.IsValid()
			? Context.SourceSpeakerTag
			: Context.ResolvedPlayerSpeakerTag;
		const FGameplayTag SourceSpeakerTag = ResolveSpeakerTagForContext(
			FGameplayTag(),
			Context,
			DefaultSourceSpeakerTag);
		const FGameplayTag TargetSpeakerTag = ResolveSpeakerTagForContext(
			Condition.TagValue,
			Context,
			Context.PrimarySpeakerTag);
		if (!SourceSpeakerTag.IsValid() || !TargetSpeakerTag.IsValid())
		{
			return false;
		}

		return CompareNumeric(static_cast<float>(GetRelationshipLevelForSpeakerPair(SourceSpeakerTag, TargetSpeakerTag)), Condition.Operator, Condition.NumericValue);
	}
	case EDialogueConditionSource::FactionPopularity:
	{
		if (!Condition.TagValue.IsValid())
		{
			return false;
		}

		if (UGameInstance* GI = GetGameInstance())
		{
			if (UParleyFactionSubsystem* FactionSubsystem = GI->GetSubsystem<UParleyFactionSubsystem>())
			{
				const float Popularity = FactionSubsystem->GetFactionPopularity(Condition.TagValue);
				return CompareNumeric(Popularity, Condition.Operator, Condition.NumericValue);
			}
		}

		return false;
	}
	case EDialogueConditionSource::FactionSpeakerReputation:
	{
		if (!Condition.TagValue.IsValid())
		{
			return false;
		}

		const FGameplayTag SpeakerTag = ResolveSpeakerTagForContext(
			Condition.SecondaryTagValue,
			Context,
			Context.PrimarySpeakerTag);
		if (!SpeakerTag.IsValid())
		{
			return false;
		}

		if (UGameInstance* GI = GetGameInstance())
		{
			if (UParleyFactionSubsystem* FactionSubsystem = GI->GetSubsystem<UParleyFactionSubsystem>())
			{
				const float Reputation = FactionSubsystem->GetFactionSpeakerReputation(Condition.TagValue, SpeakerTag);
				return CompareNumeric(Reputation, Condition.Operator, Condition.NumericValue);
			}
		}

		return false;
	}
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

bool UParleyDialogueSubsystem::ApplyDialogueTagMutation(const FDialogueTagMutation& Mutation, const FDialogueRuntimeContext& Context)
{
	if (!Mutation.Tag.IsValid())
	{
		UE_LOG(ParleyLog, Verbose, TEXT("[Dialogue] Tag mutation skipped: invalid tag."));
		return false;
	}

	switch (Mutation.Target)
	{
	case EDialogueTagMutationTarget::ActivePlayerTransientConversation:
	{
		const APlayerState* ActivePS = Cast<APlayerState>(Context.ActivePlayerState);
		if (!ActivePS)
		{
			UE_LOG(ParleyLog, Verbose, TEXT("[Dialogue] Tag mutation skipped: no active player state for transient mutation."));
			return false;
		}

		FParleyDialogueRuntimeState& Runtime = GetRuntimeState();
		FParleyActiveDialogueSession* ActiveSession = FindSessionForSlot(Runtime.ActiveSessions, GetPlayerSlotFromPlayerState(ActivePS));
		if (!ActiveSession)
		{
			UE_LOG(ParleyLog, Verbose, TEXT("[Dialogue] Tag mutation skipped: no active session for transient mutation."));
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
		UE_LOG(ParleyLog, Warning, TEXT("[Dialogue] Tag mutation skipped: unsupported target %d."), static_cast<int32>(Mutation.Target));
		return false;
	}

	FParleyProgressionStore* ProgressionStore = GetProgressionStore(this);
	if (!ProgressionStore)
	{
		UE_LOG(ParleyLog, Warning, TEXT("[Dialogue] Tag mutation failed: runtime progression state unavailable."));
		return false;
	}

	switch (Mutation.Target)
	{
	case EDialogueTagMutationTarget::GameStateProgression:
	{
		const bool bAdded = Mutation.Operation == EDialogueTagMutationOp::Add;
		const bool bChanged = bAdded
			? !ProgressionStore->ProgressionTags.HasTagExact(Mutation.Tag)
			: ProgressionStore->ProgressionTags.HasTagExact(Mutation.Tag);
		if (!bChanged)
		{
			return false;
		}

		if (bAdded)
		{
			ProgressionStore->ProgressionTags.AddTag(Mutation.Tag);
		}
		else
		{
			ProgressionStore->ProgressionTags.RemoveTag(Mutation.Tag);
		}

		OnProgressionTagMutated.Broadcast(Mutation.Tag, bAdded, FGameplayTag());
		return true;
	}
	case EDialogueTagMutationTarget::ActivePlayerProgression:
	{
		const APlayerState* ActivePS = Cast<APlayerState>(Context.ActivePlayerState);
		if (!ActivePS)
		{
			UE_LOG(ParleyLog, Verbose, TEXT("[Dialogue] Tag mutation skipped: no active player state for progression mutation."));
			return false;
		}

		if (FDialoguePlayerPersistentState* PlayerState = FindOrAddPlayerDialogueState(ProgressionStore, BuildPlayerIdentityFromState(ActivePS)))
		{
			const bool bAdded = Mutation.Operation == EDialogueTagMutationOp::Add;
			const bool bChanged = bAdded
				? !PlayerState->ProgressionTags.HasTagExact(Mutation.Tag)
				: PlayerState->ProgressionTags.HasTagExact(Mutation.Tag);
			if (!bChanged)
			{
				return false;
			}

			if (bAdded)
			{
				PlayerState->ProgressionTags.AddTag(Mutation.Tag);
			}
			else
			{
				PlayerState->ProgressionTags.RemoveTag(Mutation.Tag);
			}

			OnProgressionTagMutated.Broadcast(
				Mutation.Tag,
				bAdded,
				ParleyPlayerSlot::SlotToTag(GetPlayerSlotFromPlayerState(ActivePS)));
			return true;
		}
		return false;
	}
	default:
		return false;
	}
}

bool UParleyDialogueSubsystem::ApplyDialogueRelationshipMutation(const FDialogueRelationshipMutationNodeData& Mutation, const FDialogueRuntimeContext& Context)
{
	FParleyProgressionStore* ProgressionStore = GetProgressionStore(this);
	if (!ProgressionStore)
	{
		UE_LOG(ParleyLog, Warning, TEXT("[Dialogue] Relationship mutation failed: runtime progression state unavailable."));
		return false;
	}

	const FGameplayTag DefaultSourceSpeakerTag = Context.SourceSpeakerTag.IsValid()
		? Context.SourceSpeakerTag
		: Context.ResolvedPlayerSpeakerTag;
	const FGameplayTag SourceSpeakerTag = ResolveSpeakerTagForContext(
		Mutation.SourceSpeakerTag,
		Context,
		DefaultSourceSpeakerTag);
	const FGameplayTag TargetSpeakerTag = ResolveSpeakerTagForContext(
		Mutation.TargetSpeakerTag,
		Context,
		Context.PrimarySpeakerTag);
	if (!SourceSpeakerTag.IsValid() || !TargetSpeakerTag.IsValid())
	{
		UE_LOG(ParleyLog, Verbose, TEXT("[Dialogue] Relationship mutation skipped: source/target speaker tag invalid."));
		return false;
	}

	FDialogueSpeakerRelationshipState* RelationshipState = nullptr;
	for (FDialogueSpeakerRelationshipState& Entry : ProgressionStore->DialogueSpeakerRelationshipStates)
	{
		if (Entry.SourceSpeakerTag.MatchesTagExact(SourceSpeakerTag)
			&& Entry.TargetSpeakerTag.MatchesTagExact(TargetSpeakerTag))
		{
			RelationshipState = &Entry;
			break;
		}
	}
	if (!RelationshipState)
	{
		FDialogueSpeakerRelationshipState& Added = ProgressionStore->DialogueSpeakerRelationshipStates.AddDefaulted_GetRef();
		Added.SourceSpeakerTag = SourceSpeakerTag;
		Added.TargetSpeakerTag = TargetSpeakerTag;
		RelationshipState = &Added;
	}

	const int32 OldLevel = GetRelationshipLevelForSpeakerPair(SourceSpeakerTag, TargetSpeakerTag);
	RelationshipState->RelationshipPoints += Mutation.DeltaPoints;
	const int32 NewLevel = GetRelationshipLevelForSpeakerPair(SourceSpeakerTag, TargetSpeakerTag);
	EParleyPlayerSlot PlayerSlot = EParleyPlayerSlot::Unknown;
	if (const APlayerState* ActivePS = Cast<APlayerState>(Context.ActivePlayerState))
	{
		PlayerSlot = GetPlayerSlotFromPlayerState(ActivePS);
	}
	const FGameplayTag PlayerSlotTag = ParleyPlayerSlot::SlotToTag(PlayerSlot);

	OnSpeakerRelationshipChanged.Broadcast(
		SourceSpeakerTag,
		TargetSpeakerTag,
		PlayerSlotTag,
		Mutation.DeltaPoints,
		RelationshipState->RelationshipPoints);

	OnRelationshipChanged.Broadcast(
		TargetSpeakerTag,
		PlayerSlotTag,
		Mutation.DeltaPoints,
		RelationshipState->RelationshipPoints);

	if (OldLevel != NewLevel)
	{
		OnSpeakerRelationshipLevelChanged.Broadcast(
			SourceSpeakerTag,
			TargetSpeakerTag,
			PlayerSlotTag,
			OldLevel,
			NewLevel,
			RelationshipState->RelationshipPoints);
		OnRelationshipLevelChanged.Broadcast(
			TargetSpeakerTag,
			PlayerSlotTag,
			OldLevel,
			NewLevel);
	}
	return true;
}

bool UParleyDialogueSubsystem::ApplyDialogueFactionMutation(const FDialogueFactionMutationNodeData& Mutation, const FDialogueRuntimeContext& Context)
{
	if (!Mutation.FactionTag.IsValid())
	{
		UE_LOG(ParleyLog, Verbose, TEXT("[Dialogue] Faction mutation skipped: invalid faction tag."));
		return false;
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UParleyFactionSubsystem* FactionSubsystem = GI->GetSubsystem<UParleyFactionSubsystem>())
		{
			bool bAppliedAny = false;
			if (!FMath::IsNearlyZero(Mutation.DeltaPopularity))
			{
				bAppliedAny |= FactionSubsystem->ModifyFactionPopularity(Mutation.FactionTag, Mutation.DeltaPopularity);
			}

			if (!FMath::IsNearlyZero(Mutation.DeltaSpeakerReputation))
			{
				const FGameplayTag TargetSpeakerTag = ResolveSpeakerTagForContext(
					Mutation.TargetSpeakerTag,
					Context,
					Context.PrimarySpeakerTag);

				if (TargetSpeakerTag.IsValid())
				{
					bAppliedAny |= FactionSubsystem->ModifyFactionSpeakerReputation(
						Mutation.FactionTag,
						TargetSpeakerTag,
						Mutation.DeltaSpeakerReputation);
				}
				else
				{
					UE_LOG(ParleyLog, Verbose, TEXT("[Dialogue] Faction speaker reputation mutation skipped: no target speaker tag was resolved."));
				}
			}

			return bAppliedAny || (FMath::IsNearlyZero(Mutation.DeltaPopularity) && FMath::IsNearlyZero(Mutation.DeltaSpeakerReputation));
		}
	}
	UE_LOG(ParleyLog, Warning, TEXT("[Dialogue] Faction mutation failed: faction subsystem unavailable."));
	return false;
}

bool UParleyDialogueSubsystem::ApplyRamenServeOutcome(
	const FGameplayTag SpeakerTag,
	const int32 RelationshipDeltaPoints,
	const FGameplayTag ReactionEmotionTag,
	AActor* PreferredSpeakerActor)
{
	const bool bValidSpeaker = SpeakerTag.IsValid();
	if (!bValidSpeaker)
	{
		return false;
	}

	bool bApplied = false;

	if (RelationshipDeltaPoints != 0)
	{
		FDialogueRelationshipMutationNodeData RelationshipMutation;
		RelationshipMutation.TargetSpeakerTag = SpeakerTag;
		RelationshipMutation.DeltaPoints = static_cast<float>(RelationshipDeltaPoints);

		FDialogueRuntimeContext Context;
		Context.PrimarySpeakerTag = SpeakerTag;
		Context.PrimarySpeakerActor = PreferredSpeakerActor;
		Context.ResolvedPlayerSpeakerTag = GetDialogueSpeakerPlayerPlaceholderTag();
		Context.SourceSpeakerTag = Context.ResolvedPlayerSpeakerTag;
		Context.World = GetWorld();
		bApplied |= ApplyDialogueRelationshipMutation(RelationshipMutation, Context);
	}

	if (ReactionEmotionTag.IsValid())
	{
		auto MatchesSpeakerTag = [&SpeakerTag](const FGameplayTag CandidateTag) -> bool
		{
			return CandidateTag.IsValid()
				&& (SpeakerTag.MatchesTag(CandidateTag) || CandidateTag.MatchesTag(SpeakerTag));
		};

		UParleySpeakerComponent* TargetSpeakerComponent = nullptr;
		if (PreferredSpeakerActor)
		{
			if (UParleySpeakerComponent* PreferredSpeaker = PreferredSpeakerActor->FindComponentByClass<UParleySpeakerComponent>())
			{
				TargetSpeakerComponent = PreferredSpeaker;
			}
		}

		UWorld* World = GetWorld();
		if (!TargetSpeakerComponent && World)
		{
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				AActor* Actor = *It;
				if (!Actor)
				{
					continue;
				}

				if (const UParleySpeakerComponent* TalkComponent = Actor->FindComponentByClass<UParleySpeakerComponent>())
				{
					if (MatchesSpeakerTag(TalkComponent->GetSpeakerTag()))
					{
						TargetSpeakerComponent = const_cast<UParleySpeakerComponent*>(TalkComponent);
						break;
					}
				}
			}
		}

		if (TargetSpeakerComponent)
		{
			TargetSpeakerComponent->OnSpeakerEmotionRequested.Broadcast(ReactionEmotionTag, FGameplayTag(), false);
			bApplied = true;
		}
	}

	return bApplied;
}

