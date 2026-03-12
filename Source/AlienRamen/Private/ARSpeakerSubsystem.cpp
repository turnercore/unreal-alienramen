#include "ARSpeakerSubsystem.h"

#include "ARDialogueSubsystem.h"
#include "ARLog.h"
#include "Engine/GameInstance.h"

namespace
{
	static UARDialogueSubsystem* GetDialogueSubsystem(const UARSpeakerSubsystem* Subsystem)
	{
		if (UGameInstance* GI = Subsystem ? Subsystem->GetGameInstance() : nullptr)
		{
			return GI->GetSubsystem<UARDialogueSubsystem>();
		}
		return nullptr;
	}
}

void UARSpeakerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UARDialogueSubsystem>();
	SpeakerTalkableCache.Reset();
}

void UARSpeakerSubsystem::Deinitialize()
{
	SpeakerTalkableCache.Reset();
	Super::Deinitialize();
}

bool UARSpeakerSubsystem::IsSpeakerTalkable(FGameplayTag SpeakerTag) const
{
	if (!SpeakerTag.IsValid())
	{
		return false;
	}

	if (const bool* Cached = SpeakerTalkableCache.Find(SpeakerTag))
	{
		return *Cached;
	}

	return false;
}

bool UARSpeakerSubsystem::RefreshSpeakerTalkableState(FGameplayTag SpeakerTag)
{
	if (!SpeakerTag.IsValid())
	{
		UE_LOG(ARLog, Verbose, TEXT("[Speaker] Talkable refresh skipped: invalid speaker tag."));
		return false;
	}

	UARDialogueSubsystem* DialogueSubsystem = GetDialogueSubsystem(this);
	if (!DialogueSubsystem)
	{
		UE_LOG(ARLog, Verbose, TEXT("[Speaker] Talkable refresh skipped for '%s': dialogue subsystem unavailable."), *SpeakerTag.ToString());
		return false;
	}

	const bool bNewTalkable = DialogueSubsystem->HasUnlockedDialogueForSpeakerForAnyPlayer(SpeakerTag);
	const bool bHadExisting = SpeakerTalkableCache.Contains(SpeakerTag);
	const bool bOldTalkable = SpeakerTalkableCache.FindRef(SpeakerTag);
	SpeakerTalkableCache.Add(SpeakerTag, bNewTalkable);

	UE_LOG(
		ARLog,
		Verbose,
		TEXT("[Speaker] Talkable refresh '%s': New=%s Old=%s HadCache=%s"),
		*SpeakerTag.ToString(),
		bNewTalkable ? TEXT("true") : TEXT("false"),
		bOldTalkable ? TEXT("true") : TEXT("false"),
		bHadExisting ? TEXT("true") : TEXT("false"));

	if (!bHadExisting || bOldTalkable != bNewTalkable)
	{
		UE_LOG(ARLog, Verbose, TEXT("[Speaker] Talkable changed '%s' -> %s; broadcasting."), *SpeakerTag.ToString(), bNewTalkable ? TEXT("true") : TEXT("false"));
		OnSpeakerTalkableChanged.Broadcast(SpeakerTag, bNewTalkable);
	}

	return true;
}

void UARSpeakerSubsystem::RefreshAllSpeakerTalkableStates()
{
	UARDialogueSubsystem* DialogueSubsystem = GetDialogueSubsystem(this);
	if (!DialogueSubsystem)
	{
		UE_LOG(ARLog, Verbose, TEXT("[Speaker] RefreshAll talkables skipped: dialogue subsystem unavailable."));
		return;
	}

	TArray<FGameplayTag> SpeakerTags;
	DialogueSubsystem->GetRegisteredPrimarySpeakerTags(SpeakerTags);
	if (SpeakerTags.IsEmpty())
	{
		UE_LOG(ARLog, Verbose, TEXT("[Speaker] RefreshAll talkables: no registered dialogue speaker tags."));
		return;
	}

	UE_LOG(ARLog, Verbose, TEXT("[Speaker] RefreshAll talkables: evaluating %d registered dialogue speaker tags."), SpeakerTags.Num());
	for (const FGameplayTag& SpeakerTag : SpeakerTags)
	{
		if (SpeakerTag.IsValid())
		{
			RefreshSpeakerTalkableState(SpeakerTag);
		}
	}
}

