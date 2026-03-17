#include "ParleySpeakerSubsystem.h"

#include "ParleyDialogueSubsystem.h"
#include "ParleyLog.h"
#include "Engine/GameInstance.h"

namespace
{
	static UParleyDialogueSubsystem* GetDialogueSubsystem(const UParleySpeakerSubsystem* Subsystem)
	{
		if (UGameInstance* GI = Subsystem ? Subsystem->GetGameInstance() : nullptr)
		{
			return GI->GetSubsystem<UParleyDialogueSubsystem>();
		}
		return nullptr;
	}
}

void UParleySpeakerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UParleyDialogueSubsystem>();
	SpeakerTalkableCache.Reset();

	if (UParleyDialogueSubsystem* DialogueSubsystem = GetDialogueSubsystem(this))
	{
		DialogueSubsystem->OnProgressionStateMarkedDirty.AddDynamic(this, &UParleySpeakerSubsystem::HandleDialogueProgressionStateMarkedDirty);
	}
}

void UParleySpeakerSubsystem::Deinitialize()
{
	if (UParleyDialogueSubsystem* DialogueSubsystem = GetDialogueSubsystem(this))
	{
		DialogueSubsystem->OnProgressionStateMarkedDirty.RemoveDynamic(this, &UParleySpeakerSubsystem::HandleDialogueProgressionStateMarkedDirty);
	}

	SpeakerTalkableCache.Reset();
	Super::Deinitialize();
}

bool UParleySpeakerSubsystem::IsSpeakerTalkable(FGameplayTag SpeakerTag) const
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

bool UParleySpeakerSubsystem::RefreshSpeakerTalkableState(FGameplayTag SpeakerTag)
{
	if (!SpeakerTag.IsValid())
	{
		UE_LOG(ParleyLog, Verbose, TEXT("[Speaker] Talkable refresh skipped: invalid speaker tag."));
		return false;
	}

	UParleyDialogueSubsystem* DialogueSubsystem = GetDialogueSubsystem(this);
	if (!DialogueSubsystem)
	{
		UE_LOG(ParleyLog, Verbose, TEXT("[Speaker] Talkable refresh skipped for '%s': dialogue subsystem unavailable."), *SpeakerTag.ToString());
		return false;
	}

	const bool bNewTalkable = DialogueSubsystem->HasUnlockedDialogueForSpeakerForAnyPlayer(SpeakerTag);
	const bool bHadExisting = SpeakerTalkableCache.Contains(SpeakerTag);
	const bool bOldTalkable = SpeakerTalkableCache.FindRef(SpeakerTag);
	SpeakerTalkableCache.Add(SpeakerTag, bNewTalkable);

	UE_LOG(
		ParleyLog,
		Verbose,
		TEXT("[Speaker] Talkable refresh '%s': New=%s Old=%s HadCache=%s"),
		*SpeakerTag.ToString(),
		bNewTalkable ? TEXT("true") : TEXT("false"),
		bOldTalkable ? TEXT("true") : TEXT("false"),
		bHadExisting ? TEXT("true") : TEXT("false"));

	if (!bHadExisting || bOldTalkable != bNewTalkable)
	{
		UE_LOG(ParleyLog, Verbose, TEXT("[Speaker] Talkable changed '%s' -> %s; broadcasting."), *SpeakerTag.ToString(), bNewTalkable ? TEXT("true") : TEXT("false"));
		OnSpeakerTalkableChanged.Broadcast(SpeakerTag, bNewTalkable);
	}

	return true;
}

void UParleySpeakerSubsystem::RefreshAllSpeakerTalkableStates()
{
	UParleyDialogueSubsystem* DialogueSubsystem = GetDialogueSubsystem(this);
	if (!DialogueSubsystem)
	{
		UE_LOG(ParleyLog, Verbose, TEXT("[Speaker] RefreshAll talkables skipped: dialogue subsystem unavailable."));
		return;
	}

	TArray<FGameplayTag> SpeakerTags;
	DialogueSubsystem->GetRegisteredPrimarySpeakerTags(SpeakerTags);
	if (SpeakerTags.IsEmpty())
	{
		UE_LOG(ParleyLog, Verbose, TEXT("[Speaker] RefreshAll talkables: no registered dialogue speaker tags."));
		return;
	}

	UE_LOG(ParleyLog, Verbose, TEXT("[Speaker] RefreshAll talkables: evaluating %d registered dialogue speaker tags."), SpeakerTags.Num());
	for (const FGameplayTag& SpeakerTag : SpeakerTags)
	{
		if (SpeakerTag.IsValid())
		{
			RefreshSpeakerTalkableState(SpeakerTag);
		}
	}
}

void UParleySpeakerSubsystem::HandleDialogueProgressionStateMarkedDirty()
{
	RefreshAllSpeakerTalkableStates();
}
