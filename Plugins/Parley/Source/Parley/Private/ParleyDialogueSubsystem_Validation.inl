// Split from ParleyDialogueSubsystem.cpp for maintainability.

bool UParleyDialogueSubsystem::ValidateSpeaker(const FARDialogueSpeakerRow& SpeakerRow, FDialogueValidationReport& OutReport) const
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
		const UParleyDialogueSettings* DialogueSettings = GetDefault<UParleyDialogueSettings>();
		UDataTable* SpeakerTable = nullptr;
		FGameplayTag MatchedRoot;
		FString LookupError;
		if (UTagContentResolverSubsystem* Lookup = GetLookupSubsystem(this))
		{
			if (!Lookup->TryResolveDataTableForRowStruct(FARDialogueSpeakerRow::StaticStruct(), SpeakerTable, MatchedRoot, LookupError))
			{
				LookupError.Empty();
				if (DialogueSettings && DialogueSettings->SpeakerDefinitionRootTag.IsValid())
				{
					Lookup->TryResolveDataTableForRootTag(DialogueSettings->SpeakerDefinitionRootTag, SpeakerTable, LookupError);
					MatchedRoot = DialogueSettings->SpeakerDefinitionRootTag;
				}
			}

			FGameplayTag EffectiveSpeakerRoot = MatchedRoot;
			if ((!EffectiveSpeakerRoot.IsValid()) && DialogueSettings && DialogueSettings->SpeakerDefinitionRootTag.IsValid())
			{
				EffectiveSpeakerRoot = DialogueSettings->SpeakerDefinitionRootTag;
			}

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
					if (!CandidateTag.IsValid() && EffectiveSpeakerRoot.IsValid())
					{
						CandidateTag = BuildTagFromRootAndLeaf(EffectiveSpeakerRoot, RowName);
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
			TEXT("Speaker=%s|Pri=%d|Weight=%d|Chance=%s|CycleBlock=%d|CharRestrict=%d|MinRel=%s|Repeat=%d|SeenG=%d|SeenP=%d|DoneG=%d|Lock=%s|Block=%s"),
			*Header.PrimarySpeakerTag.ToString(),
			Header.Priority,
			Header.OfferWeight,
			*FString::SanitizeFloat(Header.ChanceOffered),
			Header.bBlockOfferPerCycle ? 1 : 0,
			static_cast<int32>(Header.CharacterRestriction),
			*FString::SanitizeFloat(Header.MinimumRelationshipPoints),
			Header.bRepeatable ? 1 : 0,
			Header.bSeenByGameBlocksReoffer ? 1 : 0,
			Header.bSeenByPlayerBlocksReoffer ? 1 : 0,
			Header.bCompletedByGameBlocksReoffer ? 1 : 0,
			*BuildConditionGroupKey(Header.LockedConditions),
			*BuildConditionGroupKey(Header.BlockedConditions));
	}
}

