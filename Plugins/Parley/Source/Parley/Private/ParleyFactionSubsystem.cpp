#include "ParleyFactionSubsystem.h"

#include "ParleyFactionSettings.h"
#include "ParleyLog.h"
#include "ParleyPlayerControllerInterface.h"
#include "TagKeySubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagsManager.h"
#include "StructUtils/InstancedStruct.h"

namespace
{
static bool IsAuthorityWorld_Faction(const UWorld* World)
{
	if (!World)
	{
		return false;
	}

	return World->GetNetMode() == NM_Standalone || World->GetAuthGameMode() != nullptr;
}

// Faction mutation events are purely authoritative, so relay them to every local/controller-bound widget receiver.
template <typename TCallback>
static void ForEachFactionWidgetController(UWorld* World, TCallback&& Callback)
{
	if (!World)
	{
		return;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Controller = It->Get();
		if (IParleyPlayerControllerInterface* ControllerInterface = Cast<IParleyPlayerControllerInterface>(Controller))
		{
			Callback(*ControllerInterface);
		}
	}
}
}

void UParleyFactionSubsystem::Deinitialize()
{
	PersistedFactionPopularityStates.Reset();
	PersistedFactionSpeakerReputationStates.Reset();
	GameProgressionTags.Reset();
	Super::Deinitialize();
}

bool UParleyFactionSubsystem::GetFactionDefinition(const FGameplayTag FactionTag, FParleyFactionDefinitionRow& OutDefinition) const
{
	FString Error;
	if (!ResolveFactionDefinition(FactionTag, OutDefinition, Error))
	{
		UE_LOG(ParleyLog, Verbose, TEXT("[Faction] GetFactionDefinition failed for '%s': %s"), *FactionTag.ToString(), *Error);
		return false;
	}

	return true;
}

void UParleyFactionSubsystem::GetAllFactionTags(TArray<FGameplayTag>& OutFactionTags) const
{
	FString Error;
	if (!BuildFactionTagList(OutFactionTags, Error))
	{
		OutFactionTags.Reset();
		UE_LOG(ParleyLog, Verbose, TEXT("[Faction] GetAllFactionTags failed: %s"), *Error);
	}
}

void UParleyFactionSubsystem::GetFactionPopularityStates(TArray<FParleyFactionState>& OutStates) const
{
	OutStates = PersistedFactionPopularityStates;
}

void UParleyFactionSubsystem::GetFactionSpeakerReputationStates(TArray<FParleyFactionSpeakerReputationState>& OutStates) const
{
	OutStates = PersistedFactionSpeakerReputationStates;
}

float UParleyFactionSubsystem::GetFactionPopularity(const FGameplayTag FactionTag) const
{
	if (!FactionTag.IsValid())
	{
		return 0.0f;
	}

	if (const FParleyFactionState* Existing = FindFactionPopularityState(FactionTag))
	{
		return Existing->Popularity;
	}

	FParleyFactionDefinitionRow Definition;
	FString Error;
	if (ResolveFactionDefinition(FactionTag, Definition, Error))
	{
		return ClampPopularity(Definition, Definition.BasePopularity);
	}

	return 0.0f;
}

float UParleyFactionSubsystem::GetEffectiveFactionPopularity(const FGameplayTag FactionTag) const
{
	if (!FactionTag.IsValid())
	{
		return 0.0f;
	}

	const float BasePopularity = GetFactionPopularity(FactionTag);
	FParleyFactionDefinitionRow Definition;
	FString Error;
	if (!ResolveFactionDefinition(FactionTag, Definition, Error))
	{
		return BasePopularity;
	}

	return ClampPopularity(Definition, BasePopularity + ComputeModifierDelta(Definition, GameProgressionTags));
}

float UParleyFactionSubsystem::GetFactionSpeakerReputation(const FGameplayTag FactionTag, const FGameplayTag SpeakerTag) const
{
	if (!FactionTag.IsValid() || !SpeakerTag.IsValid())
	{
		return 0.0f;
	}

	if (const FParleyFactionSpeakerReputationState* Existing = FindSpeakerReputationState(FactionTag, SpeakerTag))
	{
		return Existing->Reputation;
	}

	return 0.0f;
}

