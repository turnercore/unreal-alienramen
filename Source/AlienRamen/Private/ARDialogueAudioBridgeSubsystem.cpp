#include "ARDialogueAudioBridgeSubsystem.h"

#include "ARDialogueAudioSettings.h"
#include "ARLog.h"
#include "Engine/DataTable.h"
#include "Kismet/GameplayStatics.h"
#include "ParleyDialogueSettings.h"
#include "UObject/Class.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

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

	static UClass* ResolveFMODBlueprintStaticsClass()
	{
		static const TCHAR* ClassPath = TEXT("/Script/FMODStudio.FMODBlueprintStatics");
		if (UClass* FoundClass = FindObject<UClass>(nullptr, ClassPath))
		{
			return FoundClass;
		}

		return LoadObject<UClass>(nullptr, ClassPath);
	}
}

void UARDialogueAudioBridgeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	RebuildCueMapIfNeeded();
}

void UARDialogueAudioBridgeSubsystem::Deinitialize()
{
	UnbindCueTableChangedDelegate();
	DeliveredLineTimeByKey.Reset();
	CueToSignalEventAsset.Reset();
	ResolvedSignalEventByCue.Reset();
	CachedCueTablePath.Reset();
	bCueMapInitialized = false;

	Super::Deinitialize();
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
	UObject* ResolvedEventAsset = nullptr;
	if (!ResolveSignalEventAssetForCue(Request.AudioCueTag, ResolvedEventAsset) || !ResolvedEventAsset)
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

	// FMOD invocation is resolved dynamically (reflection) so this module has no hard FMOD build dependency.
	return PlayFMODEvent2DByReflection(SourceController, ResolvedEventAsset);
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
		UnbindCueTableChangedDelegate();
		CachedCueTablePath.Reset();
		CueToSignalEventAsset.Reset();
		ResolvedSignalEventByCue.Reset();
		bCueMapInitialized = false;
		return false;
	}

	const FSoftObjectPath DesiredTablePath = AudioSettings->DialogueAudioCueFMODTable.ToSoftObjectPath();
	if (bCueMapInitialized && CachedCueTablePath == DesiredTablePath)
	{
		return !CueToSignalEventAsset.IsEmpty();
	}

	UnbindCueTableChangedDelegate();
	CachedCueTablePath = DesiredTablePath;
	CueToSignalEventAsset.Reset();
	ResolvedSignalEventByCue.Reset();
	bCueMapInitialized = true;

	if (!DesiredTablePath.IsValid())
	{
		return false;
	}

	UDataTable* DesiredTable = AudioSettings->DialogueAudioCueFMODTable.LoadSynchronous();
	if (!DesiredTable)
	{
		return false;
	}
	BindToCueTable(DesiredTable);

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

		if (!CueToSignalEventAsset.Contains(Row->AudioCueTag))
		{
			CueToSignalEventAsset.Add(Row->AudioCueTag, Row->FMODEventAsset);
		}

		if (!Row->FMODEventAsset.IsNull())
		{
			if (UObject* PreloadedAsset = Row->FMODEventAsset.LoadSynchronous())
			{
				ResolvedSignalEventByCue.Add(Row->AudioCueTag, PreloadedAsset);
			}
		}
	}

	return !CueToSignalEventAsset.IsEmpty();
}

void UARDialogueAudioBridgeSubsystem::HandleCueTableChanged()
{
	bCueMapInitialized = false;
	UE_LOG(ARLog, Verbose, TEXT("[Dialogue|Audio] Cue mapping table changed; invalidating local cue cache."));
}

void UARDialogueAudioBridgeSubsystem::BindToCueTable(UDataTable* DataTable)
{
	if (!DataTable)
	{
		return;
	}

	BoundCueTable = DataTable;
	CueTableChangedHandle = DataTable->OnDataTableChanged().AddUObject(this, &UARDialogueAudioBridgeSubsystem::HandleCueTableChanged);
}

