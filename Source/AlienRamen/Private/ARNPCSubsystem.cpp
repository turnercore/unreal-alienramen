#include "ARNPCSubsystem.h"

#include "ARDialogueSettings.h"
#include "ARDialogueSubsystem.h"
#include "ARLog.h"
#include "TagContentResolverSubsystem.h"
#include "Engine/GameInstance.h"
#include "GameplayTagsManager.h"

namespace
{
	static UARDialogueSubsystem* GetDialogueSubsystem(const UARNPCSubsystem* Subsystem)
	{
		if (UGameInstance* GI = Subsystem ? Subsystem->GetGameInstance() : nullptr)
		{
			return GI->GetSubsystem<UARDialogueSubsystem>();
		}
		return nullptr;
	}

	static UTagContentResolverSubsystem* GetLookupSubsystem(const UARNPCSubsystem* Subsystem)
	{
		if (UGameInstance* GI = Subsystem ? Subsystem->GetGameInstance() : nullptr)
		{
			return GI->GetSubsystem<UTagContentResolverSubsystem>();
		}
		return nullptr;
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
}

void UARNPCSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UARDialogueSubsystem>();
	NpcTalkableCache.Reset();
}

void UARNPCSubsystem::Deinitialize()
{
	NpcTalkableCache.Reset();
	Super::Deinitialize();
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
	UTagContentResolverSubsystem* Lookup = GetLookupSubsystem(this);
	const UARDialogueSettings* DialogueSettings = GetDefault<UARDialogueSettings>();
	if (!Lookup || !DialogueSettings || !DialogueSettings->SpeakerDefinitionRootTag.IsValid())
	{
		return;
	}

	TArray<FName> RowNames;
	FString Error;
	if (!Lookup->TryGetRowNamesForRootTag(DialogueSettings->SpeakerDefinitionRootTag, RowNames, Error))
	{
		UE_LOG(ARLog, Verbose, TEXT("[NPC] RefreshAll talkables failed to fetch speaker rows: %s"), *Error);
		return;
	}

	for (const FName RowName : RowNames)
	{
		const FGameplayTag SpeakerTag = BuildTagFromRootAndLeaf(DialogueSettings->SpeakerDefinitionRootTag, RowName);
		if (SpeakerTag.IsValid())
		{
			RefreshNpcTalkableState(SpeakerTag);
		}
	}
}

