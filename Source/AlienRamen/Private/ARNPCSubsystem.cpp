#include "ARNPCSubsystem.h"

#include "ARDialogueSettings.h"
#include "ARDialogueSubsystem.h"
#include "ARDialogueTypes.h"
#include "ARLog.h"
#include "ARNPCSettings.h"
#include "ARSaveGame.h"
#include "ARSaveSubsystem.h"
#include "ContentLookupSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameplayTagsManager.h"

namespace
{
	static bool IsAuthorityWorld_NPC(const UWorld* World)
	{
		if (!World)
		{
			return false;
		}
		return World->GetNetMode() == NM_Standalone || World->GetAuthGameMode() != nullptr;
	}

	static bool BuildNpcDefinitionTagFromRootAndLeaf(const FGameplayTag& RootTag, const FName Leaf, FGameplayTag& OutTag)
	{
		OutTag = FGameplayTag();
		if (!RootTag.IsValid() || Leaf.IsNone())
		{
			return false;
		}
		const FString Combined = FString::Printf(TEXT("%s.%s"), *RootTag.ToString(), *Leaf.ToString());
		OutTag = FGameplayTag::RequestGameplayTag(FName(*Combined), false);
		return OutTag.IsValid();
	}

	static bool ResolveNpcDefinition(UContentLookupSubsystem* Lookup, const FGameplayTag& NpcTag, FARNpcDefinitionRow& OutRow)
	{
		if (!Lookup || !NpcTag.IsValid())
		{
			return false;
		}

		FInstancedStruct RowData;
		FString Error;
		if (!Lookup->LookupWithGameplayTag(NpcTag, RowData, Error))
		{
			UE_LOG(ARLog, Verbose, TEXT("[NPC] Resolve definition failed '%s': %s"), *NpcTag.ToString(), *Error);
			return false;
		}

		if (const FARNpcDefinitionRow* Typed = RowData.GetPtr<FARNpcDefinitionRow>())
		{
			OutRow = *Typed;
			if (!OutRow.NpcTag.IsValid())
			{
				OutRow.NpcTag = NpcTag;
			}
			return true;
		}

		return false;
	}
}

void UARNPCSubsystem::Deinitialize()
{
	NpcTalkableCache.Reset();
	Super::Deinitialize();
}

static UARSaveSubsystem* GetSaveSubsystem(const UARNPCSubsystem* Subsystem)
{
	if (UGameInstance* GI = Subsystem ? Subsystem->GetGameInstance() : nullptr)
	{
		return GI->GetSubsystem<UARSaveSubsystem>();
	}
	return nullptr;
}

static UContentLookupSubsystem* GetLookupSubsystem(const UARNPCSubsystem* Subsystem)
{
	if (UGameInstance* GI = Subsystem ? Subsystem->GetGameInstance() : nullptr)
	{
		return GI->GetSubsystem<UContentLookupSubsystem>();
	}
	return nullptr;
}

static UARDialogueSubsystem* GetDialogueSubsystem(const UARNPCSubsystem* Subsystem)
{
	if (UGameInstance* GI = Subsystem ? Subsystem->GetGameInstance() : nullptr)
	{
		return GI->GetSubsystem<UARDialogueSubsystem>();
	}
	return nullptr;
}

static FARNpcRelationshipState* FindNpcState(UARSaveGame* SaveGame, const FGameplayTag NpcTag)
{
	if (!SaveGame || !NpcTag.IsValid())
	{
		return nullptr;
	}

	for (FARNpcRelationshipState& State : SaveGame->NpcRelationshipStates)
	{
		if (State.NpcTag.MatchesTagExact(NpcTag))
		{
			return &State;
		}
	}
	return nullptr;
}

static FARNpcRelationshipState* FindOrAddNpcState(UARNPCSubsystem* Subsystem, UARSaveGame* SaveGame, const FGameplayTag NpcTag)
{
	if (!SaveGame || !NpcTag.IsValid())
	{
		return nullptr;
	}

	if (FARNpcRelationshipState* Existing = FindNpcState(SaveGame, NpcTag))
	{
		return Existing;
	}

	FARNpcRelationshipState& Added = SaveGame->NpcRelationshipStates.AddDefaulted_GetRef();
	Added.NpcTag = NpcTag;

	if (UContentLookupSubsystem* Lookup = GetLookupSubsystem(Subsystem))
	{
		FARNpcDefinitionRow Def;
		if (ResolveNpcDefinition(Lookup, NpcTag, Def))
		{
			Added.LoveRating = FMath::Max(0, Def.StartingLoveRating);
			Added.CurrentWantTag = Def.InitialWantTag;
			Added.bCurrentWantSatisfied = false;
		}
	}

	return &Added;
}

