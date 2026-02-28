#include "ARSaveSubsystem.h"

#include "ARGameStateBase.h"
#include "ARLog.h"
#include "ARPlayerController.h"
#include "ARPlayerStateBase.h"
#include "ARSaveGame.h"
#include "ARSaveIndexGame.h"
#include "ARSaveUserSettings.h"
#include "Kismet/GameplayStatics.h"
#include "StructSerializable.h"

namespace ARSaveInternal
{
	static const TCHAR* SaveIndexSlot = TEXT("SaveIndex");
	static const TCHAR* DebugSaveIndexSlot = TEXT("SaveIndexDebug");
	static const TCHAR* SlotAdj[] = {
		TEXT("Spicy"), TEXT("Neon"), TEXT("Corporate"), TEXT("Fermented"), TEXT("Unpaid"), TEXT("Galactic"),
		TEXT("Suspicious"), TEXT("Nuclear"), TEXT("Chaotic"), TEXT("Certified"), TEXT("Questionable"), TEXT("Overclocked")
	};
	static const TCHAR* SlotNoun[] = {
		TEXT("Ramen"), TEXT("Invader"), TEXT("Noodle"), TEXT("Colony"), TEXT("Dumpling"), TEXT("Broth"),
		TEXT("MegaCorp"), TEXT("Franchise"), TEXT("Saucer"), TEXT("Payroll"), TEXT("Kiosk"), TEXT("Meatball")
	};
	static const TCHAR* SlotTail[] = {
		TEXT("Incident"), TEXT("Ledger"), TEXT("Catastrophe"), TEXT("Protocol"), TEXT("Shift"), TEXT("Experiment"),
		TEXT("Merger"), TEXT("Lunch"), TEXT("Outbreak"), TEXT("Heist"), TEXT("Audit"), TEXT("Expansion")
	};

	static bool TryReadGameplayTagContainerField(const UObject* Object, const TCHAR* FieldPrefix, FGameplayTagContainer& OutValue)
	{
		if (!Object)
		{
			return false;
		}

		for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
		{
			if (const FStructProperty* StructProp = CastField<FStructProperty>(*It))
			{
				if (!StructProp->GetName().StartsWith(FieldPrefix, ESearchCase::IgnoreCase))
				{
					continue;
				}
				if (StructProp->Struct != FGameplayTagContainer::StaticStruct())
				{
					continue;
				}

				if (const FGameplayTagContainer* ValuePtr = StructProp->ContainerPtrToValuePtr<FGameplayTagContainer>(Object))
				{
					OutValue = *ValuePtr;
					return true;
				}
			}
		}

		return false;
	}

	static bool TryReadIntField(const UObject* Object, const TCHAR* FieldPrefix, int32& OutValue)
	{
		if (!Object)
		{
			return false;
		}

		for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
		{
			if (const FIntProperty* IntProp = CastField<FIntProperty>(*It))
			{
				if (!IntProp->GetName().StartsWith(FieldPrefix, ESearchCase::IgnoreCase))
				{
					continue;
				}
				if (const int32* ValuePtr = IntProp->ContainerPtrToValuePtr<int32>(Object))
				{
					OutValue = *ValuePtr;
					return true;
				}
			}
		}
		return false;
	}

}

void UARSaveSubsystem::Deinitialize()
{
	CurrentSaveGame = nullptr;
	CurrentSlotBaseName = NAME_None;
	PendingCanonicalSyncRequests.Reset();
	PendingTravelGameStateData.Reset();
	Super::Deinitialize();
}

FName UARSaveSubsystem::NormalizeSlotBaseName(FName SlotBaseName)
{
	FString Slot = SlotBaseName.ToString().TrimStartAndEnd();
	if (Slot.IsEmpty())
	{
		Slot = TEXT("Save");
	}
	return FName(*Slot);
}