bool UParleyDialogueSubsystem::ValidateConversation(UParleyConversationAsset* ConversationAsset, FDialogueValidationReport& OutReport) const
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
	if (ConversationAsset->Header.OfferWeight < 1)
	{
		Add(
			EDialogueValidationSeverity::Warning,
			FGuid(),
			FString::Printf(
				TEXT("OfferWeight is %d. Runtime clamps this to 1."),
				ConversationAsset->Header.OfferWeight));
	}
	if (ConversationAsset->Header.ChanceOffered < 0.0f || ConversationAsset->Header.ChanceOffered > 1.0f)
	{
		Add(
			EDialogueValidationSeverity::Warning,
			FGuid(),
			FString::Printf(
				TEXT("ChanceOffered is %.3f. Runtime clamps this into [0,1]."),
				ConversationAsset->Header.ChanceOffered));
	}
	if (ConversationAsset->Header.bPrivateConversation && ConversationAsset->Header.bImportant)
	{
		Add(
			EDialogueValidationSeverity::Warning,
			FGuid(),
			TEXT("Conversation is both Private and Important. Important flow may override privacy and force additional viewers."));
	}
	if (!ConversationAsset->CompiledData.EnterNodeId.IsValid()) { Add(EDialogueValidationSeverity::Error, FGuid(), TEXT("Missing Enter node.")); }
	if (ConversationAsset->CompiledData.Nodes.IsEmpty()) { Add(EDialogueValidationSeverity::Error, FGuid(), TEXT("Compiled graph has no nodes.")); }
	ValidateConditionGroup(ConversationAsset->Header.LockedConditions, FGuid());
	ValidateConditionGroup(ConversationAsset->Header.BlockedConditions, FGuid());

	if (ConversationAsset->Header.ConversationTag.IsValid())
	{
#if WITH_EDITOR
		int32 MatchingConversationTagCount = 0;
		TArray<FString> OverlappingConversationLabels;
		const UParleyDialogueSettings* DialogueSettings = GetDefault<UParleyDialogueSettings>();
		const FString CurrentOfferGatingSignature = BuildConversationOfferGatingSignature(ConversationAsset->Header);

		if (UTagContentResolverSubsystem* Lookup = GetLookupSubsystem(this))
		{
			UDataTable* ConversationTable = nullptr;
			FGameplayTag MatchedRoot;
			FString LookupError;
			if (!Lookup->TryResolveDataTableForRowStruct(FParleyConversationAssetRow::StaticStruct(), ConversationTable, MatchedRoot, LookupError))
			{
				LookupError.Empty();
				if (DialogueSettings && DialogueSettings->ConversationDefinitionRootTag.IsValid())
				{
					Lookup->TryResolveDataTableForRootTag(DialogueSettings->ConversationDefinitionRootTag, ConversationTable, LookupError);
				}
			}

			if (ConversationTable && ConversationTable->GetRowStruct() == FParleyConversationAssetRow::StaticStruct())
			{
				for (const FName RowName : ConversationTable->GetRowNames())
				{
					const FParleyConversationAssetRow* Row = ConversationTable->FindRow<FParleyConversationAssetRow>(RowName, TEXT("DialogueConversationDuplicateCheck"), false);
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

					UParleyConversationAsset* CandidateConversation = Row->Conversation.LoadSynchronous();
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
				FString::Printf(TEXT("Duplicate conversation tag '%s' detected (%d occurrences in TagContentResolver rows)."),
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

	const UParleyDialogueSettings* DialogueSettings = GetDefault<UParleyDialogueSettings>();
	const FGameplayTag ValidationSpeakerRootTag = (DialogueSettings && DialogueSettings->SpeakerDefinitionRootTag.IsValid())
		? DialogueSettings->SpeakerDefinitionRootTag
		: FGameplayTag();

	const FARDialogueRuntimeState& Runtime = GetRuntimeState();
	TMap<FGameplayTag, FARDialogueSpeakerRow> ValidationSpeakerRows = Runtime.SpeakerRowsByTag;
	if (ValidationSpeakerRows.IsEmpty())
	{
		UDataTable* SpeakerTable = nullptr;
		FGameplayTag MatchedRoot;
		FString LookupError;
		if (UTagContentResolverSubsystem* Lookup = GetLookupSubsystem(this))
		{
			if (!Lookup->TryResolveDataTableForRowStruct(FARDialogueSpeakerRow::StaticStruct(), SpeakerTable, MatchedRoot, LookupError))
			{
				LookupError.Empty();
				if (DialogueSettings && DialogueSettings->SpeakerDefinitionRootTag.IsValid())
				{
					Lookup->TryResolveDataTableForRootTag(DialogueSettings->SpeakerDefinitionRootTag, SpeakerTable, LookupError);
					MatchedRoot = DialogueSettings->SpeakerDefinitionRootTag;
				}
			}

			FGameplayTag EffectiveSpeakerRoot = MatchedRoot;
			if ((!EffectiveSpeakerRoot.IsValid()) && DialogueSettings && DialogueSettings->SpeakerDefinitionRootTag.IsValid())
			{
				EffectiveSpeakerRoot = DialogueSettings->SpeakerDefinitionRootTag;
			}

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
						Copy.SpeakerTag = BuildTagFromRootAndLeaf(EffectiveSpeakerRoot, RowName);
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
		if (IsStructurallyValidConversationSpeakerTag(ConversationAsset->Header.PrimarySpeakerTag, ValidationSpeakerRootTag))
		{
			Add(EDialogueValidationSeverity::Warning, FGuid(),
				FString::Printf(
					TEXT("PrimarySpeakerTag '%s' is not currently resolved in speaker rows; runtime may still resolve it by speaker root '%s'."),
					*ConversationAsset->Header.PrimarySpeakerTag.ToString(),
					*ValidationSpeakerRootTag.ToString()));
		}
		else
		{
			Add(EDialogueValidationSeverity::Error, FGuid(),
				FString::Printf(TEXT("PrimarySpeakerTag '%s' is not resolvable to a known speaker."),
					*ConversationAsset->Header.PrimarySpeakerTag.ToString()));
		}
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
			if (IsStructurallyValidConversationSpeakerTag(ParticipantTag, ValidationSpeakerRootTag))
			{
				Add(EDialogueValidationSeverity::Warning, FGuid(),
					FString::Printf(
						TEXT("Participating speaker tag '%s' is not currently resolved in speaker rows; runtime may still resolve it by speaker root '%s'."),
						*ParticipantTag.ToString(),
						*ValidationSpeakerRootTag.ToString()));
			}
			else
			{
				Add(EDialogueValidationSeverity::Error, FGuid(),
					FString::Printf(TEXT("Participating speaker tag '%s' is not resolvable to a known speaker."),
						*ParticipantTag.ToString()));
			}
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
	auto ValidateLineEntry = [&](
		const FDialogueLineNodeData& LineData,
		const FGuid& NodeId,
		const TCHAR* ContextLabel)
	{
		if (LineData.Line.Text.IsEmpty())
		{
			Add(
				EDialogueValidationSeverity::Error,
				NodeId,
				FString::Printf(TEXT("%s text is missing (sound-only lines are invalid)."), ContextLabel));
		}
		if (LineData.Line.LengthSeconds <= 0.0f && LineData.Line.Sound == nullptr)
		{
			Add(
				EDialogueValidationSeverity::Warning,
				NodeId,
				FString::Printf(
					TEXT("%s has no audio and Length Seconds is 0. Set a positive Length Seconds value or assign a Sound."),
					ContextLabel));
		}
		if (!LineData.Line.SpeakerTag.IsValid())
		{
			Add(
				EDialogueValidationSeverity::Error,
				NodeId,
				FString::Printf(TEXT("%s speaker tag is invalid."), ContextLabel));
		}
		else if (!IsResolvableConversationSpeakerTag(ValidationSpeakerRows, LineData.Line.SpeakerTag))
		{
			Add(
				EDialogueValidationSeverity::Error,
				NodeId,
				FString::Printf(
					TEXT("%s speaker '%s' does not resolve to a known speaker."),
					ContextLabel,
					*LineData.Line.SpeakerTag.ToString()));
		}
		else if (!ParticipantSpeakerTags.Contains(LineData.Line.SpeakerTag))
		{
			Add(
				EDialogueValidationSeverity::Warning,
				NodeId,
				FString::Printf(
					TEXT("%s speaker '%s' is not listed in ParticipatingSpeakerTags/PrimarySpeakerTag."),
					ContextLabel,
					*LineData.Line.SpeakerTag.ToString()));
		}
		if (!LineData.Line.LocalLineGuid.IsValid())
		{
			Add(
				EDialogueValidationSeverity::Error,
				NodeId,
				FString::Printf(TEXT("%s LocalLineGuid must be valid."), ContextLabel));
		}
		else if (LineGuidSet.Contains(LineData.Line.LocalLineGuid))
		{
			Add(
				EDialogueValidationSeverity::Warning,
				NodeId,
				TEXT("Repeated line LocalLineGuid detected in conversation."));
		}
		LineGuidSet.Add(LineData.Line.LocalLineGuid);

		ValidateConditionGroup(LineData.SkipLockedConditions, NodeId);
		ValidateConditionGroup(LineData.SkipBlockedConditions, NodeId);
	};
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
		case EDialogueNodeType::Route:
			RegisterEdge(Node.NodeId, Node.NextNodeId, true, TEXT("Route node next output"));
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
				ValidateLineEntry(*LineData, Node.NodeId, TEXT("Line"));
			}
			RegisterEdge(Node.NodeId, Node.NextNodeId, true, TEXT("Line node next output"));
			break;
		}
		case EDialogueNodeType::MultiLine:
		{
			const FDialogueMultiLineNodeData* MultiLineData = Node.NodeData.GetPtr<FDialogueMultiLineNodeData>();
			if (!MultiLineData)
			{
				Add(EDialogueValidationSeverity::Error, Node.NodeId, TEXT("Multi-line node payload mismatch."));
			}
			else
			{
				if (MultiLineData->Lines.IsEmpty())
				{
					Add(EDialogueValidationSeverity::Warning, Node.NodeId, TEXT("Multi-line node has zero lines."));
				}

				TSet<FGuid> SeenEntryIds;
				for (const FDialogueMultiLineEntry& Entry : MultiLineData->Lines)
				{
					if (!Entry.EntryId.IsValid())
					{
						Add(EDialogueValidationSeverity::Error, Node.NodeId, TEXT("Multi-line entry has invalid EntryId."));
					}
					else if (SeenEntryIds.Contains(Entry.EntryId))
					{
						Add(EDialogueValidationSeverity::Error, Node.NodeId, TEXT("Duplicate EntryId found in multi-line node."));
					}
					SeenEntryIds.Add(Entry.EntryId);

					ValidateLineEntry(Entry.LineData, Node.NodeId, TEXT("Multi-line entry"));
				}
			}

			RegisterEdge(Node.NodeId, Node.NextNodeId, true, TEXT("Multi-line node next output"));
			break;
		}
		case EDialogueNodeType::SplitLine:
		{
			const FDialogueMultiLineNodeData* SplitLineData = Node.NodeData.GetPtr<FDialogueMultiLineNodeData>();
			if (!SplitLineData)
			{
				Add(EDialogueValidationSeverity::Error, Node.NodeId, TEXT("Split-line node payload mismatch."));
			}
			else
			{
				if (SplitLineData->Lines.Num() < 2)
				{
					Add(EDialogueValidationSeverity::Warning, Node.NodeId, TEXT("Split-line node should author at least two lines."));
				}

				TSet<FGuid> SeenEntryIds;
				for (const FDialogueMultiLineEntry& Entry : SplitLineData->Lines)
				{
					if (!Entry.EntryId.IsValid())
					{
						Add(EDialogueValidationSeverity::Error, Node.NodeId, TEXT("Split-line entry has invalid EntryId."));
					}
					else if (SeenEntryIds.Contains(Entry.EntryId))
					{
						Add(EDialogueValidationSeverity::Error, Node.NodeId, TEXT("Duplicate EntryId found in split-line node."));
					}
					SeenEntryIds.Add(Entry.EntryId);

					ValidateLineEntry(Entry.LineData, Node.NodeId, TEXT("Split-line entry"));
				}
			}

			RegisterEdge(Node.NodeId, Node.NextNodeId, true, TEXT("Split-line node next output"));
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
		case EDialogueNodeType::Sequence:
		{
			TSet<FGuid> SeenSequenceBranchIds;
			int32 ConnectedBranchCount = 0;
			for (const FDialogueCompiledSequenceBranch& Branch : Node.SequenceBranches)
			{
				if (!Branch.BranchId.IsValid())
				{
					Add(EDialogueValidationSeverity::Error, Node.NodeId, TEXT("Sequence branch has invalid BranchId."));
					continue;
				}
				if (SeenSequenceBranchIds.Contains(Branch.BranchId))
				{
					Add(EDialogueValidationSeverity::Error, Node.NodeId, TEXT("Sequence branch id is duplicated in node."));
				}
				SeenSequenceBranchIds.Add(Branch.BranchId);
				if (Branch.NextNodeId.IsValid())
				{
					++ConnectedBranchCount;
				}
				RegisterEdge(Node.NodeId, Branch.NextNodeId, true, TEXT("Sequence branch output"));
			}
			if (Node.SequenceBranches.IsEmpty())
			{
				Add(EDialogueValidationSeverity::Warning, Node.NodeId, TEXT("Sequence node has no branches."));
			}
			else if (ConnectedBranchCount == 0)
			{
				Add(EDialogueValidationSeverity::Warning, Node.NodeId, TEXT("Sequence node has no connected branch outputs and will end non-completed."));
			}
			break;
		}
		case EDialogueNodeType::RouteByCharacter:
		{
			TSet<FGuid> SeenBranchIds;
			TSet<FGameplayTag> SeenSpeakerTags;
			int32 ConnectedBranchCount = 0;
			for (int32 BranchIndex = 0; BranchIndex < Node.CharacterRouteBranches.Num(); ++BranchIndex)
			{
				const FDialogueCompiledCharacterRouteBranch& Branch = Node.CharacterRouteBranches[BranchIndex];
				if (!Branch.BranchId.IsValid())
				{
					Add(EDialogueValidationSeverity::Error, Node.NodeId, TEXT("Character route branch has invalid BranchId."));
					continue;
				}
				if (SeenBranchIds.Contains(Branch.BranchId))
				{
					Add(EDialogueValidationSeverity::Error, Node.NodeId, TEXT("Character route branch id is duplicated in node."));
				}
				SeenBranchIds.Add(Branch.BranchId);

				if (!Branch.SpeakerTag.IsValid())
				{
					Add(EDialogueValidationSeverity::Error, Node.NodeId, TEXT("Character route branch requires a valid SpeakerTag."));
				}
				else if (!IsResolvableConversationSpeakerTag(ValidationSpeakerRows, Branch.SpeakerTag))
				{
					Add(
						EDialogueValidationSeverity::Warning,
						Node.NodeId,
						FString::Printf(
							TEXT("Character route branch speaker '%s' does not resolve to a known speaker."),
							*Branch.SpeakerTag.ToString()));
				}
				else if (SeenSpeakerTags.Contains(Branch.SpeakerTag))
				{
					Add(
						EDialogueValidationSeverity::Warning,
						Node.NodeId,
						FString::Printf(
							TEXT("Character route branch ordering ambiguity: duplicate speaker tag '%s' found; first-match order controls routing."),
							*Branch.SpeakerTag.ToString()));
				}
				SeenSpeakerTags.Add(Branch.SpeakerTag);

				if (Branch.NextNodeId.IsValid())
				{
					++ConnectedBranchCount;
				}
				RegisterEdge(Node.NodeId, Branch.NextNodeId, true, TEXT("Character route branch output"));
			}
			if (Node.CharacterRouteBranches.IsEmpty())
			{
				Add(EDialogueValidationSeverity::Warning, Node.NodeId, TEXT("Character route node has no branches."));
			}
			else if (ConnectedBranchCount == 0)
			{
				Add(EDialogueValidationSeverity::Warning, Node.NodeId, TEXT("Character route node has no connected outputs and will end non-completed."));
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

	// Orphan/unreachable nodes are allowed as staging/storage nodes during authoring.
	// Keep their diagnostics visible, but do not fail compile because of them.
	for (FDialogueValidationIssue& Issue : OutReport.Issues)
	{
		if (Issue.Severity != EDialogueValidationSeverity::Error || !Issue.NodeId.IsValid())
		{
			continue;
		}

		if (!ReachableNodeIds.Contains(Issue.NodeId))
		{
			Issue.Severity = EDialogueValidationSeverity::Warning;
			Issue.Message = FText::FromString(FString::Printf(TEXT("[Orphan Node] %s"), *Issue.Message.ToString()));
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
	const UParleyDialogueSubsystem* DialogueSubsystem,
	const UParleyConversationAsset* Conversation,
	APlayerState* RequesterPS,
	const FARPlayerIdentity& PlayerIdentity)
{
	FDialogueRuntimeContext Context;
	Context.World = DialogueSubsystem ? DialogueSubsystem->GetWorld() : nullptr;
	Context.GameState = Context.World ? Context.World->GetGameState() : nullptr;
	Context.ConversationTag = Conversation ? Conversation->Header.ConversationTag : FGameplayTag();
	Context.ConversationAsset = const_cast<UParleyConversationAsset*>(Conversation);
	Context.PrimarySpeakerTag = Conversation ? Conversation->Header.PrimarySpeakerTag : FGameplayTag();
	Context.ActivePlayerState = RequesterPS;
	Context.ActivePlayerController = RequesterPS ? Cast<APlayerController>(RequesterPS->GetOwner()) : nullptr;
	Context.ResolvedPlayerSpeakerTag = ResolvePlayerSpeakerTag(RequesterPS);
	Context.RelationshipPointsForPrimarySpeaker = DialogueSubsystem ? DialogueSubsystem->GetRelationshipPointsForSpeaker(Context.PrimarySpeakerTag) : 0.0f;
	Context.RelationshipLevelForPrimarySpeaker = DialogueSubsystem ? DialogueSubsystem->GetRelationshipLevelForSpeaker(Context.PrimarySpeakerTag) : 0;
	if (RequesterPS)
	{
		GetLoadoutTagsFromPlayerState(RequesterPS, Context.LoadoutView.LoadoutTags);
	}

	const FParleyProgressionStore* ProgressionStore = GetProgressionStore(DialogueSubsystem);
	if (ProgressionStore)
	{
		Context.GameOnlyProgressionTags = ProgressionStore->ProgressionTags;
		GetProgressionTagsForIdentity(ProgressionStore, PlayerIdentity, Context.PlayerOnlyProgressionTags);
		Context.CombinedProgressionTags = DialogueSubsystem->GetCombinedDialogueTags(Context.PlayerOnlyProgressionTags, Context.GameOnlyProgressionTags);
		Context.bCompletedByGame = ProgressionStore->DialogueCompletedConversationTagsByGame.HasTagExact(Context.ConversationTag);
		if (const FDialoguePlayerPersistentState* PlayerState = FindPlayerDialogueState(ProgressionStore, PlayerIdentity))
		{
			Context.bCompletedByPlayer = PlayerState->CompletedConversationTags.HasTagExact(Context.ConversationTag);
		}
	}

	return Context;
}

static bool PassesConversationOfferRules(
	const UParleyDialogueSubsystem* DialogueSubsystem,
	const FDialogueRuntimeContext& Context,
	const FDialogueConversationHeader& Header)
{
	if (!PassesCharacterRestriction(Header.CharacterRestriction, Context.ResolvedPlayerSpeakerTag))
	{
		return false;
	}

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
	const UParleyDialogueSubsystem* DialogueSubsystem,
	const FDialogueRuntimeContext& Context,
	const FDialogueConversationHeader& Header,
	FString* OutFailureReason)
{
	if (!PassesCharacterRestriction(Header.CharacterRestriction, Context.ResolvedPlayerSpeakerTag))
	{
		if (OutFailureReason)
		{
			*OutFailureReason = TEXT("CharacterRestriction failed.");
		}
		return false;
	}

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

