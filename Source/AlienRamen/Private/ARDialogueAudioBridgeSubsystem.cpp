#include "ARDialogueAudioBridgeSubsystem.h"

#include "ARDialogueAudioSettings.h"
#include "ARLog.h"
#include "FMODBlueprintStatics.h"
#include "FMODEvent.h"
#include "Kismet/GameplayStatics.h"
#include "ParleyDialogueSettings.h"
#include "Engine/DataTable.h"

namespace
{
	static FString BuildDeliveredLineKey(const FDialogueAudioRequest& Request)
	{
		const FString SessionPart = Request.SessionId;
		const FString LinePart = Request.LineGuid.IsValid() ? Request.LineGuid.ToString(EGuidFormats::DigitsWithHyphens) : FString();
		if (SessionPart.IsEmpty() || LinePart.IsEmpty())
		{
			return FString();
		}

		return FString::Printf(TEXT("%s|%s"), *SessionPart, *LinePart);
	}
}

bool UARDialogueAudioBridgeSubsystem::HandleLocalDialogueAudioRequest(APlayerController* SourceController, const FDialogueAudioRequest& Request)
{
	if (!SourceController)
	{
		return false;
	}

	const UParleyDialogueSettings* DialogueSettings = GetDefault<UParleyDialogueSettings>();
	if (!DialogueSettings)
	{
		return false;
	}

	const UARDialogueAudioSettings* AudioSettings = GetDefault<UARDialogueAudioSettings>();
	const double NowSeconds = FPlatformTime::Seconds();
	if (AudioSettings && AudioSettings->LocalLineDedupeWindowSeconds > 0.0f)
	{
		// Requests can arrive once per local participant controller (couch co-op/eavesdrop),
		// but dialogue line audio should play once per machine per delivered line id.
		PruneDeliveredLineCache(NowSeconds);
		if (IsDuplicateLocalLine(Request, NowSeconds))
		{
			return false;
		}
		RecordDeliveredLine(Request, NowSeconds);
	}

	if (DialogueSettings->DialogueAudioMode == EParleyDialogueAudioMode::NativeAudio)
	{
		// In native mode, signal cues are intentionally ignored by this bridge.
		if (!Request.NativeSound)
		{
			return false;
		}

		UGameplayStatics::PlaySound2D(SourceController, Request.NativeSound);
		return true;
	}

	if (!Request.AudioCueTag.IsValid())
	{
		return false;
	}

	// In signal mode, native sound payload is intentionally suppressed by Parley runtime.
	UFMODEvent* ResolvedEvent = nullptr;
	if (!ResolveFMODEventForCue(Request.AudioCueTag, ResolvedEvent) || !ResolvedEvent)
	{
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[Dialogue|Audio] No FMOD mapping for cue '%s' (Session=%s Line=%s)."),
			*Request.AudioCueTag.ToString(),
			*Request.SessionId,
			*Request.LineGuid.ToString(EGuidFormats::DigitsWithHyphensInBraces));
		return false;
	}

	UFMODBlueprintStatics::PlayEvent2D(SourceController, ResolvedEvent, true);
	return true;
}

bool UARDialogueAudioBridgeSubsystem::IsDuplicateLocalLine(const FDialogueAudioRequest& Request, const double NowSeconds) const
{
	const UARDialogueAudioSettings* AudioSettings = GetDefault<UARDialogueAudioSettings>();
	if (!AudioSettings || AudioSettings->LocalLineDedupeWindowSeconds <= 0.0f)
	{
		return false;
	}

	const FString Key = BuildDeliveredLineKey(Request);
	if (Key.IsEmpty())
	{
		return false;
	}

	const double* LastTime = DeliveredLineTimeByKey.Find(Key);
	if (!LastTime)
	{
		return false;
	}

	return (NowSeconds - *LastTime) <= AudioSettings->LocalLineDedupeWindowSeconds;
}

void UARDialogueAudioBridgeSubsystem::RecordDeliveredLine(const FDialogueAudioRequest& Request, const double NowSeconds)
{
	const FString Key = BuildDeliveredLineKey(Request);
	if (Key.IsEmpty())
	{
		return;
	}

	DeliveredLineTimeByKey.Add(Key, NowSeconds);
}

void UARDialogueAudioBridgeSubsystem::PruneDeliveredLineCache(const double NowSeconds)
{
	const UARDialogueAudioSettings* AudioSettings = GetDefault<UARDialogueAudioSettings>();
	const double RetentionSeconds = AudioSettings ? FMath::Max(1.0, AudioSettings->LocalLineDedupeRetentionSeconds) : 6.0;
	const double MinKeepTime = NowSeconds - RetentionSeconds;

	for (auto It = DeliveredLineTimeByKey.CreateIterator(); It; ++It)
	{
		if (It.Value() < MinKeepTime)
		{
			It.RemoveCurrent();
		}
	}
}

bool UARDialogueAudioBridgeSubsystem::RebuildCueMapIfNeeded()
{
	const UARDialogueAudioSettings* AudioSettings = GetDefault<UARDialogueAudioSettings>();
	if (!AudioSettings)
	{
		CachedCueTable.Reset();
		CueToFMODEvent.Reset();
		return false;
	}

	UDataTable* DesiredTable = AudioSettings->DialogueAudioCueFMODTable.LoadSynchronous();
	if (CachedCueTable.Get() == DesiredTable && DesiredTable != nullptr && !CueToFMODEvent.IsEmpty())
	{
		return true;
	}

	CachedCueTable = DesiredTable;
	CueToFMODEvent.Reset();
	if (!DesiredTable)
	{
		return false;
	}

	if (DesiredTable->GetRowStruct() != FARDialogueAudioCueFMODRow::StaticStruct())
	{
		UE_LOG(
			ARLog,
			Warning,
			TEXT("[Dialogue|Audio] DialogueAudioCueFMODTable '%s' row struct mismatch. Expected FARDialogueAudioCueFMODRow."),
			*GetNameSafe(DesiredTable));
		return false;
	}

	for (const FName RowName : DesiredTable->GetRowNames())
	{
		const FARDialogueAudioCueFMODRow* Row = DesiredTable->FindRow<FARDialogueAudioCueFMODRow>(RowName, TEXT("DialogueAudioBridge"), false);
		if (!Row || !Row->AudioCueTag.IsValid())
		{
			continue;
		}

		if (!CueToFMODEvent.Contains(Row->AudioCueTag))
		{
			CueToFMODEvent.Add(Row->AudioCueTag, Row->FMODEventAsset);
		}
	}

	return !CueToFMODEvent.IsEmpty();
}

bool UARDialogueAudioBridgeSubsystem::ResolveFMODEventForCue(const FGameplayTag& CueTag, UFMODEvent*& OutFMODEvent)
{
	OutFMODEvent = nullptr;
	if (!CueTag.IsValid())
	{
		return false;
	}

	RebuildCueMapIfNeeded();
	const TSoftObjectPtr<UFMODEvent>* EventRef = CueToFMODEvent.Find(CueTag);
	if (!EventRef)
	{
		return false;
	}

	OutFMODEvent = EventRef->LoadSynchronous();
	return OutFMODEvent != nullptr;
}