bool UParleyFactionSubsystem::ModifyFactionPopularity(const FGameplayTag FactionTag, const float DeltaPopularity)
{
	if (!IsAuthorityWorld_Faction(GetWorld()) || !FactionTag.IsValid())
	{
		return false;
	}

	const float CurrentValue = GetFactionPopularity(FactionTag);
	float NewValue = CurrentValue + DeltaPopularity;
	FParleyFactionDefinitionRow Definition;
	FString Error;
	if (ResolveFactionDefinition(FactionTag, Definition, Error))
	{
		NewValue = ClampPopularity(Definition, NewValue);
	}

	const float AppliedDelta = NewValue - CurrentValue;
	if (FMath::IsNearlyZero(AppliedDelta))
	{
		return true;
	}

	if (FParleyFactionState* Existing = FindFactionPopularityStateMutable(FactionTag))
	{
		Existing->Popularity = NewValue;
	}
	else
	{
		FParleyFactionState& Added = PersistedFactionPopularityStates.AddDefaulted_GetRef();
		Added.FactionTag = FactionTag;
		Added.Popularity = NewValue;
	}

	OnFactionPopularityChanged.Broadcast(FactionTag, AppliedDelta, NewValue);
	ForEachFactionWidgetController(GetWorld(), [FactionTag, AppliedDelta, NewValue](IParleyPlayerControllerInterface& ControllerInterface)
	{
		ControllerInterface.NotifyFactionPopularityChanged(FactionTag, AppliedDelta, NewValue);
	});
	return true;
}

bool UParleyFactionSubsystem::ModifyFactionSpeakerReputation(
	const FGameplayTag FactionTag,
	const FGameplayTag SpeakerTag,
	const float DeltaReputation)
{
	if (!IsAuthorityWorld_Faction(GetWorld()) || !FactionTag.IsValid() || !SpeakerTag.IsValid())
	{
		return false;
	}

	const float CurrentValue = GetFactionSpeakerReputation(FactionTag, SpeakerTag);
	const float NewValue = CurrentValue + DeltaReputation;
	const float AppliedDelta = NewValue - CurrentValue;
	if (FMath::IsNearlyZero(AppliedDelta))
	{
		return true;
	}

	if (FParleyFactionSpeakerReputationState* Existing = FindSpeakerReputationStateMutable(FactionTag, SpeakerTag))
	{
		Existing->Reputation = NewValue;
	}
	else
	{
		FParleyFactionSpeakerReputationState& Added = PersistedFactionSpeakerReputationStates.AddDefaulted_GetRef();
		Added.FactionTag = FactionTag;
		Added.SpeakerTag = SpeakerTag;
		Added.Reputation = NewValue;
	}

	OnFactionSpeakerReputationChanged.Broadcast(FactionTag, SpeakerTag, AppliedDelta, NewValue);
	ForEachFactionWidgetController(GetWorld(), [FactionTag, SpeakerTag, AppliedDelta, NewValue](IParleyPlayerControllerInterface& ControllerInterface)
	{
		ControllerInterface.NotifyFactionSpeakerReputationChanged(FactionTag, SpeakerTag, AppliedDelta, NewValue);
	});
	return true;
}

void UParleyFactionSubsystem::UpdateFactionPopularityFromReplication(
	const FGameplayTag FactionTag,
	const float DeltaPopularity,
	const float NewTotal)
{
	if (!FactionTag.IsValid())
	{
		return;
	}

	if (FParleyFactionState* Existing = FindFactionPopularityStateMutable(FactionTag))
	{
		Existing->Popularity = NewTotal;
	}
	else
	{
		FParleyFactionState& Added = PersistedFactionPopularityStates.AddDefaulted_GetRef();
		Added.FactionTag = FactionTag;
		Added.Popularity = NewTotal;
	}

	OnFactionPopularityChanged.Broadcast(FactionTag, DeltaPopularity, NewTotal);
}