bool UARNPCSubsystem::SubmitNpcRamenDelivery(FGameplayTag NpcTag, FGameplayTag DeliveredRamenTag, bool& bOutAccepted)
{
	bOutAccepted = false;

	if (!IsAuthorityWorld_NPC(GetWorld()) || !NpcTag.IsValid() || !DeliveredRamenTag.IsValid())
	{
		return false;
	}

	UARSaveSubsystem* SaveSubsystem = GetSaveSubsystem(this);
	UARSaveGame* SaveGame = SaveSubsystem ? SaveSubsystem->GetCurrentSaveGame() : nullptr;
	if (!SaveGame)
	{
		return false;
	}

	FARNpcRelationshipState* State = FindOrAddNpcState(this, SaveGame, NpcTag);
	if (!State)
	{
		return false;
	}

	if (!State->CurrentWantTag.IsValid() || !State->CurrentWantTag.MatchesTagExact(DeliveredRamenTag))
	{
		return true;
	}

	FARNpcDefinitionRow Def;
	if (UContentLookupSubsystem* Lookup = GetLookupSubsystem(this))
	{
		ResolveNpcDefinition(Lookup, NpcTag, Def);
	}

	State->LoveRating = FMath::Max(0, State->LoveRating + FMath::Max(1, Def.LoveIncreaseOnWantDelivery));
	State->bCurrentWantSatisfied = true;
	bOutAccepted = true;
	SaveSubsystem->MarkSaveDirty();

	RefreshNpcTalkableState(NpcTag);
	return true;
}

bool UARNPCSubsystem::TryGetNpcRelationshipState(FGameplayTag NpcTag, FARNpcRelationshipState& OutState) const
{
	OutState = FARNpcRelationshipState();
	if (!NpcTag.IsValid())
	{
		return false;
	}

	UARSaveSubsystem* SaveSubsystem = GetSaveSubsystem(this);
	const UARSaveGame* SaveGame = SaveSubsystem ? SaveSubsystem->GetCurrentSaveGame() : nullptr;
	if (!SaveGame)
	{
		return false;
	}

	for (const FARNpcRelationshipState& State : SaveGame->NpcRelationshipStates)
	{
		if (State.NpcTag.MatchesTagExact(NpcTag))
		{
			OutState = State;
			return true;
		}
	}

	return false;
}

bool UARNPCSubsystem::IsNpcTalkable(FGameplayTag NpcTag) const
{
	if (!NpcTag.IsValid())
	{
		return false;
	}

	if (const bool* Cached = NpcTalkableCache.Find(NpcTag))
	{
		return *Cached;
	}

	return false;
}

bool UARNPCSubsystem::RefreshNpcTalkableState(FGameplayTag NpcTag)
{
	if (!NpcTag.IsValid())
	{
		return false;
	}

	UARDialogueSubsystem* DialogueSubsystem = GetDialogueSubsystem(this);
	if (!DialogueSubsystem)
	{
		return false;
	}

	const bool bNewTalkable = DialogueSubsystem->HasUnlockedDialogueForNpcForAnyPlayer(NpcTag);
	const bool bHadExisting = NpcTalkableCache.Contains(NpcTag);
	const bool bOldTalkable = NpcTalkableCache.FindRef(NpcTag);
	NpcTalkableCache.Add(NpcTag, bNewTalkable);

	if (!bHadExisting || bOldTalkable != bNewTalkable)
	{
		OnNpcTalkableChanged.Broadcast(NpcTag, bNewTalkable);
	}

	return true;
}

void UARNPCSubsystem::RefreshAllNpcTalkableStates()
{
	UContentLookupSubsystem* Lookup = GetLookupSubsystem(this);
	if (!Lookup)
	{
		return;
	}

	const UARNPCSettings* Settings = GetDefault<UARNPCSettings>();
	const FGameplayTag RootTag = Settings ? Settings->NpcDefinitionRootTag : FGameplayTag();
	if (!RootTag.IsValid())
	{
		return;
	}

	TArray<FName> RowNames;
	FString Error;
	if (!Lookup->GetAllRowNamesForRootTag(RootTag, RowNames, Error))
	{
		UE_LOG(ARLog, Verbose, TEXT("[NPC] RefreshAll talkables failed to fetch rows: %s"), *Error);
		return;
	}

	for (const FName RowName : RowNames)
	{
		FGameplayTag NpcTag;
		if (!BuildNpcDefinitionTagFromRootAndLeaf(RootTag, RowName, NpcTag))
		{
			continue;
		}
		RefreshNpcTalkableState(NpcTag);
	}
}