void UARDialogueAudioBridgeSubsystem::UnbindCueTableChangedDelegate()
{
	if (UDataTable* DataTable = BoundCueTable.Get())
	{
		if (CueTableChangedHandle.IsValid())
		{
			DataTable->OnDataTableChanged().Remove(CueTableChangedHandle);
		}
	}

	CueTableChangedHandle.Reset();
	BoundCueTable.Reset();
}

bool UARDialogueAudioBridgeSubsystem::ResolveSignalEventAssetForCue(const FGameplayTag& CueTag, UObject*& OutEventAsset)
{
	OutEventAsset = nullptr;
	if (!CueTag.IsValid())
	{
		return false;
	}

	RebuildCueMapIfNeeded();
	if (const TWeakObjectPtr<UObject>* CachedLoaded = ResolvedSignalEventByCue.Find(CueTag))
	{
		if (CachedLoaded->IsValid())
		{
			OutEventAsset = CachedLoaded->Get();
			return OutEventAsset != nullptr;
		}
	}

	const TSoftObjectPtr<UObject>* EventRef = CueToSignalEventAsset.Find(CueTag);
	if (!EventRef)
	{
		return false;
	}

	OutEventAsset = EventRef->LoadSynchronous();
	if (!OutEventAsset)
	{
		return false;
	}

	ResolvedSignalEventByCue.Add(CueTag, OutEventAsset);
	return true;
}

bool UARDialogueAudioBridgeSubsystem::PlayFMODEvent2DByReflection(UObject* WorldContextObject, UObject* EventAsset) const
{
	if (!WorldContextObject || !EventAsset)
	{
		return false;
	}

	static const FName PlayEvent2DFunctionName(TEXT("PlayEvent2D"));
	static const FName WorldContextParamName(TEXT("WorldContextObject"));
	static const FName EventParamName(TEXT("Event"));
	static const FName AutoPlayParamName(TEXT("bAutoPlay"));

	UClass* FMODStaticsClass = ResolveFMODBlueprintStaticsClass();
	if (!FMODStaticsClass)
	{
		UE_LOG(ARLog, Verbose, TEXT("[Dialogue|Audio] FMODStudio plugin class not found; signal cue playback skipped."));
		return false;
	}

	UFunction* PlayEvent2DFunction = FMODStaticsClass->FindFunctionByName(PlayEvent2DFunctionName);
	if (!PlayEvent2DFunction)
	{
		UE_LOG(ARLog, Verbose, TEXT("[Dialogue|Audio] FMODBlueprintStatics::PlayEvent2D not found; signal cue playback skipped."));
		return false;
	}

	UObject* FunctionTarget = FMODStaticsClass->GetDefaultObject();
	if (!FunctionTarget)
	{
		return false;
	}

	FStructOnScope Params(PlayEvent2DFunction);
	bool bSetWorldContext = false;
	bool bSetEvent = false;
	for (TFieldIterator<FProperty> It(PlayEvent2DFunction); It && (It->PropertyFlags & CPF_Parm); ++It)
	{
		FProperty* Property = *It;
		if (!Property || Property->HasAnyPropertyFlags(CPF_ReturnParm))
		{
			continue;
		}

		if (FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
		{
			if (Property->GetFName() == WorldContextParamName)
			{
				ObjectProperty->SetObjectPropertyValue_InContainer(Params.GetStructMemory(), WorldContextObject);
				bSetWorldContext = true;
				continue;
			}
			if (Property->GetFName() == EventParamName)
			{
				ObjectProperty->SetObjectPropertyValue_InContainer(Params.GetStructMemory(), EventAsset);
				bSetEvent = true;
				continue;
			}
		}

		if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
		{
			if (Property->GetFName() == AutoPlayParamName)
			{
				BoolProperty->SetPropertyValue_InContainer(Params.GetStructMemory(), true);
			}
		}
	}

	if (!bSetWorldContext || !bSetEvent)
	{
		UE_LOG(ARLog, Warning, TEXT("[Dialogue|Audio] Unable to bind FMOD PlayEvent2D parameters; signal cue playback skipped."));
		return false;
	}

	FunctionTarget->ProcessEvent(PlayEvent2DFunction, Params.GetStructMemory());
	return true;
}