FName UARSaveSubsystem::GenerateRandomSlotBaseName(const bool bEnsureUnique)
{
	constexpr int32 MaxAttempts = 64;
	TSet<FName> ExistingNames;
	TSet<FName> ExistingRevisionZeroSlots;

	if (bEnsureUnique)
	{
		FARSaveResult IndexResult;
		UARSaveIndexGame* IndexObj = nullptr;
		if (LoadOrCreateIndex(IndexObj, IndexResult) && IndexObj)
		{
			for (const FARSaveSlotDescriptor& Entry : IndexObj->SlotNames)
			{
				ExistingNames.Add(Entry.SlotName);
				ExistingRevisionZeroSlots.Add(BuildRevisionSlotName(Entry.SlotName, 0));
			}
		}
	}

	for (int32 Attempt = 0; Attempt < MaxAttempts; ++Attempt)
	{
		const TCHAR* Adj = ARSaveInternal::SlotAdj[FMath::RandRange(0, UE_ARRAY_COUNT(ARSaveInternal::SlotAdj) - 1)];
		const TCHAR* Noun = ARSaveInternal::SlotNoun[FMath::RandRange(0, UE_ARRAY_COUNT(ARSaveInternal::SlotNoun) - 1)];
		const TCHAR* Tail = ARSaveInternal::SlotTail[FMath::RandRange(0, UE_ARRAY_COUNT(ARSaveInternal::SlotTail) - 1)];
		const int32 Ticket = FMath::RandRange(10, 9999);
		const FName Candidate = FName(*FString::Printf(TEXT("%s_%s_%s_%d"), Adj, Noun, Tail, Ticket));
		const FName CandidateRevisionZero = BuildRevisionSlotName(Candidate, 0);
		const bool bNameTaken = ExistingNames.Contains(Candidate);
		const bool bPhysicalSlotTaken = ExistingRevisionZeroSlots.Contains(CandidateRevisionZero)
			|| UGameplayStatics::DoesSaveGameExist(CandidateRevisionZero.ToString(), DefaultUserIndex);
		if (!bEnsureUnique || (!bNameTaken && !bPhysicalSlotTaken))
		{
			return Candidate;
		}
	}

	// Last resort: guaranteed uniqueness.
	return FName(*FString::Printf(TEXT("Save_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(10)));
}

int32 UARSaveSubsystem::GetMaxBackupRevisions() const
{
	const UARSaveUserSettings* Settings = GetDefault<UARSaveUserSettings>();
	return Settings ? FMath::Clamp(Settings->MaxBackupRevisions, 1, 100) : 5;
}

int32 UARSaveSubsystem::GetCurrentSlotRevision() const
{
	return CurrentSaveGame ? CurrentSaveGame->SaveSlotNumber : INDEX_NONE;
}

void UARSaveSubsystem::SetMaxBackupRevisions(int32 NewMaxBackups)
{
	UARSaveUserSettings* Settings = GetMutableDefault<UARSaveUserSettings>();
	if (!Settings)
	{
		return;
	}

	Settings->MaxBackupRevisions = FMath::Clamp(NewMaxBackups, 1, 100);
	Settings->SaveConfig();
}

FName UARSaveSubsystem::BuildRevisionSlotName(FName SlotBaseName, int32 SlotNumber)
{
	return FName(*FString::Printf(TEXT("%s__%d"), *SlotBaseName.ToString(), SlotNumber));
}

bool UARSaveSubsystem::TrySplitRevisionSlotName(const FString& InSlotName, FString& OutBaseSlotName, int32& OutSlotNumber)
{
	OutBaseSlotName.Reset();
	OutSlotNumber = 0;

	const int32 DelimIndex = InSlotName.Find(TEXT("__"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
	if (DelimIndex == INDEX_NONE)
	{
		return false;
	}

	const FString Base = InSlotName.Left(DelimIndex);
	const FString Suffix = InSlotName.Mid(DelimIndex + 2);
	if (Base.IsEmpty() || !Suffix.IsNumeric())
	{
		return false;
	}

	OutBaseSlotName = Base;
	OutSlotNumber = FCString::Atoi(*Suffix);
	return true;
}

bool UARSaveSubsystem::LoadOrCreateIndexForSlot(UARSaveIndexGame*& OutIndex, FARSaveResult& OutResult, const TCHAR* IndexSlotName) const
{
	OutIndex = nullptr;
	const FString SlotName = IndexSlotName;
	if (UGameplayStatics::DoesSaveGameExist(SlotName, DefaultUserIndex))
	{
		OutIndex = Cast<UARSaveIndexGame>(UGameplayStatics::LoadGameFromSlot(SlotName, DefaultUserIndex));
		if (!OutIndex)
		{
			OutResult.Error = FString::Printf(TEXT("Failed to load C++ save index '%s'."), *SlotName);
			return false;
		}
		return true;
	}

	OutIndex = Cast<UARSaveIndexGame>(UGameplayStatics::CreateSaveGameObject(UARSaveIndexGame::StaticClass()));
	if (!OutIndex)
	{
		OutResult.Error = FString::Printf(TEXT("Failed to create C++ save index object '%s'."), *SlotName);
		return false;
	}
	return SaveIndexForSlot(OutIndex, OutResult, IndexSlotName);
}

bool UARSaveSubsystem::SaveIndexForSlot(UARSaveIndexGame* IndexObj, FARSaveResult& OutResult, const TCHAR* IndexSlotName) const
{
	if (!IndexObj)
	{
		OutResult.Error = TEXT("Save index is null.");
		return false;
	}
	if (!UGameplayStatics::SaveGameToSlot(IndexObj, IndexSlotName, DefaultUserIndex))
	{
		OutResult.Error = FString::Printf(TEXT("Failed to save C++ save index '%s'."), IndexSlotName);
		return false;
	}
	return true;
}

bool UARSaveSubsystem::LoadOrCreateIndex(UARSaveIndexGame*& OutIndex, FARSaveResult& OutResult) const
{
	return LoadOrCreateIndexForSlot(OutIndex, OutResult, ARSaveInternal::SaveIndexSlot);
}

bool UARSaveSubsystem::SaveIndex(UARSaveIndexGame* IndexObj, FARSaveResult& OutResult) const
{
	return SaveIndexForSlot(IndexObj, OutResult, ARSaveInternal::SaveIndexSlot);
}

bool UARSaveSubsystem::SaveSaveObject(UARSaveGame* SaveObject, FName SlotBaseName, int32 SlotNumber, FARSaveResult& OutResult) const
{
	if (!SaveObject)
	{
		OutResult.Error = TEXT("Save object is null.");
		return false;
	}
	const FName RevisionSlot = BuildRevisionSlotName(SlotBaseName, SlotNumber);
	if (!UGameplayStatics::SaveGameToSlot(SaveObject, RevisionSlot.ToString(), DefaultUserIndex))
	{
		OutResult.Error = FString::Printf(TEXT("Failed to write save slot '%s'."), *RevisionSlot.ToString());
		return false;
	}
	return true;
}

void UARSaveSubsystem::PruneOldRevisions(FName SlotBaseName, int32 LatestRevision) const
{
	const int32 MaxBackups = GetMaxBackupRevisions();
	const int32 FirstRevisionToKeep = FMath::Max(0, LatestRevision - (MaxBackups - 1));
	for (int32 Revision = 0; Revision < FirstRevisionToKeep; ++Revision)
	{
		const FName RevisionSlotName = BuildRevisionSlotName(SlotBaseName, Revision);
		if (UGameplayStatics::DoesSaveGameExist(RevisionSlotName.ToString(), DefaultUserIndex))
		{
			UGameplayStatics::DeleteGameInSlot(RevisionSlotName.ToString(), DefaultUserIndex);
		}
	}
}

int32 UARSaveSubsystem::UpsertIndexEntry(UARSaveIndexGame* IndexObj, const FARSaveSlotDescriptor& Descriptor) const
{
	if (!IndexObj)
	{
		return INDEX_NONE;
	}

	for (int32 i = 0; i < IndexObj->SlotNames.Num(); ++i)
	{
		if (IndexObj->SlotNames[i].SlotName == Descriptor.SlotName)
		{
			IndexObj->SlotNames[i] = Descriptor;
			return i;
		}
	}

	return IndexObj->SlotNames.Add(Descriptor);
}

bool UARSaveSubsystem::RemoveIndexEntry(UARSaveIndexGame* IndexObj, FName SlotBaseName) const
{
	if (!IndexObj)
	{
		return false;
	}

	IndexObj->SlotNames.RemoveAll([SlotBaseName](const FARSaveSlotDescriptor& Entry)
	{
		return Entry.SlotName == SlotBaseName;
	});
	return true;
}

void UARSaveSubsystem::GatherRuntimeData(UARSaveGame* SaveObject)
{
	if (!SaveObject)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	AARGameStateBase* GS = World->GetGameState<AARGameStateBase>();
	if (GS && GS->GetClass()->ImplementsInterface(UStructSerializable::StaticClass()))
	{
		FInstancedStruct GSState;
		IStructSerializable::Execute_ExtractStateToStruct(GS, GSState);
		SaveObject->GameStateStruct.GameStateData = GSState;

		ARSaveInternal::TryReadGameplayTagContainerField(GS, TEXT("SeenDialogue"), SaveObject->SeenDialogue);
		ARSaveInternal::TryReadGameplayTagContainerField(GS, TEXT("DialogueFlags"), SaveObject->DialogueFlags);
		ARSaveInternal::TryReadGameplayTagContainerField(GS, TEXT("Unlocks"), SaveObject->Unlocks);
		ARSaveInternal::TryReadGameplayTagContainerField(GS, TEXT("Choices"), SaveObject->Choices);
		ARSaveInternal::TryReadIntField(GS, TEXT("Money"), SaveObject->Money);
		ARSaveInternal::TryReadIntField(GS, TEXT("Material"), SaveObject->Material);
		ARSaveInternal::TryReadIntField(GS, TEXT("Cycles"), SaveObject->Cycles);
	}

	SaveObject->PlayerStates.Reset();
	if (!GS)
	{
		UE_LOG(ARLog, Warning, TEXT("[SaveSubsystem] GatherRuntimeData skipped PlayerStates capture: no GameState in world '%s'."), *GetNameSafe(World));
		return;
	}

	for (APlayerState* PS : GS->PlayerArray)
	{
		AARPlayerStateBase* ARPS = Cast<AARPlayerStateBase>(PS);
		if (!ARPS)
		{
			continue;
		}

		FARPlayerStateSaveData PlayerData;
		PlayerData.Identity.LegacyId = ARPS->GetPlayerId();
		PlayerData.Identity.DisplayName = FText::FromString(ARPS->GetDisplayNameValue());
		PlayerData.Identity.PlayerSlot = ARPS->GetPlayerSlot();
		if (PS->GetUniqueId().IsValid())
		{
			PlayerData.Identity.UniqueNetIdString = PS->GetUniqueId()->ToString();
		}
		PlayerData.LoadoutTags = ARPS->LoadoutTags;
		PlayerData.CharacterPicked = ARPS->GetCharacterPicked();

		if (ARPS->GetClass()->ImplementsInterface(UStructSerializable::StaticClass()))
		{
			FInstancedStruct StateData;
			IStructSerializable::Execute_ExtractStateToStruct(ARPS, StateData);
			PlayerData.PlayerStateData = StateData;
		}

		SaveObject->PlayerStates.Add(MoveTemp(PlayerData));
	}
}

UARSaveGame* UARSaveSubsystem::LoadSaveObjectWithRollback(FName SlotBaseName, int32 RevisionOrLatest, int32& OutResolvedSlotNumber, FARSaveResult& OutResult) const
{
	OutResolvedSlotNumber = INDEX_NONE;

	UARSaveIndexGame* IndexObj = nullptr;
	FARSaveResult IndexResult;
	if (!LoadOrCreateIndex(IndexObj, IndexResult))
	{
		OutResult = IndexResult;
		return nullptr;
	}

	int32 Latest = -1;
	for (const FARSaveSlotDescriptor& Entry : IndexObj->SlotNames)
	{
		if (Entry.SlotName == SlotBaseName)
		{
			Latest = Entry.SlotNumber;
			break;
		}
	}
	if (Latest < 0)
	{
		OutResult.Error = FString::Printf(TEXT("Save slot '%s' not found in index."), *SlotBaseName.ToString());
		return nullptr;
	}

	int32 StartRevision = RevisionOrLatest;
	if (StartRevision < 0)
	{
		StartRevision = Latest;
	}

	for (int32 Revision = StartRevision; Revision >= 0; --Revision)
	{
		const FName RevisionSlotName = BuildRevisionSlotName(SlotBaseName, Revision);
		if (!UGameplayStatics::DoesSaveGameExist(RevisionSlotName.ToString(), DefaultUserIndex))
		{
			continue;
		}

		if (UARSaveGame* Loaded = Cast<UARSaveGame>(UGameplayStatics::LoadGameFromSlot(RevisionSlotName.ToString(), DefaultUserIndex)))
		{
			OutResolvedSlotNumber = Revision;
			return Loaded;
		}
	}

	OutResult.Error = FString::Printf(TEXT("Failed to load '%s' and all previous revisions."), *SlotBaseName.ToString());
	return nullptr;
}

bool UARSaveSubsystem::CreateNewSave(FName DesiredSlotBase, FARSaveSlotDescriptor& OutSlot, FARSaveResult& OutResult)
{
	OutResult = FARSaveResult();

	UARSaveIndexGame* IndexObj = nullptr;
	if (!LoadOrCreateIndex(IndexObj, OutResult))
	{
		BroadcastSaveFailure(OutResult);
		return false;
	}

	const FName SlotBase = DesiredSlotBase.IsNone() ? GenerateRandomSlotBaseName(true) : NormalizeSlotBaseName(DesiredSlotBase);
	for (const FARSaveSlotDescriptor& Entry : IndexObj->SlotNames)
	{
		if (Entry.SlotName == SlotBase)
		{
			OutResult.Error = FString::Printf(TEXT("Save slot '%s' already exists."), *SlotBase.ToString());
			BroadcastSaveFailure(OutResult);
			return false;
		}
	}

	UARSaveGame* NewSave = Cast<UARSaveGame>(UGameplayStatics::CreateSaveGameObject(UARSaveGame::StaticClass()));
	if (!NewSave)
	{
		OutResult.Error = TEXT("Failed to create UARSaveGame.");
		BroadcastSaveFailure(OutResult);
		return false;
	}

	GatherRuntimeData(NewSave);
	NewSave->SaveSlot = SlotBase;
	NewSave->SaveSlotNumber = 0;
	NewSave->SaveGameVersion = UARSaveGame::GetCurrentSchemaVersion();
	NewSave->LastSaved = FDateTime::UtcNow();
	OutResult.ClampedFieldCount = NewSave->ValidateAndSanitize(nullptr);

	if (!SaveSaveObject(NewSave, SlotBase, 0, OutResult))
	{
		BroadcastSaveFailure(OutResult);
		return false;
	}

	FARSaveSlotDescriptor Descriptor;
	Descriptor.SlotName = SlotBase;
	Descriptor.SlotNumber = 0;
	Descriptor.SaveVersion = NewSave->SaveGameVersion;
	Descriptor.CyclesPlayed = NewSave->Cycles;
	Descriptor.LastSavedTime = NewSave->LastSaved;
	Descriptor.Money = NewSave->Money;
	UpsertIndexEntry(IndexObj, Descriptor);

	if (!SaveIndex(IndexObj, OutResult))
	{
		BroadcastSaveFailure(OutResult);
		return false;
	}

	CurrentSaveGame = NewSave;
	CurrentSlotBaseName = SlotBase;
	OutSlot = Descriptor;
	OutResult.bSuccess = true;
	OutResult.SlotName = SlotBase;
	OutResult.SlotNumber = 0;
	OnSaveCompleted.Broadcast(OutResult);
	return true;
}

bool UARSaveSubsystem::PersistCanonicalSaveFromBytes(const TArray<uint8>& SaveBytes, FName SlotBaseName, int32 SlotNumber, FARSaveResult& OutResult)
{
	OutResult = FARSaveResult();
	if (SaveBytes.Num() == 0)
	{
		OutResult.Error = TEXT("Canonical save bytes are empty.");
		return false;
	}

	UARSaveGame* SaveObject = Cast<UARSaveGame>(UGameplayStatics::LoadGameFromMemory(SaveBytes));
	if (!SaveObject)
	{
		OutResult.Error = TEXT("Failed to deserialize canonical save bytes.");
		return false;
	}

	if (!SaveSaveObject(SaveObject, SlotBaseName, SlotNumber, OutResult))
	{
		return false;
	}

	UARSaveIndexGame* IndexObj = nullptr;
	if (!LoadOrCreateIndex(IndexObj, OutResult))
	{
		return false;
	}

	FARSaveSlotDescriptor Descriptor;
	Descriptor.SlotName = SlotBaseName;
	Descriptor.SlotNumber = SlotNumber;
	Descriptor.SaveVersion = SaveObject->SaveGameVersion;
	Descriptor.CyclesPlayed = SaveObject->Cycles;
	Descriptor.LastSavedTime = SaveObject->LastSaved;
	Descriptor.Money = SaveObject->Money;
	UpsertIndexEntry(IndexObj, Descriptor);
	if (!SaveIndex(IndexObj, OutResult))
	{
		return false;
	}
	PruneOldRevisions(SlotBaseName, SlotNumber);

	CurrentSaveGame = SaveObject;
	CurrentSlotBaseName = SlotBaseName;
	OutResult.bSuccess = true;
	OutResult.SlotName = SlotBaseName;
	OutResult.SlotNumber = SlotNumber;
	return true;
}

bool UARSaveSubsystem::SaveCurrentGame(FName SlotBaseName, bool bCreateNewRevision, FARSaveResult& OutResult)
{
	OutResult = FARSaveResult();

	UWorld* World = GetWorld();
	if (!World)
	{
		OutResult.Error = TEXT("No world available for save.");
		BroadcastSaveFailure(OutResult);
		return false;
	}

	if (World->GetNetMode() != NM_Standalone && World->GetAuthGameMode() == nullptr)
	{
		OutResult.Error = TEXT("SaveCurrentGame must run on authority/server for canonical snapshot.");
		BroadcastSaveFailure(OutResult);
		return false;
	}

	UARSaveIndexGame* IndexObj = nullptr;
	if (!LoadOrCreateIndex(IndexObj, OutResult))
	{
		BroadcastSaveFailure(OutResult);
		return false;
	}

	FName SlotBase = SlotBaseName.IsNone() ? NAME_None : NormalizeSlotBaseName(SlotBaseName);
	if (SlotBase.IsNone())
	{
		SlotBase = CurrentSlotBaseName;
	}
	if (SlotBase.IsNone())
	{
		SlotBase = GenerateRandomSlotBaseName(true);
	}

	int32 ExistingLatest = -1;
	for (const FARSaveSlotDescriptor& Entry : IndexObj->SlotNames)
	{
		if (Entry.SlotName == SlotBase)
		{
			ExistingLatest = Entry.SlotNumber;
			break;
		}
	}

	int32 NewSlotNumber = 0;
	if (ExistingLatest >= 0)
	{
		NewSlotNumber = bCreateNewRevision ? ExistingLatest + 1 : ExistingLatest;
	}

	UARSaveGame* SaveObject = Cast<UARSaveGame>(UGameplayStatics::CreateSaveGameObject(UARSaveGame::StaticClass()));
	if (!SaveObject)
	{
		OutResult.Error = TEXT("Failed to allocate UARSaveGame.");
		BroadcastSaveFailure(OutResult);
		return false;
	}

	GatherRuntimeData(SaveObject);
	SaveObject->SaveSlot = SlotBase;
	SaveObject->SaveSlotNumber = NewSlotNumber;
	SaveObject->SaveGameVersion = UARSaveGame::GetCurrentSchemaVersion();
	SaveObject->LastSaved = FDateTime::UtcNow();

	TArray<FString> Warnings;
	OutResult.ClampedFieldCount = SaveObject->ValidateAndSanitize(&Warnings);
	for (const FString& Warning : Warnings)
	{
		UE_LOG(ARLog, Warning, TEXT("[SaveSubsystem] %s"), *Warning);
	}

	if (!SaveSaveObject(SaveObject, SlotBase, NewSlotNumber, OutResult))
	{
		BroadcastSaveFailure(OutResult);
		return false;
	}

	FARSaveSlotDescriptor Descriptor;
	Descriptor.SlotName = SlotBase;
	Descriptor.SlotNumber = NewSlotNumber;
	Descriptor.SaveVersion = SaveObject->SaveGameVersion;
	Descriptor.CyclesPlayed = SaveObject->Cycles;
	Descriptor.LastSavedTime = SaveObject->LastSaved;
	Descriptor.Money = SaveObject->Money;
	UpsertIndexEntry(IndexObj, Descriptor);

	if (!SaveIndex(IndexObj, OutResult))
	{
		BroadcastSaveFailure(OutResult);
		return false;
	}
	PruneOldRevisions(SlotBase, NewSlotNumber);

	// Distribute canonical save to clients so each machine persists equivalent snapshot.
	TArray<uint8> SaveBytes;
	if (UGameplayStatics::SaveGameToMemory(SaveObject, SaveBytes))
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			if (AARPlayerController* PC = Cast<AARPlayerController>(It->Get()))
			{
				if (PC->GetNetMode() != NM_Standalone && !PC->IsLocalController())
				{
					PC->ClientPersistCanonicalSave(SaveBytes, SlotBase, NewSlotNumber);
				}
			}
		}
	}

	CurrentSaveGame = SaveObject;
	CurrentSlotBaseName = SlotBase;
	FlushPendingCanonicalSyncRequests();
	OutResult.bSuccess = true;
	OutResult.SlotName = SlotBase;
	OutResult.SlotNumber = NewSlotNumber;
	OnSaveCompleted.Broadcast(OutResult);
	return true;
}

bool UARSaveSubsystem::LoadGame(FName SlotBaseName, int32 RevisionOrLatest, FARSaveResult& OutResult)
{
	OutResult = FARSaveResult();
	const FName SlotBase = NormalizeSlotBaseName(SlotBaseName);

	int32 ResolvedRevision = INDEX_NONE;
	UARSaveGame* LoadedSave = LoadSaveObjectWithRollback(SlotBase, RevisionOrLatest, ResolvedRevision, OutResult);
	if (!LoadedSave)
	{
		BroadcastLoadFailure(OutResult);
		return false;
	}

	if (!UARSaveGame::IsSchemaVersionSupported(LoadedSave->SaveGameVersion))
	{
		OutResult.Error = FString::Printf(
			TEXT("Save schema version %d is unsupported (supported range: %d..%d)."),
			LoadedSave->SaveGameVersion,
			UARSaveGame::GetMinSupportedSchemaVersion(),
			UARSaveGame::GetCurrentSchemaVersion());
		BroadcastLoadFailure(OutResult);
		return false;
	}

	if (LoadedSave->SaveGameVersion < UARSaveGame::GetCurrentSchemaVersion())
	{
		UE_LOG(
			ARLog,
			Warning,
			TEXT("[SaveSubsystem] Loaded older save schema version %d (current %d). Migration path not implemented yet."),
			LoadedSave->SaveGameVersion,
			UARSaveGame::GetCurrentSchemaVersion());
	}

	OutResult.bSuccess = true;
	OutResult.SlotName = SlotBase;
	OutResult.SlotNumber = ResolvedRevision;
	ApplyLoadedSave(LoadedSave, OutResult);
	OnLoadCompleted.Broadcast(OutResult);
	OnGameLoaded.Broadcast();
	return true;
}

bool UARSaveSubsystem::ListSaves(TArray<FARSaveSlotDescriptor>& OutSlots, FARSaveResult& OutResult) const
{
	OutSlots.Reset();
	OutResult = FARSaveResult();

	UARSaveIndexGame* IndexObj = nullptr;
	if (!LoadOrCreateIndex(IndexObj, OutResult))
	{
		return false;
	}

	OutSlots = IndexObj->SlotNames;
	OutResult.bSuccess = true;
	return true;
}

bool UARSaveSubsystem::ListDebugSaves(TArray<FARSaveSlotDescriptor>& OutSlots, FARSaveResult& OutResult) const
{
	OutSlots.Reset();
	OutResult = FARSaveResult();

	UARSaveIndexGame* IndexObj = nullptr;
	if (!LoadOrCreateIndexForSlot(IndexObj, OutResult, ARSaveInternal::DebugSaveIndexSlot))
	{
		return false;
	}

	OutSlots = IndexObj->SlotNames;
	OutResult.bSuccess = true;
	return true;
}

bool UARSaveSubsystem::DeleteSave(FName SlotBaseName, FARSaveResult& OutResult)
{
	OutResult = FARSaveResult();
	const FName SlotBase = NormalizeSlotBaseName(SlotBaseName);

	UARSaveIndexGame* IndexObj = nullptr;
	if (!LoadOrCreateIndex(IndexObj, OutResult))
	{
		BroadcastSaveFailure(OutResult);
		return false;
	}

	int32 MaxRevision = -1;
	for (const FARSaveSlotDescriptor& Entry : IndexObj->SlotNames)
	{
		if (Entry.SlotName == SlotBase)
		{
			MaxRevision = Entry.SlotNumber;
			break;
		}
	}

	if (MaxRevision >= 0)
	{
		for (int32 Revision = 0; Revision <= MaxRevision; ++Revision)
		{
			const FName RevisionSlotName = BuildRevisionSlotName(SlotBase, Revision);
			if (UGameplayStatics::DoesSaveGameExist(RevisionSlotName.ToString(), DefaultUserIndex))
			{
				UGameplayStatics::DeleteGameInSlot(RevisionSlotName.ToString(), DefaultUserIndex);
			}
		}
	}

	RemoveIndexEntry(IndexObj, SlotBase);
	if (!SaveIndex(IndexObj, OutResult))
	{
		BroadcastSaveFailure(OutResult);
		return false;
	}

	if (CurrentSlotBaseName == SlotBase)
	{
		CurrentSlotBaseName = NAME_None;
		CurrentSaveGame = nullptr;
	}

	OutResult.bSuccess = true;
	OutResult.SlotName = SlotBase;
	OutResult.SlotNumber = MaxRevision;
	OnSaveCompleted.Broadcast(OutResult);
	return true;
}

bool UARSaveSubsystem::PushCurrentSaveToPlayer(AARPlayerController* TargetPlayerController, FARSaveResult& OutResult)
{
	OutResult = FARSaveResult();
	if (!TargetPlayerController)
	{
		OutResult.Error = TEXT("PushCurrentSaveToPlayer failed: TargetPlayerController is null.");
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		OutResult.Error = TEXT("PushCurrentSaveToPlayer failed: no world.");
		return false;
	}

	if (World->GetNetMode() != NM_Standalone && World->GetAuthGameMode() == nullptr)
	{
		OutResult.Error = TEXT("PushCurrentSaveToPlayer must run on authority/server.");
		return false;
	}

	if (!CurrentSaveGame)
	{
		QueuePendingCanonicalSyncRequest(TargetPlayerController);
		OutResult.Error = TEXT("PushCurrentSaveToPlayer deferred: no current save loaded yet; request queued.");
		return false;
	}

	FName SlotBase = CurrentSlotBaseName;
	if (SlotBase.IsNone())
	{
		SlotBase = NormalizeSlotBaseName(CurrentSaveGame->SaveSlot);
	}

	if (SlotBase.IsNone())
	{
		OutResult.Error = TEXT("PushCurrentSaveToPlayer failed: current slot base name is invalid.");
		return false;
	}

	const int32 Revision = CurrentSaveGame->SaveSlotNumber;
	if (Revision < 0)
	{
		OutResult.Error = TEXT("PushCurrentSaveToPlayer failed: current save revision is invalid.");
		return false;
	}

	TArray<uint8> SaveBytes;
	if (!UGameplayStatics::SaveGameToMemory(CurrentSaveGame, SaveBytes))
	{
		OutResult.Error = TEXT("PushCurrentSaveToPlayer failed: could not serialize current save to memory.");
		return false;
	}

	TargetPlayerController->ClientPersistCanonicalSave(SaveBytes, SlotBase, Revision);
	PendingCanonicalSyncRequests.RemoveAll([TargetPlayerController](const TWeakObjectPtr<AARPlayerController>& PendingPC)
	{
		return !PendingPC.IsValid() || PendingPC.Get() == TargetPlayerController;
	});

	OutResult.bSuccess = true;
	OutResult.SlotName = SlotBase;
	OutResult.SlotNumber = Revision;
	return true;
}

void UARSaveSubsystem::RequestGameStateHydration(AARGameStateBase* Requester)
{
	if (!Requester)
	{
		return;
	}

	if (!Requester->HasAuthority())
	{
		UE_LOG(ARLog, Verbose, TEXT("[SaveSubsystem] RequestGameStateHydration ignored on non-authority requester '%s'."), *GetNameSafe(Requester));
		return;
	}

	// Travel-transient GameState data is authoritative for first hydration pass after travel.
	if (PendingTravelGameStateData.IsValid())
	{
		IStructSerializable::Execute_ApplyStateFromStruct(Requester, PendingTravelGameStateData);
		PendingTravelGameStateData.Reset();
		return;
	}

	if (!CurrentSaveGame)
	{
		return;
	}

	const FInstancedStruct StructData = CurrentSaveGame->GetGameStateDataInstancedStruct();
	if (StructData.IsValid())
	{
		IStructSerializable::Execute_ApplyStateFromStruct(Requester, StructData);
	}
}

void UARSaveSubsystem::SetPendingTravelGameStateData(const FInstancedStruct& PendingStateData)
{
	if (!PendingStateData.IsValid())
	{
		PendingTravelGameStateData.Reset();
		return;
	}

	PendingTravelGameStateData = PendingStateData;
}

void UARSaveSubsystem::ClearPendingTravelGameStateData()
{
	PendingTravelGameStateData.Reset();
}

bool UARSaveSubsystem::TryHydratePlayerStateFromCurrentSave(AARPlayerStateBase* Requester, const bool bAllowSlotFallback)
{
	if (!Requester || !CurrentSaveGame)
	{
		return false;
	}

	if (!Requester->HasAuthority())
	{
		UE_LOG(ARLog, Verbose, TEXT("[SaveSubsystem] RequestPlayerStateHydration ignored on non-authority requester '%s'."), *GetNameSafe(Requester));
		return false;
	}

	FARPlayerIdentity QueryIdentity;
	QueryIdentity.LegacyId = Requester->GetPlayerId();
	QueryIdentity.DisplayName = FText::FromString(Requester->GetDisplayNameValue());
	QueryIdentity.PlayerSlot = Requester->GetPlayerSlot();
	if (Requester->GetUniqueId().IsValid())
	{
		QueryIdentity.UniqueNetIdString = Requester->GetUniqueId()->ToString();
	}

	FARPlayerStateSaveData PlayerData;
	int32 Index = INDEX_NONE;
	bool bFound = CurrentSaveGame->FindPlayerStateDataByIdentity(QueryIdentity, PlayerData, Index);
	if (!bFound && bAllowSlotFallback)
	{
		bFound = CurrentSaveGame->FindPlayerStateDataBySlot(Requester->GetPlayerSlot(), PlayerData, Index);
	}

	if (!bFound || Index == INDEX_NONE)
	{
		return false;
	}

	if (PlayerData.PlayerStateData.IsValid())
	{
		IStructSerializable::Execute_ApplyStateFromStruct(Requester, PlayerData.PlayerStateData);
	}

	// Ensure canonical replicated player-facing fields are restored even if not part of struct schema.
	Requester->SetCharacterPicked(PlayerData.CharacterPicked);
	Requester->SetDisplayNameValue(PlayerData.Identity.DisplayName.ToString());
	Requester->SetLoadoutTags(PlayerData.LoadoutTags);
	return true;
}

void UARSaveSubsystem::BroadcastSaveFailure(const FARSaveResult& Result)
{
	UE_LOG(ARLog, Warning, TEXT("[SaveSubsystem] Save failed: %s"), *Result.Error);
	OnSaveFailed.Broadcast(Result);
}

void UARSaveSubsystem::BroadcastLoadFailure(const FARSaveResult& Result)
{
	UE_LOG(ARLog, Warning, TEXT("[SaveSubsystem] Load failed: %s"), *Result.Error);
	OnLoadFailed.Broadcast(Result);
}

void UARSaveSubsystem::ApplyLoadedSave(UARSaveGame* LoadedSave, const FARSaveResult& LoadResult)
{
	CurrentSaveGame = LoadedSave;
	CurrentSlotBaseName = LoadResult.SlotName;
	FlushPendingCanonicalSyncRequests();

	if (UARSaveIndexGame* IndexObj = Cast<UARSaveIndexGame>(UGameplayStatics::LoadGameFromSlot(ARSaveInternal::SaveIndexSlot, DefaultUserIndex)))
	{
		FARSaveSlotDescriptor Descriptor;
		Descriptor.SlotName = LoadResult.SlotName;
		Descriptor.SlotNumber = LoadResult.SlotNumber;
		Descriptor.SaveVersion = LoadedSave->SaveGameVersion;
		Descriptor.CyclesPlayed = LoadedSave->Cycles;
		Descriptor.LastSavedTime = LoadedSave->LastSaved;
		Descriptor.Money = LoadedSave->Money;
		UpsertIndexEntry(IndexObj, Descriptor);

		FARSaveResult IgnoreResult;
		SaveIndex(IndexObj, IgnoreResult);
	}
}

void UARSaveSubsystem::QueuePendingCanonicalSyncRequest(AARPlayerController* TargetPlayerController)
{
	if (!TargetPlayerController)
	{
		return;
	}

	PendingCanonicalSyncRequests.RemoveAll([TargetPlayerController](const TWeakObjectPtr<AARPlayerController>& PendingPC)
	{
		return !PendingPC.IsValid() || PendingPC.Get() == TargetPlayerController;
	});
	PendingCanonicalSyncRequests.Add(TargetPlayerController);
}

void UARSaveSubsystem::FlushPendingCanonicalSyncRequests()
{
	if (!CurrentSaveGame)
	{
		return;
	}

	for (int32 i = PendingCanonicalSyncRequests.Num() - 1; i >= 0; --i)
	{
		AARPlayerController* PendingPC = PendingCanonicalSyncRequests[i].Get();
		if (!PendingPC)
		{
			PendingCanonicalSyncRequests.RemoveAtSwap(i);
			continue;
		}

		FARSaveResult Result;
		PushCurrentSaveToPlayer(PendingPC, Result);
	}
}