void UParleyFactionSubsystem::UpdateFactionSpeakerReputationFromReplication(
	const FGameplayTag FactionTag,
	const FGameplayTag SpeakerTag,
	const float DeltaReputation,
	const float NewTotal)
{
	if (!FactionTag.IsValid() || !SpeakerTag.IsValid())
	{
		return;
	}

	if (FParleyFactionSpeakerReputationState* Existing = FindSpeakerReputationStateMutable(FactionTag, SpeakerTag))
	{
		Existing->Reputation = NewTotal;
	}
	else
	{
		FParleyFactionSpeakerReputationState& Added = PersistedFactionSpeakerReputationStates.AddDefaulted_GetRef();
		Added.FactionTag = FactionTag;
		Added.SpeakerTag = SpeakerTag;
		Added.Reputation = NewTotal;
	}

	OnFactionSpeakerReputationChanged.Broadcast(FactionTag, SpeakerTag, DeltaReputation, NewTotal);
}

void UParleyFactionSubsystem::SetFactionPopularityStates(const TArray<FParleyFactionState>& States)
{
	PersistedFactionPopularityStates.Reset();
	for (const FParleyFactionState& State : States)
	{
		if (!State.FactionTag.IsValid())
		{
			continue;
		}

		if (FParleyFactionState* Existing = FindFactionPopularityStateMutable(State.FactionTag))
		{
			Existing->Popularity = State.Popularity;
			continue;
		}

		FParleyFactionState& Added = PersistedFactionPopularityStates.AddDefaulted_GetRef();
		Added.FactionTag = State.FactionTag;
		Added.Popularity = State.Popularity;
	}
}

void UParleyFactionSubsystem::SetFactionSpeakerReputationStates(const TArray<FParleyFactionSpeakerReputationState>& States)
{
	PersistedFactionSpeakerReputationStates.Reset();
	for (const FParleyFactionSpeakerReputationState& State : States)
	{
		if (!State.FactionTag.IsValid() || !State.SpeakerTag.IsValid())
		{
			continue;
		}

		if (FParleyFactionSpeakerReputationState* Existing = FindSpeakerReputationStateMutable(State.FactionTag, State.SpeakerTag))
		{
			Existing->Reputation = State.Reputation;
			continue;
		}

		FParleyFactionSpeakerReputationState& Added = PersistedFactionSpeakerReputationStates.AddDefaulted_GetRef();
		Added.FactionTag = State.FactionTag;
		Added.SpeakerTag = State.SpeakerTag;
		Added.Reputation = State.Reputation;
	}
}

void UParleyFactionSubsystem::SetProgressionTags(const FGameplayTagContainer& Tags)
{
	GameProgressionTags = Tags;
}

FParleyFactionState* UParleyFactionSubsystem::FindFactionPopularityStateMutable(const FGameplayTag FactionTag)
{
	for (FParleyFactionState& Entry : PersistedFactionPopularityStates)
	{
		if (Entry.FactionTag.MatchesTagExact(FactionTag))
		{
			return &Entry;
		}
	}

	return nullptr;
}

const FParleyFactionState* UParleyFactionSubsystem::FindFactionPopularityState(const FGameplayTag FactionTag) const
{
	for (const FParleyFactionState& Entry : PersistedFactionPopularityStates)
	{
		if (Entry.FactionTag.MatchesTagExact(FactionTag))
		{
			return &Entry;
		}
	}

	return nullptr;
}

FParleyFactionSpeakerReputationState* UParleyFactionSubsystem::FindSpeakerReputationStateMutable(
	const FGameplayTag FactionTag,
	const FGameplayTag SpeakerTag)
{
	for (FParleyFactionSpeakerReputationState& Entry : PersistedFactionSpeakerReputationStates)
	{
		if (Entry.FactionTag.MatchesTagExact(FactionTag) && Entry.SpeakerTag.MatchesTagExact(SpeakerTag))
		{
			return &Entry;
		}
	}

	return nullptr;
}

const FParleyFactionSpeakerReputationState* UParleyFactionSubsystem::FindSpeakerReputationState(
	const FGameplayTag FactionTag,
	const FGameplayTag SpeakerTag) const
{
	for (const FParleyFactionSpeakerReputationState& Entry : PersistedFactionSpeakerReputationStates)
	{
		if (Entry.FactionTag.MatchesTagExact(FactionTag) && Entry.SpeakerTag.MatchesTagExact(SpeakerTag))
		{
			return &Entry;
		}
	}

	return nullptr;
}

bool UParleyFactionSubsystem::BuildFactionTagList(TArray<FGameplayTag>& OutFactionTags, FString& OutError) const
{
	OutFactionTags.Reset();
	OutError.Reset();

	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		OutError = TEXT("GameInstance is null.");
		return false;
	}

	UTagKeySubsystem* Lookup = GI->GetSubsystem<UTagKeySubsystem>();
	if (!Lookup)
	{
		OutError = TEXT("TagKeySubsystem missing.");
		return false;
	}

	const UParleyFactionSettings* FactionSettings = GetDefault<UParleyFactionSettings>();
	const FGameplayTag RootTag = (FactionSettings ? FactionSettings->FactionDefinitionRootTag : FGameplayTag());
	if (!RootTag.IsValid())
	{
		OutError = TEXT("FactionDefinitionRootTag is not configured.");
		return false;
	}

	TArray<FName> RowNames;
	if (!Lookup->TryGetRowNamesForRootTag(RootTag, RowNames, OutError))
	{
		return false;
	}

	for (const FName RowName : RowNames)
	{
		const FGameplayTag CandidateTag = BuildFactionTagFromRootAndLeaf(RootTag, RowName);
		if (!CandidateTag.IsValid())
		{
			continue;
		}

		FParleyFactionDefinitionRow Row;
		FString ResolveError;
		if (!ResolveFactionDefinition(CandidateTag, Row, ResolveError))
		{
			UE_LOG(ParleyLog, Verbose, TEXT("[Faction] BuildFactionTagList failed to resolve '%s': %s"), *CandidateTag.ToString(), *ResolveError);
			continue;
		}

		const FGameplayTag EffectiveFactionTag = Row.FactionTag.IsValid() ? Row.FactionTag : CandidateTag;
		if (!OutFactionTags.ContainsByPredicate([&EffectiveFactionTag](const FGameplayTag ExistingTag)
			{
				return ExistingTag.MatchesTagExact(EffectiveFactionTag);
			}))
		{
			OutFactionTags.Add(EffectiveFactionTag);
		}
	}

	return true;
}

bool UParleyFactionSubsystem::ResolveFactionDefinition(const FGameplayTag& FactionTag, FParleyFactionDefinitionRow& OutRow, FString& OutError) const
{
	OutError.Reset();

	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		OutError = TEXT("GameInstance is null.");
		return false;
	}

	UTagKeySubsystem* Lookup = GI->GetSubsystem<UTagKeySubsystem>();
	if (!Lookup)
	{
		OutError = TEXT("TagKeySubsystem missing.");
		return false;
	}

	FInstancedStruct RowData;
	if (!Lookup->TryResolveRowStructForTag(FactionTag, RowData, OutError))
	{
		return false;
	}

	if (const FParleyFactionDefinitionRow* TypedRow = RowData.GetPtr<FParleyFactionDefinitionRow>())
	{
		OutRow = *TypedRow;
		return true;
	}

	OutError = FString::Printf(TEXT("Row struct mismatch for '%s'; expected FParleyFactionDefinitionRow."), *FactionTag.ToString());
	return false;
}

float UParleyFactionSubsystem::ComputeModifierDelta(const FParleyFactionDefinitionRow& Row, const FGameplayTagContainer& ProgressionTags) const
{
	float Delta = 0.0f;
	for (const FParleyFactionPopularityModifierRule& Rule : Row.PopularityModifierRules)
	{
		if (!Rule.ConditionTag.IsValid())
		{
			continue;
		}

		if (ProgressionTags.HasTag(Rule.ConditionTag))
		{
			Delta += Rule.Delta;
		}
	}

	return Delta;
}

float UParleyFactionSubsystem::ClampPopularity(const FParleyFactionDefinitionRow& Row, const float Value)
{
	const float MinValue = FMath::Min(Row.MinPopularity, Row.MaxPopularity);
	const float MaxValue = FMath::Max(Row.MinPopularity, Row.MaxPopularity);
	return FMath::Clamp(Value, MinValue, MaxValue);
}

FGameplayTag UParleyFactionSubsystem::BuildFactionTagFromRootAndLeaf(const FGameplayTag& RootTag, FName LeafRowName)
{
	if (!RootTag.IsValid() || LeafRowName.IsNone())
	{
		return FGameplayTag();
	}

	const FString TagPath = FString::Printf(TEXT("%s.%s"), *RootTag.ToString(), *LeafRowName.ToString());
	return UGameplayTagsManager::Get().RequestGameplayTag(FName(*TagPath), false);
}
