#include "ARSaveSubsystem.h"

#include "ARGameStateBase.h"
#include "ARGameModeBase.h"
#include "ARLog.h"
#include "ARCharacterStateRuntime.h"
#include "ARCharacterSubsystem.h"
#include "ARPlayerController.h"
#include "ARPlayerStateBase.h"
#include "ARLoadoutSettings.h"
#include "ParleyDialogueSubsystem.h"
#include "AREnergyDrinkCarryItem.h"
#include "ARRamenBowlActor.h"
#include "ARRamenMeatActor.h"
#include "ParleySpeakerSubsystem.h"
#include "ARSaveGame.h"
#include "ARSaveIndexGame.h"
#include "ARSaveUserSettings.h"
#include "ARCarryItemBase.h"
#include "ARShopCarryComponent.h"
#include "Components/SceneComponent.h"
#include "Async/Async.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ScopeExit.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Pawn.h"
#include "Templates/UnrealTemplate.h"
#include "StructSerializable.h"

namespace ARSaveInternal
{
	static const TCHAR* SaveIndexSlot = TEXT("SaveIndex");
	static const TCHAR* DebugSaveIndexSlot = TEXT("SaveIndexDebug");
	static const TCHAR* DebugSlotSuffix = TEXT("_debug");
	static const TCHAR* SlotAdj[] = {
		TEXT("Spicy"), TEXT("Neon"), TEXT("Corporate"), TEXT("Fermented"), TEXT("Unpaid"), TEXT("Galactic"),
		TEXT("Suspicious"), TEXT("Nuclear"), TEXT("Chaotic"), TEXT("Certified"), TEXT("Questionable"), TEXT("Overclocked")
	};
static const TCHAR* SlotNoun[] = {
	TEXT("Ramen"), TEXT("Invader"), TEXT("Noodle"), TEXT("Colony"), TEXT("Dumpling"), TEXT("Broth"),
	TEXT("MegaCorp"), TEXT("Franchise"), TEXT("Saucer"), TEXT("Payroll"), TEXT("Kiosk"), TEXT("Meatball")
	};

static FName NormalizeSlotBaseForNamespace(FName SlotBaseName, bool bUseDebugSaves)
{
	FString Base = SlotBaseName.ToString().TrimStartAndEnd();
	if (Base.IsEmpty())
	{
		return NAME_None;
	}

	if (bUseDebugSaves)
	{
		if (!Base.EndsWith(DebugSlotSuffix, ESearchCase::IgnoreCase))
		{
			Base += DebugSlotSuffix;
		}
	}
	else if (Base.EndsWith(DebugSlotSuffix, ESearchCase::IgnoreCase))
	{
		Base.LeftChopInline(FCString::Strlen(DebugSlotSuffix), EAllowShrinking::No);
	}

	return FName(*Base);
}

static FName GetLogicalSlotBaseForNamespace(FName SlotBaseName, bool bUseDebugSaves)
{
	FString Base = SlotBaseName.ToString().TrimStartAndEnd();
	if (Base.IsEmpty())
	{
		return NAME_None;
	}

	if (bUseDebugSaves && Base.EndsWith(DebugSlotSuffix, ESearchCase::IgnoreCase))
	{
		Base.LeftChopInline(FCString::Strlen(DebugSlotSuffix), EAllowShrinking::No);
	}

	return FName(*Base);
}

static const TCHAR* GetIndexSlotNameForNamespace(bool bUseDebugSaves)
{
	return bUseDebugSaves ? DebugSaveIndexSlot : SaveIndexSlot;
}

static void EnablePIESeamlessTravelIfNeeded()
{
#if !UE_BUILD_SHIPPING
	if (GIsEditor)
	{
		if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("net.AllowPIESeamlessTravel")))
		{
			CVar->Set(true, ECVF_SetByGameSetting);
		}
	}
#endif
}

static void ApplySavedGameStateFieldsToRuntime(AARGameStateBase* GameState, const UARSaveGame* SaveGame)
{
	if (!GameState || !SaveGame)
	{
		return;
	}

	FGameplayTagContainer UnlocksToApply = SaveGame->Unlocks;
	if (const UARLoadoutSettings* LoadoutSettings = GetDefault<UARLoadoutSettings>())
	{
		UnlocksToApply.AppendTags(LoadoutSettings->GetEffectiveDefaultStartingUnlocks());
	}

	GameState->SetUnlocksFromSave(UnlocksToApply);
	GameState->SetMoneyFromSave(SaveGame->Money);
	GameState->SetScrapFromSave(SaveGame->Scrap);
	GameState->SetMeatFromSave(SaveGame->Meat);
	GameState->SetActiveFactionTagFromSave(SaveGame->ActiveFactionTag);
	GameState->SetActiveFactionEffectTagsFromSave(SaveGame->ActiveFactionEffectTags);
}

static bool IsShopModeWorld(const UWorld* World)
{
	if (!World)
	{
		return false;
	}

	const AARGameModeBase* GameMode = Cast<AARGameModeBase>(World->GetAuthGameMode());
	const FGameplayTag ShopModeTag = FGameplayTag::RequestGameplayTag(TEXT("Mode.Shop"), false);
	return GameMode && ShopModeTag.IsValid() && GameMode->GetModeTag() == ShopModeTag;
}

static void BuildHeldShopCarrySet(const UWorld* World, TSet<const AActor*>& OutHeldActors)
{
	OutHeldActors.Reset();
	if (!World)
	{
		return;
	}

	const AARGameStateBase* GameState = World->GetGameState<AARGameStateBase>();
	if (!GameState)
	{
		return;
	}

	for (APlayerState* PlayerStateBase : GameState->PlayerArray)
	{
		const AARPlayerStateBase* PlayerState = Cast<AARPlayerStateBase>(PlayerStateBase);
		const APawn* Pawn = PlayerState ? PlayerState->GetCurrentCharacterPawn() : nullptr;
		const UARShopCarryComponent* CarryComponent = Pawn ? Pawn->FindComponentByClass<UARShopCarryComponent>() : nullptr;
		const AActor* HeldActor = CarryComponent ? CarryComponent->GetHeldActor() : nullptr;
		if (HeldActor)
		{
			OutHeldActors.Add(HeldActor);
		}
	}
}

static void CaptureShopTransientCarryables(UWorld* World, TArray<FARShopTransientCarryableSnapshot>& OutSnapshots)
{
	OutSnapshots.Reset();
	if (!World)
	{
		return;
	}

	TSet<const AActor*> HeldActors;
	BuildHeldShopCarrySet(World, HeldActors);

	for (TActorIterator<AARCarryItemBase> It(World); It; ++It)
	{
		AARCarryItemBase* CarryActor = *It;
		if (!CarryActor || !IsValid(CarryActor))
		{
			continue;
		}

		const AAREnergyDrinkCarryItem* EnergyDrinkActor = Cast<AAREnergyDrinkCarryItem>(CarryActor);
		const AARRamenMeatActor* MeatActor = Cast<AARRamenMeatActor>(CarryActor);
		const AARRamenBowlActor* BowlActor = Cast<AARRamenBowlActor>(CarryActor);

		if (HeldActors.Contains(CarryActor))
		{
			continue;
		}

		const USceneComponent* RootComponent = CarryActor->GetRootComponent();
		if (RootComponent && RootComponent->GetAttachParent() != nullptr)
		{
			continue;
		}

		FARShopTransientCarryableSnapshot& Snapshot = OutSnapshots.AddDefaulted_GetRef();
		Snapshot.ActorClass = CarryActor->GetClass();
		Snapshot.WorldTransform = CarryActor->GetActorTransform();
		if (EnergyDrinkActor)
		{
			Snapshot.EnergyDrinkItemTag = EnergyDrinkActor->GetEnergyDrinkItemTag();
		}
		if (MeatActor)
		{
			Snapshot.MeatColor = MeatActor->GetMeatColor();
			Snapshot.MeatTag = MeatActor->GetMeatTag();
			Snapshot.MeatQualityTier = MeatActor->GetMeatQualityTier();
			Snapshot.MeatAmount = FMath::Max(1, MeatActor->GetMeatAmount());
		}
		if (BowlActor)
		{
			Snapshot.BowlSpec = BowlActor->GetBowlSpec();
			Snapshot.BowlFillStep = FMath::Max(0, BowlActor->GetFillStep());
		}
	}

	OutSnapshots.Sort([](const FARShopTransientCarryableSnapshot& A, const FARShopTransientCarryableSnapshot& B)
	{
		const FString ClassA = A.ActorClass.ToSoftObjectPath().ToString();
		const FString ClassB = B.ActorClass.ToSoftObjectPath().ToString();
		if (ClassA != ClassB)
		{
			return ClassA < ClassB;
		}

		const FVector LocA = A.WorldTransform.GetLocation();
		const FVector LocB = B.WorldTransform.GetLocation();
		if (!LocA.Equals(LocB, KINDA_SMALL_NUMBER))
		{
			if (!FMath::IsNearlyEqual(LocA.X, LocB.X, KINDA_SMALL_NUMBER))
			{
				return LocA.X < LocB.X;
			}
			if (!FMath::IsNearlyEqual(LocA.Y, LocB.Y, KINDA_SMALL_NUMBER))
			{
				return LocA.Y < LocB.Y;
			}
			return LocA.Z < LocB.Z;
		}

		return A.EnergyDrinkItemTag.ToString() < B.EnergyDrinkItemTag.ToString();
	});
}

static FARCharacterSaveData* FindOrAddCharacterState(UARSaveGame* SaveGame, const FGameplayTag CharacterTag)
{
	if (!SaveGame)
	{
		return nullptr;
	}

	return &SaveGame->FindOrAddCharacterStateData(CharacterTag);
}

static bool CaptureHeldShopItemSnapshot(AActor* HeldActor, FARCharacterHeldShopItemSnapshot& OutSnapshot)
{
	OutSnapshot = FARCharacterHeldShopItemSnapshot();
	if (!HeldActor)
	{
		return false;
	}

	if (AAREnergyDrinkCarryItem* EnergyDrink = Cast<AAREnergyDrinkCarryItem>(HeldActor))
	{
		OutSnapshot.ActorClass = EnergyDrink->GetClass();
		OutSnapshot.EnergyDrinkItemTag = EnergyDrink->GetEnergyDrinkItemTag();
		return true;
	}

	if (AARRamenMeatActor* MeatActor = Cast<AARRamenMeatActor>(HeldActor))
	{
		OutSnapshot.ActorClass = MeatActor->GetClass();
		OutSnapshot.MeatColor = MeatActor->GetMeatColor();
		OutSnapshot.MeatTag = MeatActor->GetMeatTag();
		OutSnapshot.MeatQualityTier = MeatActor->GetMeatQualityTier();
		OutSnapshot.MeatAmount = FMath::Max(1, MeatActor->GetMeatAmount());
		return true;
	}

	if (AARRamenBowlActor* BowlActor = Cast<AARRamenBowlActor>(HeldActor))
	{
		OutSnapshot.ActorClass = BowlActor->GetClass();
		OutSnapshot.BowlSpec = BowlActor->GetBowlSpec();
		OutSnapshot.BowlFillStep = FMath::Max(0, BowlActor->GetFillStep());
		return true;
	}

	return false;
}

static void CaptureShopCharacterSnapshot(UARSaveGame* SaveGame, const AARPlayerStateBase* PlayerState)
{
	if (!SaveGame || !PlayerState)
	{
		return;
	}

	const AARCharacterStateRuntime* Runtime = PlayerState->GetCurrentCharacterRuntime();
	const FGameplayTag CharacterTag = Runtime
		? ARPlayer::NormalizeCharacterTag(Runtime->GetCharacterTag())
		: ARPlayer::NormalizeCharacterTag(PlayerState->GetCurrentCharacterTag());
	if (!CharacterTag.IsValid())
	{
		return;
	}

	FARCharacterSaveData* CharacterState = FindOrAddCharacterState(SaveGame, CharacterTag);
	if (!CharacterState)
	{
		return;
	}

	if (Runtime)
	{
		Runtime->WriteSaveData(*CharacterState);
	}

	CharacterState->ShopSnapshot = FARCharacterShopSnapshot();

	const APawn* Pawn = PlayerState->GetCurrentCharacterPawn();
	if (Pawn)
	{
		CharacterState->ShopSnapshot.bHasCharacterTransform = true;
		CharacterState->ShopSnapshot.CharacterTransform = Pawn->GetActorTransform();

		if (const UARShopCarryComponent* CarryComponent = Pawn->FindComponentByClass<UARShopCarryComponent>())
		{
			FARCharacterHeldShopItemSnapshot HeldItemSnapshot;
			if (CaptureHeldShopItemSnapshot(CarryComponent->GetHeldActor(), HeldItemSnapshot))
			{
				CharacterState->ShopSnapshot.bHasHeldItem = true;
				CharacterState->ShopSnapshot.HeldItem = MoveTemp(HeldItemSnapshot);
			}
		}
	}
}

}

void UARSaveSubsystem::Deinitialize()
{
	CurrentSaveGame = nullptr;
	CurrentSlotBaseName = NAME_None;
	PendingCanonicalSyncRequests.Reset();
	PendingTravelGameStateData.Reset();
	PendingLoadedSaveModeTag = FGameplayTag();
	PendingLoadedSaveMapPath.Reset();
	bPendingFreshLoadEntry = false;
	ClearSharedOnlineIdentityRuntimeCache();
	Super::Deinitialize();
}

void UARSaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ClearSharedOnlineIdentityRuntimeCache();
	ARSaveInternal::EnablePIESeamlessTravelIfNeeded();
}

FString UARSaveSubsystem::BuildSharedOnlineIdentityRuntimeKey(const FARPlayerIdentity& Identity)
{
	if (!Identity.HasStrictOnlineIdentity())
	{
		return FString();
	}

	return FString::Printf(TEXT("%s|%s"), *Identity.UniqueNetIdType, *Identity.UniqueNetIdString);
}

void UARSaveSubsystem::ClearSharedOnlineIdentityRuntimeCache()
{
	SharedOnlineIdentityRuntimeClaims.Reset();
}

bool UARSaveSubsystem::ResolveSharedOnlineSecondaryProfileForPlayerState(const APlayerState* PlayerState) const
{
	if (!PlayerState)
	{
		return false;
	}

	FARPlayerIdentity Identity;
	if (PlayerState->GetUniqueId().IsValid())
	{
		Identity.UniqueNetIdString = PlayerState->GetUniqueId()->ToString();
		Identity.UniqueNetIdType = PlayerState->GetUniqueId()->GetType().ToString();
	}

	const FString IdentityKey = BuildSharedOnlineIdentityRuntimeKey(Identity);
	if (IdentityKey.IsEmpty())
	{
		return false;
	}

	TArray<TWeakObjectPtr<APlayerState>>& Claims = SharedOnlineIdentityRuntimeClaims.FindOrAdd(IdentityKey);
	Claims.RemoveAll([](const TWeakObjectPtr<APlayerState>& Entry)
	{
		APlayerState* Existing = Entry.Get();
		if (!Existing || !IsValid(Existing))
		{
			return true;
		}

		const AController* OwnerController = Cast<AController>(Existing->GetOwner());
		return !OwnerController || OwnerController->PlayerState != Existing;
	});

	for (int32 Index = 0; Index < Claims.Num(); ++Index)
	{
		if (Claims[Index].Get() == PlayerState)
		{
			return Index > 0;
		}
	}

	Claims.Add(const_cast<APlayerState*>(PlayerState));
	return Claims.Num() > 1;
}

FARPlayerIdentity UARSaveSubsystem::BuildRuntimePlayerIdentity(const APlayerState* PlayerState) const
{
	FARPlayerIdentity Identity;
	const AARPlayerStateBase* ARPS = Cast<AARPlayerStateBase>(PlayerState);
	if (!ARPS)
	{
		return Identity;
	}

	Identity.DisplayName = FText::FromString(ARPS->GetDisplayNameValue());

	if (PlayerState->GetUniqueId().IsValid())
	{
		Identity.UniqueNetIdString = PlayerState->GetUniqueId()->ToString();
		Identity.UniqueNetIdType = PlayerState->GetUniqueId()->GetType().ToString();
	}

	Identity.bSharedOnlineIdSecondaryProfile = ResolveSharedOnlineSecondaryProfileForPlayerState(PlayerState);
	return Identity;
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
	auto BuildBaseCandidate = []() -> FName
	{
		const TCHAR* Adj = ARSaveInternal::SlotAdj[FMath::RandRange(0, UE_ARRAY_COUNT(ARSaveInternal::SlotAdj) - 1)];
		const TCHAR* Noun = ARSaveInternal::SlotNoun[FMath::RandRange(0, UE_ARRAY_COUNT(ARSaveInternal::SlotNoun) - 1)];
		return FName(*FString::Printf(TEXT("%s_%s"), Adj, Noun));
	};

	auto IsLogicalNameTaken = [this](const FName CandidateLogicalName, const TSet<FName>& ExistingLogicalNames) -> bool
	{
		if (ExistingLogicalNames.Contains(CandidateLogicalName))
		{
			return true;
		}

		const FName CanonicalRevisionZero = BuildRevisionSlotName(CandidateLogicalName, 0);
		if (UGameplayStatics::DoesSaveGameExist(CanonicalRevisionZero.ToString(), DefaultUserIndex))
		{
			return true;
		}

		const FName DebugSlotBase = ARSaveInternal::NormalizeSlotBaseForNamespace(CandidateLogicalName, true);
		const FName DebugRevisionZero = BuildRevisionSlotName(DebugSlotBase, 0);
		return UGameplayStatics::DoesSaveGameExist(DebugRevisionZero.ToString(), DefaultUserIndex);
	};

	constexpr int32 MaxBaseAttempts = 128;
	constexpr int32 MaxNumericFallbackAttempts = 128;
	TSet<FName> ExistingLogicalNames;

	if (bEnsureUnique)
	{
		UARSaveIndexGame* CanonicalIndex = nullptr;
		if (UGameplayStatics::DoesSaveGameExist(ARSaveInternal::SaveIndexSlot, DefaultUserIndex))
		{
			if (USaveGame* LoadedIndex = UGameplayStatics::LoadGameFromSlot(ARSaveInternal::SaveIndexSlot, DefaultUserIndex))
			{
				CanonicalIndex = Cast<UARSaveIndexGame>(LoadedIndex);
				if (!CanonicalIndex)
				{
					UE_LOG(ARLog, Warning, TEXT("[SaveSubsystem] GenerateRandomSlotBaseName: index slot '%s' exists but is incompatible. Falling back to disk probes for uniqueness."),
						ARSaveInternal::SaveIndexSlot);
				}
			}
			else
			{
				UE_LOG(ARLog, Warning, TEXT("[SaveSubsystem] GenerateRandomSlotBaseName: failed loading index slot '%s'. Falling back to disk probes for uniqueness."),
					ARSaveInternal::SaveIndexSlot);
			}
		}
		if (CanonicalIndex)
		{
			for (const FARSaveSlotDescriptor& Entry : CanonicalIndex->SlotNames)
			{
				ExistingLogicalNames.Add(ARSaveInternal::GetLogicalSlotBaseForNamespace(Entry.SlotName, false));
			}
		}

		UARSaveIndexGame* DebugIndex = nullptr;
		if (UGameplayStatics::DoesSaveGameExist(ARSaveInternal::DebugSaveIndexSlot, DefaultUserIndex))
		{
			if (USaveGame* LoadedIndex = UGameplayStatics::LoadGameFromSlot(ARSaveInternal::DebugSaveIndexSlot, DefaultUserIndex))
			{
				DebugIndex = Cast<UARSaveIndexGame>(LoadedIndex);
				if (!DebugIndex)
				{
					UE_LOG(ARLog, Warning, TEXT("[SaveSubsystem] GenerateRandomSlotBaseName: debug index slot '%s' exists but is incompatible. Falling back to disk probes for uniqueness."),
						ARSaveInternal::DebugSaveIndexSlot);
				}
			}
			else
			{
				UE_LOG(ARLog, Warning, TEXT("[SaveSubsystem] GenerateRandomSlotBaseName: failed loading debug index slot '%s'. Falling back to disk probes for uniqueness."),
					ARSaveInternal::DebugSaveIndexSlot);
			}
		}
		if (DebugIndex)
		{
			for (const FARSaveSlotDescriptor& Entry : DebugIndex->SlotNames)
			{
				ExistingLogicalNames.Add(ARSaveInternal::GetLogicalSlotBaseForNamespace(Entry.SlotName, true));
			}
		}
	}

	for (int32 Attempt = 0; Attempt < MaxBaseAttempts; ++Attempt)
	{
		const FName Candidate = BuildBaseCandidate();
		if (!bEnsureUnique || !IsLogicalNameTaken(Candidate, ExistingLogicalNames))
		{
			return Candidate;
		}
	}

	for (int32 Attempt = 0; Attempt < MaxNumericFallbackAttempts; ++Attempt)
	{
		const FName BaseCandidate = BuildBaseCandidate();
		const int32 Suffix = FMath::RandRange(10, 9999);
		const FName Candidate = FName(*FString::Printf(TEXT("%s_%d"), *BaseCandidate.ToString(), Suffix));
		if (!bEnsureUnique || !IsLogicalNameTaken(Candidate, ExistingLogicalNames))
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

void UARSaveSubsystem::LogCanonicalSavePayloadSize(const TCHAR* Context, FName SlotBaseName, int32 SlotNumber, int32 PayloadSizeBytes, int32 RemoteRecipientCount) const
{
	const UARSaveUserSettings* Settings = GetDefault<UARSaveUserSettings>();
	if (!Settings || !Settings->bLogCanonicalSavePayloadSize)
	{
		return;
	}

	const int32 WarningThresholdBytes = FMath::Max(1, Settings->CanonicalSaveWarningSizeBytes);
	const int32 CriticalThresholdBytes = FMath::Max(WarningThresholdBytes, Settings->CanonicalSaveCriticalSizeBytes);
	const double PayloadSizeKiB = static_cast<double>(PayloadSizeBytes) / 1024.0;
	const bool bAtOrAboveCritical = PayloadSizeBytes >= CriticalThresholdBytes;
	const bool bAtOrAboveWarning = PayloadSizeBytes >= WarningThresholdBytes;

	const TCHAR* ContextLabel = Context ? Context : TEXT("Unknown");
	const TCHAR* SeveritySuffix = bAtOrAboveCritical
		? TEXT(" - chunking should be prioritized")
		: (bAtOrAboveWarning ? TEXT(" - approaching chunking territory") : TEXT(""));

	if (bAtOrAboveCritical)
	{
		UE_LOG(
			ARLog,
			Error,
			TEXT("[SaveSubsystem] Canonical save payload (%s) Slot=%s Rev=%d Bytes=%d (%.1f KiB) Recipients=%d Thresholds: warn>=%d critical>=%d%s"),
			ContextLabel,
			*SlotBaseName.ToString(),
			SlotNumber,
			PayloadSizeBytes,
			PayloadSizeKiB,
			RemoteRecipientCount,
			WarningThresholdBytes,
			CriticalThresholdBytes,
			SeveritySuffix);
	}
	else if (bAtOrAboveWarning)
	{
		UE_LOG(
			ARLog,
			Warning,
			TEXT("[SaveSubsystem] Canonical save payload (%s) Slot=%s Rev=%d Bytes=%d (%.1f KiB) Recipients=%d Thresholds: warn>=%d critical>=%d%s"),
			ContextLabel,
			*SlotBaseName.ToString(),
			SlotNumber,
			PayloadSizeBytes,
			PayloadSizeKiB,
			RemoteRecipientCount,
			WarningThresholdBytes,
			CriticalThresholdBytes,
			SeveritySuffix);
	}
	else
	{
		UE_LOG(
			ARLog,
			Display,
			TEXT("[SaveSubsystem] Canonical save payload (%s) Slot=%s Rev=%d Bytes=%d (%.1f KiB) Recipients=%d Thresholds: warn>=%d critical>=%d"),
			ContextLabel,
			*SlotBaseName.ToString(),
			SlotNumber,
			PayloadSizeBytes,
			PayloadSizeKiB,
			RemoteRecipientCount,
			WarningThresholdBytes,
			CriticalThresholdBytes);
	}
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

bool UARSaveSubsystem::ResolvePlayerSaveDataIndex(
	const UARSaveGame* SaveGame,
	const FARPlayerIdentity& Identity,
	int32& OutIndex)
{
	OutIndex = INDEX_NONE;
	if (!SaveGame)
	{
		return false;
	}

	FARPlayerStateSaveData IgnoredData;
	if (SaveGame->FindPlayerStateDataByIdentity(Identity, IgnoredData, OutIndex))
	{
		return SaveGame->PlayerStates.IsValidIndex(OutIndex);
	}

	return false;
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
			UE_LOG(ARLog, Warning, TEXT("[SaveSubsystem] Recreating incompatible save index '%s'."), *SlotName);
			UGameplayStatics::DeleteGameInSlot(SlotName, DefaultUserIndex);
		}
		else
		{
			return true;
		}
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

bool UARSaveSubsystem::RollbackRevisionWrite(FName SlotBaseName, int32 SlotNumber, FARSaveResult& OutResult) const
{
	const FString RevisionSlotName = BuildRevisionSlotName(SlotBaseName, SlotNumber).ToString();
	if (!UGameplayStatics::DoesSaveGameExist(RevisionSlotName, DefaultUserIndex))
	{
		return true;
	}

	if (UGameplayStatics::DeleteGameInSlot(RevisionSlotName, DefaultUserIndex))
	{
		return true;
	}

	const FString RollbackError = FString::Printf(TEXT("Failed to delete revision slot '%s' during rollback."), *RevisionSlotName);
	UE_LOG(ARLog, Warning, TEXT("[SaveSubsystem] %s"), *RollbackError);
	if (OutResult.Error.IsEmpty())
	{
		OutResult.Error = RollbackError;
	}
	else
	{
		OutResult.Error += TEXT(" ");
		OutResult.Error += RollbackError;
	}
	return false;
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
		SaveObject->Unlocks = GS->GetUnlocks();
		SaveObject->Money = GS->GetMoney();
		SaveObject->Scrap = GS->GetScrap();
		SaveObject->Meat = GS->GetMeat();
		SaveObject->ActiveFactionTag = GS->GetActiveFactionTag();
		SaveObject->ActiveFactionEffectTags = GS->GetActiveFactionEffectTags();
	}

	if (const UARLoadoutSettings* LoadoutSettings = GetDefault<UARLoadoutSettings>())
	{
		SaveObject->Unlocks.AppendTags(LoadoutSettings->GetEffectiveDefaultStartingUnlocks());
	}

	// Persist cycles from current save as authoritative progression counter (not GameState-owned).
	if (CurrentSaveGame)
	{
		SaveObject->Cycles = CurrentSaveGame->Cycles;
		SaveObject->ProgressionTags = CurrentSaveGame->ProgressionTags;
		SaveObject->FactionClout = CurrentSaveGame->FactionClout;
		SaveObject->FactionPopularityStates = CurrentSaveGame->FactionPopularityStates;
		SaveObject->FactionSpeakerReputationStates = CurrentSaveGame->FactionSpeakerReputationStates;
		SaveObject->DialogueSpeakerRelationshipStates = CurrentSaveGame->DialogueSpeakerRelationshipStates;
		SaveObject->DialogueCompletedConversationTagsByGame = CurrentSaveGame->DialogueCompletedConversationTagsByGame;
		SaveObject->CharacterStates = CurrentSaveGame->CharacterStates;
		SaveObject->StoredEnergyDrinkStacks = CurrentSaveGame->StoredEnergyDrinkStacks;
		SaveObject->QueuedEnergyDrinkStacks = CurrentSaveGame->QueuedEnergyDrinkStacks;
		SaveObject->ActiveRunBuffPayloads = CurrentSaveGame->ActiveRunBuffPayloads;
		SaveObject->ActiveRunBuffCycleId = CurrentSaveGame->ActiveRunBuffCycleId;
		SaveObject->ShopTransientCarryables = CurrentSaveGame->ShopTransientCarryables;
		SaveObject->bClearShopTransientCarryablesOnNextShopLoad = CurrentSaveGame->bClearShopTransientCarryablesOnNextShopLoad;
		SaveObject->PendingVendingStockedBowls = CurrentSaveGame->PendingVendingStockedBowls;
	}

	SaveObject->LastSavedModeTag = FGameplayTag();
	SaveObject->LastSavedMapPath.Reset();
	if (const AARGameModeBase* GameMode = Cast<AARGameModeBase>(World->GetAuthGameMode()))
	{
		SaveObject->LastSavedModeTag = GameMode->GetModeTag();
	}
	if (const UPackage* WorldPackage = World->PersistentLevel ? World->PersistentLevel->GetOutermost() : nullptr)
	{
		SaveObject->LastSavedMapPath = WorldPackage->GetName();
	}

	if (ARSaveInternal::IsShopModeWorld(World))
	{
		ARSaveInternal::CaptureShopTransientCarryables(World, SaveObject->ShopTransientCarryables);
	}
	else
	{
		for (FARCharacterSaveData& CharacterState : SaveObject->CharacterStates)
		{
			CharacterState.ShopSnapshot = FARCharacterShopSnapshot();
		}
	}

	const TArray<FARPlayerStateSaveData> ExistingPlayerStates = CurrentSaveGame ? CurrentSaveGame->PlayerStates : SaveObject->PlayerStates;
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

		const FARPlayerIdentity RuntimeIdentity = BuildRuntimePlayerIdentity(PS);
		FARPlayerStateSaveData PlayerData;
		int32 BestMatchIndex = INDEX_NONE;
		int32 BestMatchScore = MIN_int32;
		for (int32 ExistingIndex = 0; ExistingIndex < ExistingPlayerStates.Num(); ++ExistingIndex)
		{
			const FARPlayerStateSaveData& ExistingPlayerData = ExistingPlayerStates[ExistingIndex];
			const FARPlayerIdentity& ExistingIdentity = ExistingPlayerData.Identity;

			int32 MatchScore = MIN_int32;
			if (ExistingIdentity.Matches(RuntimeIdentity))
			{
				MatchScore = 100;
			}

			if (MatchScore > BestMatchScore)
			{
				BestMatchScore = MatchScore;
				BestMatchIndex = ExistingIndex;
			}
		}

		if (ExistingPlayerStates.IsValidIndex(BestMatchIndex))
		{
			PlayerData = ExistingPlayerStates[BestMatchIndex];
		}

		const AARCharacterStateRuntime* CurrentRuntime = ARPS->GetCurrentCharacterRuntime();
		const FGameplayTag RuntimeCharacterTag = CurrentRuntime
			? ARPlayer::NormalizeCharacterTag(CurrentRuntime->GetCharacterTag())
			: FGameplayTag();
		PlayerData.Identity = RuntimeIdentity;
		PlayerData.CurrentCharacterTag = RuntimeCharacterTag.IsValid()
			? RuntimeCharacterTag
			: ARPlayer::NormalizeCharacterTag(ARPS->GetCurrentCharacterTag());
		PlayerData.bDialogueAutoAdvanceEnabled = ARPS->IsDialogueAutoAdvanceEnabled();
		if (PlayerData.CurrentCharacterTag.IsValid())
		{
			FARCharacterSaveData& ActiveCharacterState = SaveObject->FindOrAddCharacterStateData(PlayerData.CurrentCharacterTag);
			if (CurrentRuntime)
			{
				CurrentRuntime->WriteSaveData(ActiveCharacterState);
			}
		}
		PlayerData.SyncCharacterSelectionFromCurrentTag();

		SaveObject->PlayerStates.Add(MoveTemp(PlayerData));

		if (ARSaveInternal::IsShopModeWorld(World))
		{
			ARSaveInternal::CaptureShopCharacterSnapshot(SaveObject, ARPS);
		}
	}

	if (UARCharacterSubsystem* CharacterSubsystem = World->GetSubsystem<UARCharacterSubsystem>())
	{
		TArray<AARCharacterStateRuntime*> RegisteredRuntimes;
		CharacterSubsystem->GetRegisteredRuntimes(RegisteredRuntimes);
		for (AARCharacterStateRuntime* Runtime : RegisteredRuntimes)
		{
			if (!Runtime)
			{
				continue;
			}

			const FGameplayTag RuntimeCharacterTag = ARPlayer::NormalizeCharacterTag(Runtime->GetCharacterTag());
			if (!RuntimeCharacterTag.IsValid())
			{
				continue;
			}

			FARCharacterSaveData& CharacterState = SaveObject->FindOrAddCharacterStateData(RuntimeCharacterTag);
			Runtime->WriteSaveData(CharacterState);
			if (!ARSaveInternal::IsShopModeWorld(World))
			{
				CharacterState.ShopSnapshot = FARCharacterShopSnapshot();
			}
		}
	}
}

UARSaveGame* UARSaveSubsystem::LoadSaveObjectWithRollback(FName SlotBaseName, int32 RevisionOrLatest, int32& OutResolvedSlotNumber, FARSaveResult& OutResult, const TCHAR* IndexSlotName) const
{
	OutResolvedSlotNumber = INDEX_NONE;

	UARSaveIndexGame* IndexObj = nullptr;
	FARSaveResult IndexResult;
	if (!LoadOrCreateIndexForSlot(IndexObj, IndexResult, IndexSlotName))
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

bool UARSaveSubsystem::CreateNewSave(FName DesiredSlotBase, FARSaveSlotDescriptor& OutSlot, FARSaveResult& OutResult, bool bUseDebugSaves)
{
	OutSlot = FARSaveSlotDescriptor();
	OutResult = FARSaveResult();

	if (CurrentSaveGame)
	{
		OutResult.Error = FString::Printf(
			TEXT("CreateNewSave blocked: active save '%s' is loaded. Call UnloadCurrentSave first."),
			*CurrentSlotBaseName.ToString());
		OutResult.ResultCode = EARSaveResultCode::ValidationFailed;
		BroadcastSaveFailure(OutResult);
		return false;
	}

	FName SlotBase = DesiredSlotBase.IsNone() ? GenerateRandomSlotBaseName(true) : NormalizeSlotBaseName(DesiredSlotBase);
	SlotBase = ARSaveInternal::NormalizeSlotBaseForNamespace(SlotBase, bUseDebugSaves);
	const TCHAR* IndexSlotName = ARSaveInternal::GetIndexSlotNameForNamespace(bUseDebugSaves);

	UARSaveIndexGame* IndexObj = nullptr;
	if (UGameplayStatics::DoesSaveGameExist(IndexSlotName, DefaultUserIndex))
	{
		IndexObj = Cast<UARSaveIndexGame>(UGameplayStatics::LoadGameFromSlot(IndexSlotName, DefaultUserIndex));
		if (!IndexObj)
		{
			OutResult.Error = FString::Printf(TEXT("Failed to load save index '%s'."), IndexSlotName);
			OutResult.ResultCode = EARSaveResultCode::ValidationFailed;
			BroadcastSaveFailure(OutResult);
			return false;
		}
	}
	else
	{
		// New game should not persist anything to disk yet, including an empty index.
		IndexObj = Cast<UARSaveIndexGame>(UGameplayStatics::CreateSaveGameObject(UARSaveIndexGame::StaticClass()));
		if (!IndexObj)
		{
			OutResult.Error = TEXT("Failed to create transient save index object.");
			OutResult.ResultCode = EARSaveResultCode::ValidationFailed;
			BroadcastSaveFailure(OutResult);
			return false;
		}
	}

	for (const FARSaveSlotDescriptor& Entry : IndexObj->SlotNames)
	{
		if (Entry.SlotName == SlotBase)
		{
			OutResult.Error = FString::Printf(TEXT("Save slot '%s' already exists."), *SlotBase.ToString());
			OutResult.ResultCode = EARSaveResultCode::ValidationFailed;
			BroadcastSaveFailure(OutResult);
			return false;
		}
	}

	const FName RevisionZeroSlot = BuildRevisionSlotName(SlotBase, 0);
	if (UGameplayStatics::DoesSaveGameExist(RevisionZeroSlot.ToString(), DefaultUserIndex))
	{
		OutResult.Error = FString::Printf(TEXT("Save slot '%s' already exists on disk."), *SlotBase.ToString());
		OutResult.ResultCode = EARSaveResultCode::ValidationFailed;
		BroadcastSaveFailure(OutResult);
		return false;
	}

	UARSaveGame* NewSave = Cast<UARSaveGame>(UGameplayStatics::CreateSaveGameObject(UARSaveGame::StaticClass()));
	if (!NewSave)
	{
		OutResult.Error = TEXT("Failed to create UARSaveGame.");
		OutResult.ResultCode = EARSaveResultCode::ValidationFailed;
		BroadcastSaveFailure(OutResult);
		return false;
	}

	GatherRuntimeData(NewSave);
	NewSave->SaveSlot = ARSaveInternal::GetLogicalSlotBaseForNamespace(SlotBase, bUseDebugSaves);
	NewSave->SaveSlotNumber = 0;
	NewSave->SaveGameVersion = UARSaveGame::GetCurrentSchemaVersion();
	NewSave->LastSaved = FDateTime();
	OutResult.ClampedFieldCount = NewSave->ValidateAndSanitize(nullptr);

	FARSaveSlotDescriptor Descriptor;
	Descriptor.SlotName = SlotBase;
	Descriptor.SlotNumber = 0;
	Descriptor.SaveVersion = NewSave->SaveGameVersion;
	Descriptor.CyclesPlayed = NewSave->Cycles;
	Descriptor.LastSavedTime = NewSave->LastSaved;
	Descriptor.Money = NewSave->Money;

	CurrentSaveGame = NewSave;
	CurrentSlotBaseName = SlotBase;
	LastSaveTimestampUtc = FDateTime();
	bSaveDirty = true;
	ClearPendingFreshLoadEntry();
	OutSlot = Descriptor;
	OutResult.bSuccess = true;
	OutResult.ResultCode = EARSaveResultCode::Success;
	OutResult.SlotName = SlotBase;
	OutResult.SlotNumber = 0;
	// New-save creation is intentionally in-memory only; OnSaveCompleted is reserved for persisted writes.
	return true;
}

void UARSaveSubsystem::UnloadCurrentSave()
{
	CurrentSaveGame = nullptr;
	CurrentSlotBaseName = NAME_None;
	LastSaveTimestampUtc = FDateTime();
	bSaveDirty = false;
	PendingTravelGameStateData.Reset();
	ClearPendingFreshLoadEntry();

	UWorld* World = GetWorld();
	AARGameStateBase* GameState = World ? World->GetGameState<AARGameStateBase>() : nullptr;
	if (!GameState || !GameState->HasAuthority())
	{
		return;
	}

	const UARLoadoutSettings* LoadoutSettings = GetDefault<UARLoadoutSettings>();
	const FGameplayTagContainer DefaultUnlocks = LoadoutSettings
		? LoadoutSettings->GetEffectiveDefaultStartingUnlocks()
		: FGameplayTagContainer();

	GameState->SetUnlocksFromSave(DefaultUnlocks);
	GameState->SetMoneyFromSave(0);
	GameState->SetScrapFromSave(0);
	GameState->SetMeatFromSave(FARMeatState());
	GameState->SyncCyclesFromSave(0);
	GameState->SetActiveFactionTagFromSave(FGameplayTag());
	GameState->SetActiveFactionEffectTagsFromSave(FGameplayTagContainer());
	GameState->NotifyHydratedFromSave();

	for (APlayerState* BasePlayerState : GameState->PlayerArray)
	{
		AARPlayerStateBase* PlayerState = Cast<AARPlayerStateBase>(BasePlayerState);
		if (!PlayerState)
		{
			continue;
		}

		// Reset player identity/loadout runtime to first-join baseline for a fresh save flow.
		PlayerState->SetCurrentCharacterRuntime(nullptr);
		PlayerState->SetLoadoutTags(FGameplayTagContainer());
		PlayerState->InitializeForFirstSessionJoin();
		PlayerState->SetReadyForRun(false);
		PlayerState->SetDownedState(false);
		PlayerState->SetDeadState(false);
	}
}

void UARSaveSubsystem::ClearPendingFreshLoadEntry()
{
	bPendingFreshLoadEntry = false;
	PendingLoadedSaveModeTag = FGameplayTag();
	PendingLoadedSaveMapPath.Reset();
}

bool UARSaveSubsystem::PersistCanonicalSaveFromBytes(const TArray<uint8>& SaveBytes, FName SlotBaseName, int32 SlotNumber, FARSaveResult& OutResult)
{
	OutResult = FARSaveResult();
	if (SaveBytes.Num() == 0)
	{
		OutResult.Error = TEXT("Canonical save bytes are empty.");
		return false;
	}

	LogCanonicalSavePayloadSize(TEXT("PersistCanonicalSaveFromBytes"), SlotBaseName, SlotNumber, SaveBytes.Num(), 0);

	UARSaveGame* SaveObject = Cast<UARSaveGame>(UGameplayStatics::LoadGameFromMemory(SaveBytes));
	if (!SaveObject)
	{
		OutResult.Error = TEXT("Failed to deserialize canonical save bytes.");
		return false;
	}

	if (!UARSaveGame::IsSchemaVersionSupported(SaveObject->SaveGameVersion))
	{
		OutResult.Error = FString::Printf(
			TEXT("Canonical save schema %d unsupported on this build (supported %d..%d)."),
			SaveObject->SaveGameVersion,
			UARSaveGame::GetMinSupportedSchemaVersion(),
			UARSaveGame::GetCurrentSchemaVersion());
		UE_LOG(ARLog, Warning, TEXT("[SaveSubsystem] %s"), *OutResult.Error);
		return false;
	}

	TArray<FString> Warnings;
	OutResult.ClampedFieldCount = SaveObject->ValidateAndSanitize(&Warnings);
	for (const FString& Warning : Warnings)
	{
		UE_LOG(ARLog, Warning, TEXT("[SaveSubsystem] %s"), *Warning);
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
		RollbackRevisionWrite(SlotBaseName, SlotNumber, OutResult);
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

bool UARSaveSubsystem::SaveCurrentGame(FName SlotBaseName, bool bCreateNewRevision, FARSaveResult& OutResult, bool bUseDebugSaves)
{
	return SaveCurrentGameAsyncInternal(SlotBaseName, bCreateNewRevision, OutResult, bUseDebugSaves, false);
}

bool UARSaveSubsystem::SaveCurrentGameUnthrottled(FName SlotBaseName, bool bCreateNewRevision, FARSaveResult& OutResult, bool bUseDebugSaves)
{
	return SaveCurrentGameAsyncInternal(SlotBaseName, bCreateNewRevision, OutResult, bUseDebugSaves, true);
}

bool UARSaveSubsystem::SaveCurrentGameBlocking(FName SlotBaseName, bool bCreateNewRevision, FARSaveResult& OutResult, bool bUseDebugSaves)
{
	return SaveCurrentGameBlockingInternal(SlotBaseName, bCreateNewRevision, OutResult, bUseDebugSaves, false);
}

bool UARSaveSubsystem::SaveCurrentGameBlockingInternal(FName SlotBaseName, bool bCreateNewRevision, FARSaveResult& OutResult, bool bUseDebugSaves, const bool bIgnoreThrottle)
{
	OutResult = FARSaveResult();

	if (bSaveInProgress)
	{
		OutResult.Error = TEXT("Save already in progress.");
		OutResult.ResultCode = EARSaveResultCode::InProgress;
		BroadcastSaveFailure(OutResult);
		return false;
	}

	TGuardValue<bool> SaveGuard(bSaveInProgress, true);
	OnSaveStarted.Broadcast();

	UWorld* World = GetWorld();
	if (!World)
	{
		OutResult.Error = TEXT("No world available for save.");
		OutResult.ResultCode = EARSaveResultCode::NoWorld;
		BroadcastSaveFailure(OutResult);
		return false;
	}

	if (World->GetNetMode() != NM_Standalone && World->GetAuthGameMode() == nullptr)
	{
		OutResult.Error = TEXT("SaveCurrentGame must run on authority/server for canonical snapshot.");
		OutResult.ResultCode = EARSaveResultCode::AuthorityRequired;
		BroadcastSaveFailure(OutResult);
		return false;
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UParleyDialogueSubsystem* DialogueSubsystem = GI->GetSubsystem<UParleyDialogueSubsystem>())
		{
			if (DialogueSubsystem->HasActiveDialogueSession())
			{
				OutResult.Error = TEXT("SaveCurrentGame blocked: game cannot be saved mid-conversation.");
				OutResult.ResultCode = EARSaveResultCode::ValidationFailed;
				BroadcastSaveFailure(OutResult);
				return false;
			}
		}
	}

	const FDateTime NowUtc = FDateTime::UtcNow();
	if (!bIgnoreThrottle && MinSaveIntervalSeconds > 0.f && LastSaveTimestampUtc.GetTicks() != 0)
	{
		const double Elapsed = (NowUtc - LastSaveTimestampUtc).GetTotalSeconds();
		if (Elapsed < MinSaveIntervalSeconds)
		{
			OutResult.Error = FString::Printf(TEXT("Save throttled (%.2fs < min %.2fs)."), Elapsed, MinSaveIntervalSeconds);
			OutResult.ResultCode = EARSaveResultCode::Throttled;
			BroadcastSaveFailure(OutResult);
			return false;
		}
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
	SlotBase = ARSaveInternal::NormalizeSlotBaseForNamespace(SlotBase, bUseDebugSaves);

	const TCHAR* IndexSlotName = ARSaveInternal::GetIndexSlotNameForNamespace(bUseDebugSaves);

	int32 ExistingLatest = -1;
	UARSaveIndexGame* IndexObj = nullptr;
	if (!LoadOrCreateIndexForSlot(IndexObj, OutResult, IndexSlotName))
	{
		OutResult.ResultCode = EARSaveResultCode::Unknown;
		BroadcastSaveFailure(OutResult);
		return false;
	}

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
		OutResult.ResultCode = EARSaveResultCode::ValidationFailed;
		BroadcastSaveFailure(OutResult);
		return false;
	}

	GatherRuntimeData(SaveObject);
	SaveObject->SaveSlot = ARSaveInternal::GetLogicalSlotBaseForNamespace(SlotBase, bUseDebugSaves);
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
		OutResult.ResultCode = EARSaveResultCode::ValidationFailed;
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

	if (!SaveIndexForSlot(IndexObj, OutResult, IndexSlotName))
	{
		RollbackRevisionWrite(SlotBase, NewSlotNumber, OutResult);
		OutResult.ResultCode = EARSaveResultCode::ValidationFailed;
		BroadcastSaveFailure(OutResult);
		return false;
	}
	PruneOldRevisions(SlotBase, NewSlotNumber);

	// Distribute canonical save to clients so each machine persists equivalent snapshot.
	bool bHasRemoteRecipients = false;
	int32 RemoteRecipientCount = 0;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (AARPlayerController* PC = Cast<AARPlayerController>(It->Get()))
		{
			if (PC->GetNetMode() != NM_Standalone && !PC->IsLocalController())
			{
				bHasRemoteRecipients = true;
				++RemoteRecipientCount;
			}
		}
	}

	if (bHasRemoteRecipients)
	{
		TArray<uint8> SaveBytes;
		if (UGameplayStatics::SaveGameToMemory(SaveObject, SaveBytes))
		{
			LogCanonicalSavePayloadSize(TEXT("BlockingSaveFanOut"), SlotBase, NewSlotNumber, SaveBytes.Num(), RemoteRecipientCount);
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
	}

	CurrentSaveGame = SaveObject;
	CurrentSlotBaseName = SlotBase;
	FlushPendingCanonicalSyncRequests();
	LastSaveTimestampUtc = SaveObject->LastSaved;
	bSaveDirty = false;
	OutResult.bSuccess = true;
	OutResult.ResultCode = EARSaveResultCode::Success;
	OutResult.SlotName = SlotBase;
	OutResult.SlotNumber = NewSlotNumber;

	if (bLogSaveSuccess)
	{
		UE_LOG(ARLog, Log, TEXT("[SaveSubsystem] Save succeeded (Slot=%s Rev=%d Time=%s DirtyCleared=%s)"),
			*SlotBase.ToString(),
			NewSlotNumber,
			*SaveObject->LastSaved.ToString(),
			bSaveDirty ? TEXT("false") : TEXT("true"));
	}

	OnSaveCompleted.Broadcast(OutResult);
	return true;
}

bool UARSaveSubsystem::SaveCurrentGameAsyncInternal(FName SlotBaseName, bool bCreateNewRevision, FARSaveResult& OutResult, bool bUseDebugSaves, const bool bIgnoreThrottle)
{
	OutResult = FARSaveResult();

	if (bSaveInProgress)
	{
		OutResult.Error = TEXT("Save already in progress.");
		OutResult.ResultCode = EARSaveResultCode::InProgress;
		BroadcastSaveFailure(OutResult);
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		OutResult.Error = TEXT("No world available for save.");
		OutResult.ResultCode = EARSaveResultCode::NoWorld;
		BroadcastSaveFailure(OutResult);
		return false;
	}

	if (World->GetNetMode() != NM_Standalone && World->GetAuthGameMode() == nullptr)
	{
		OutResult.Error = TEXT("SaveCurrentGame must run on authority/server for canonical snapshot.");
		OutResult.ResultCode = EARSaveResultCode::AuthorityRequired;
		BroadcastSaveFailure(OutResult);
		return false;
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UParleyDialogueSubsystem* DialogueSubsystem = GI->GetSubsystem<UParleyDialogueSubsystem>())
		{
			if (DialogueSubsystem->HasActiveDialogueSession())
			{
				OutResult.Error = TEXT("SaveCurrentGame blocked: game cannot be saved mid-conversation.");
				OutResult.ResultCode = EARSaveResultCode::ValidationFailed;
				BroadcastSaveFailure(OutResult);
				return false;
			}
		}
	}

	const FDateTime NowUtc = FDateTime::UtcNow();
	if (!bIgnoreThrottle && MinSaveIntervalSeconds > 0.f && LastSaveTimestampUtc.GetTicks() != 0)
	{
		const double Elapsed = (NowUtc - LastSaveTimestampUtc).GetTotalSeconds();
		if (Elapsed < MinSaveIntervalSeconds)
		{
			OutResult.Error = FString::Printf(TEXT("Save throttled (%.2fs < min %.2fs)."), Elapsed, MinSaveIntervalSeconds);
			OutResult.ResultCode = EARSaveResultCode::Throttled;
			BroadcastSaveFailure(OutResult);
			return false;
		}
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
	SlotBase = ARSaveInternal::NormalizeSlotBaseForNamespace(SlotBase, bUseDebugSaves);

	const TCHAR* IndexSlotName = ARSaveInternal::GetIndexSlotNameForNamespace(bUseDebugSaves);

	int32 ExistingLatest = -1;
	UARSaveIndexGame* IndexObj = nullptr;
	if (!LoadOrCreateIndexForSlot(IndexObj, OutResult, IndexSlotName))
	{
		OutResult.ResultCode = EARSaveResultCode::Unknown;
		BroadcastSaveFailure(OutResult);
		return false;
	}

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
		OutResult.ResultCode = EARSaveResultCode::ValidationFailed;
		BroadcastSaveFailure(OutResult);
		return false;
	}

	GatherRuntimeData(SaveObject);
	SaveObject->SaveSlot = ARSaveInternal::GetLogicalSlotBaseForNamespace(SlotBase, bUseDebugSaves);
	SaveObject->SaveSlotNumber = NewSlotNumber;
	SaveObject->SaveGameVersion = UARSaveGame::GetCurrentSchemaVersion();
	SaveObject->LastSaved = FDateTime::UtcNow();

	TArray<FString> Warnings;
	OutResult.ClampedFieldCount = SaveObject->ValidateAndSanitize(&Warnings);
	for (const FString& Warning : Warnings)
	{
		UE_LOG(ARLog, Warning, TEXT("[SaveSubsystem] %s"), *Warning);
	}

	const FName RevisionSlot = BuildRevisionSlotName(SlotBase, NewSlotNumber);
	PendingAsyncSaveGame = SaveObject;
	PendingAsyncSaveIndex = IndexObj;
	PendingAsyncSaveSlotBase = SlotBase;
	PendingAsyncSaveSlotNumber = NewSlotNumber;
	PendingAsyncIndexSlotName = IndexSlotName;
	bPendingAsyncSaveHasRemoteRecipients = false;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (AARPlayerController* PC = Cast<AARPlayerController>(It->Get()))
		{
			if (PC->GetNetMode() != NM_Standalone && !PC->IsLocalController())
			{
				bPendingAsyncSaveHasRemoteRecipients = true;
			}
		}
	}

	bSaveInProgress = true;
	OnSaveStarted.Broadcast();

	FAsyncSaveGameToSlotDelegate SaveDelegate;
	SaveDelegate.BindUObject(this, &UARSaveSubsystem::HandleAsyncCurrentGameSaveComplete);
	UGameplayStatics::AsyncSaveGameToSlot(SaveObject, RevisionSlot.ToString(), DefaultUserIndex, SaveDelegate);

	OutResult.bSuccess = true;
	OutResult.ResultCode = EARSaveResultCode::Success;
	OutResult.SlotName = SlotBase;
	OutResult.SlotNumber = NewSlotNumber;
	return true;
}

bool UARSaveSubsystem::LoadGame(FName SlotBaseName, int32 RevisionOrLatest, FARSaveResult& OutResult, bool bUseDebugSaves)
{
	OutResult = FARSaveResult();
	const FName SlotBase = ARSaveInternal::NormalizeSlotBaseForNamespace(NormalizeSlotBaseName(SlotBaseName), bUseDebugSaves);
	const TCHAR* IndexSlotName = ARSaveInternal::GetIndexSlotNameForNamespace(bUseDebugSaves);

	int32 ResolvedRevision = INDEX_NONE;
	UARSaveGame* LoadedSave = LoadSaveObjectWithRollback(SlotBase, RevisionOrLatest, ResolvedRevision, OutResult, IndexSlotName);
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

	TArray<FString> Warnings;
	OutResult.ClampedFieldCount = LoadedSave->ValidateAndSanitize(&Warnings);
	for (const FString& Warning : Warnings)
	{
		UE_LOG(ARLog, Warning, TEXT("[SaveSubsystem] %s"), *Warning);
	}

	OutResult.bSuccess = true;
	OutResult.SlotName = SlotBase;
	OutResult.SlotNumber = ResolvedRevision;
	ApplyLoadedSave(LoadedSave, OutResult);
	OnLoadCompleted.Broadcast(OutResult);
	OnGameLoaded.Broadcast();
	return true;
}

void UARSaveSubsystem::HandleAsyncCurrentGameSaveComplete(const FString& SlotName, const int32 UserIndex, const bool bWasSuccessful)
{
	(void)UserIndex;

	FARSaveResult Result;
	const FName SlotBase = PendingAsyncSaveSlotBase;
	const int32 SlotNumber = PendingAsyncSaveSlotNumber;
	UARSaveGame* SaveObject = PendingAsyncSaveGame.Get();
	UARSaveIndexGame* IndexObj = PendingAsyncSaveIndex.Get();
	const FString IndexSlotName = PendingAsyncIndexSlotName;
	const bool bHasRemoteRecipients = bPendingAsyncSaveHasRemoteRecipients;

	auto ClearPendingState = [this]()
	{
		bSaveInProgress = false;
		PendingAsyncSaveGame = nullptr;
		PendingAsyncSaveIndex = nullptr;
		PendingAsyncSaveSlotBase = NAME_None;
		PendingAsyncSaveSlotNumber = INDEX_NONE;
		PendingAsyncIndexSlotName.Reset();
		bPendingAsyncSaveHasRemoteRecipients = false;
	};

	if (!bWasSuccessful || !SaveObject || !IndexObj)
	{
		Result.bSuccess = false;
		Result.ResultCode = EARSaveResultCode::ValidationFailed;
		Result.Error = !bWasSuccessful
			? FString::Printf(TEXT("Async save failed for slot '%s'."), *SlotName)
			: TEXT("Async save completed without a live save object.");
		RollbackRevisionWrite(SlotBase, SlotNumber, Result);
		ClearPendingState();
		BroadcastSaveFailure(Result);
		return;
	}

	FARSaveSlotDescriptor Descriptor;
	Descriptor.SlotName = SlotBase;
	Descriptor.SlotNumber = SlotNumber;
	Descriptor.SaveVersion = SaveObject->SaveGameVersion;
	Descriptor.CyclesPlayed = SaveObject->Cycles;
	Descriptor.LastSavedTime = SaveObject->LastSaved;
	Descriptor.Money = SaveObject->Money;
	UpsertIndexEntry(IndexObj, Descriptor);

	TArray<uint8> IndexBytes;
	if (!UGameplayStatics::SaveGameToMemory(IndexObj, IndexBytes))
	{
		Result.bSuccess = false;
		Result.ResultCode = EARSaveResultCode::ValidationFailed;
		Result.Error = TEXT("Could not serialize save index to memory.");
		RollbackRevisionWrite(SlotBase, SlotNumber, Result);
		ClearPendingState();
		BroadcastSaveFailure(Result);
		return;
	}

	const int32 MaxBackups = GetMaxBackupRevisions();
	const int32 FirstRevisionToKeep = FMath::Max(0, SlotNumber - (MaxBackups - 1));
	TArray<FString> RevisionSlotsToDelete;
	for (int32 Revision = 0; Revision < FirstRevisionToKeep; ++Revision)
	{
		RevisionSlotsToDelete.Add(BuildRevisionSlotName(SlotBase, Revision).ToString());
	}

	TWeakObjectPtr<UARSaveSubsystem> WeakThis(this);
	Async(EAsyncExecution::ThreadPool, [WeakThis, IndexBytes = MoveTemp(IndexBytes), IndexSlotName, SlotBase, SlotNumber, SlotName, SaveObject, bHasRemoteRecipients, RevisionSlotsToDelete = MoveTemp(RevisionSlotsToDelete)]() mutable
	{
		const bool bIndexSaved = UGameplayStatics::SaveDataToSlot(IndexBytes, IndexSlotName, DefaultUserIndex);
		bool bPruneOk = true;
		if (bIndexSaved)
		{
			for (const FString& RevisionSlot : RevisionSlotsToDelete)
			{
				if (UGameplayStatics::DoesSaveGameExist(RevisionSlot, DefaultUserIndex))
				{
					bPruneOk = UGameplayStatics::DeleteGameInSlot(RevisionSlot, DefaultUserIndex) && bPruneOk;
				}
			}
		}

		AsyncTask(ENamedThreads::GameThread, [WeakThis, bIndexSaved, bPruneOk, SlotBase, SlotNumber, SlotName, SaveObject, bHasRemoteRecipients]() mutable
		{
			UARSaveSubsystem* StrongThis = WeakThis.Get();
			if (!StrongThis)
			{
				return;
			}

			FARSaveResult FinalResult;
			if (!bIndexSaved || !bPruneOk)
			{
				FinalResult.bSuccess = false;
				FinalResult.ResultCode = EARSaveResultCode::ValidationFailed;
				FinalResult.Error = !bIndexSaved
					? TEXT("Async save failed while writing the save index.")
					: TEXT("Async save failed while pruning old revisions.");
				StrongThis->RollbackRevisionWrite(SlotBase, SlotNumber, FinalResult);
				StrongThis->bSaveInProgress = false;
				StrongThis->PendingAsyncSaveGame = nullptr;
				StrongThis->PendingAsyncSaveIndex = nullptr;
				StrongThis->PendingAsyncSaveSlotBase = NAME_None;
				StrongThis->PendingAsyncSaveSlotNumber = INDEX_NONE;
				StrongThis->PendingAsyncIndexSlotName.Reset();
				StrongThis->bPendingAsyncSaveHasRemoteRecipients = false;
				StrongThis->BroadcastSaveFailure(FinalResult);
				return;
			}

			TArray<uint8> SaveBytes;
			if (UGameplayStatics::SaveGameToMemory(SaveObject, SaveBytes))
			{
				if (bHasRemoteRecipients)
				{
					int32 RemoteRecipientCount = 0;
					if (UWorld* World = StrongThis->GetWorld())
					{
						for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
						{
							if (AARPlayerController* PC = Cast<AARPlayerController>(It->Get()))
							{
								if (PC->GetNetMode() != NM_Standalone && !PC->IsLocalController())
								{
									++RemoteRecipientCount;
								}
							}
						}
					}

					StrongThis->LogCanonicalSavePayloadSize(TEXT("AsyncSaveFanOut"), SlotBase, SlotNumber, SaveBytes.Num(), RemoteRecipientCount);
					if (UWorld* World = StrongThis->GetWorld())
					{
						for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
						{
							if (AARPlayerController* PC = Cast<AARPlayerController>(It->Get()))
							{
								if (PC->GetNetMode() != NM_Standalone && !PC->IsLocalController())
								{
									PC->ClientPersistCanonicalSave(SaveBytes, SlotBase, SlotNumber);
								}
							}
						}
					}
				}
			}

			StrongThis->CurrentSaveGame = SaveObject;
			StrongThis->CurrentSlotBaseName = SlotBase;
			StrongThis->FlushPendingCanonicalSyncRequests();
			StrongThis->LastSaveTimestampUtc = SaveObject->LastSaved;
			StrongThis->bSaveDirty = false;

			FinalResult.bSuccess = true;
			FinalResult.ResultCode = EARSaveResultCode::Success;
			FinalResult.SlotName = SlotBase;
			FinalResult.SlotNumber = SlotNumber;

			if (StrongThis->bLogSaveSuccess)
			{
				UE_LOG(ARLog, Log, TEXT("[SaveSubsystem] Async save succeeded (Slot=%s Rev=%d Time=%s DirtyCleared=%s)"),
					*SlotBase.ToString(),
					SlotNumber,
					*SaveObject->LastSaved.ToString(),
					StrongThis->bSaveDirty ? TEXT("false") : TEXT("true"));
			}

			StrongThis->bSaveInProgress = false;
			StrongThis->PendingAsyncSaveGame = nullptr;
			StrongThis->PendingAsyncSaveIndex = nullptr;
			StrongThis->PendingAsyncSaveSlotBase = NAME_None;
			StrongThis->PendingAsyncSaveSlotNumber = INDEX_NONE;
			StrongThis->PendingAsyncIndexSlotName.Reset();
			StrongThis->bPendingAsyncSaveHasRemoteRecipients = false;
			StrongThis->OnSaveCompleted.Broadcast(FinalResult);
		});
	});
}

bool UARSaveSubsystem::ListSaves(TArray<FARSaveSlotDescriptor>& OutSlots, FARSaveResult& OutResult, bool bUseDebugSaves) const
{
	OutSlots.Reset();
	OutResult = FARSaveResult();

	UARSaveIndexGame* IndexObj = nullptr;
	if (!LoadOrCreateIndexForSlot(IndexObj, OutResult, ARSaveInternal::GetIndexSlotNameForNamespace(bUseDebugSaves)))
	{
		return false;
	}

	OutSlots = IndexObj->SlotNames;
	OutResult.bSuccess = true;
	return true;
}

bool UARSaveSubsystem::DeleteSave(FName SlotBaseName, FARSaveResult& OutResult, bool bUseDebugSaves)
{
	OutResult = FARSaveResult();
	const FName SlotBase = ARSaveInternal::NormalizeSlotBaseForNamespace(NormalizeSlotBaseName(SlotBaseName), bUseDebugSaves);
	const TCHAR* IndexSlotName = ARSaveInternal::GetIndexSlotNameForNamespace(bUseDebugSaves);

	UARSaveIndexGame* IndexObj = nullptr;
	if (!LoadOrCreateIndexForSlot(IndexObj, OutResult, IndexSlotName))
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
	if (!SaveIndexForSlot(IndexObj, OutResult, IndexSlotName))
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

	if (CurrentSaveGame->LastSaved.GetTicks() == 0)
	{
		QueuePendingCanonicalSyncRequest(TargetPlayerController);
		OutResult.Error = TEXT("PushCurrentSaveToPlayer deferred: current save has not been persisted yet.");
		OutResult.ResultCode = EARSaveResultCode::ValidationFailed;
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

	LogCanonicalSavePayloadSize(TEXT("PushCurrentSaveToPlayer"), SlotBase, Revision, SaveBytes.Num(), 1);
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

	// Travel-transient GameState data overlays persisted/default fields on first hydration pass after travel.
	if (PendingTravelGameStateData.IsValid())
	{
		if (CurrentSaveGame)
		{
			ARSaveInternal::ApplySavedGameStateFieldsToRuntime(Requester, CurrentSaveGame);
			Requester->SyncCyclesFromSave(CurrentSaveGame->Cycles);
		}

		IStructSerializable::Execute_ApplyStateFromStruct(Requester, PendingTravelGameStateData);
		PendingTravelGameStateData.Reset();
		Requester->NotifyHydratedFromSave();
		return;
	}

	if (!CurrentSaveGame)
	{
		if (const UARLoadoutSettings* LoadoutSettings = GetDefault<UARLoadoutSettings>())
		{
			Requester->SetUnlocksFromSave(LoadoutSettings->GetEffectiveDefaultStartingUnlocks());
		}
		Requester->NotifyHydratedFromSave();
		return;
	}

	ARSaveInternal::ApplySavedGameStateFieldsToRuntime(Requester, CurrentSaveGame);
	Requester->SyncCyclesFromSave(CurrentSaveGame->Cycles);
	Requester->NotifyHydratedFromSave();
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

bool UARSaveSubsystem::TryHydratePlayerStateFromCurrentSave(AARPlayerStateBase* Requester)
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

	const FARPlayerIdentity QueryIdentity = BuildRuntimePlayerIdentity(Requester);
	int32 Index = INDEX_NONE;
	if (!ResolvePlayerSaveDataIndex(CurrentSaveGame, QueryIdentity, Index))
	{
		return false;
	}

	Requester->ApplyPlayerSaveData(CurrentSaveGame->PlayerStates[Index]);
	return true;
}

bool UARSaveSubsystem::AdvanceWorldDays(int32 DeltaDays, bool bPersistImmediately, FARSaveResult& OutResult)
{
	OutResult = FARSaveResult();

	UWorld* World = GetWorld();
	if (!World)
	{
		OutResult.Error = TEXT("No world available.");
		return false;
	}

	if (World->GetNetMode() != NM_Standalone && World->GetAuthGameMode() == nullptr)
	{
		OutResult.Error = TEXT("AdvanceWorldDays must run on authority/server.");
		return false;
	}

	if (!CurrentSaveGame)
	{
		// Create an initial save so cycles have a home.
		if (!SaveCurrentGameBlocking(NAME_None, true, OutResult))
		{
			return false;
		}
	}

	if (DeltaDays != 0)
	{
		const int32 NewCycles = FMath::Max(0, CurrentSaveGame->Cycles + DeltaDays);
		CurrentSaveGame->Cycles = NewCycles;
		bSaveDirty = true;
		if (AARGameStateBase* GS = World->GetGameState<AARGameStateBase>())
		{
			GS->SyncCyclesFromSave(NewCycles);
		}
	}

	if (!bPersistImmediately)
	{
		OutResult.bSuccess = true;
		OutResult.ResultCode = EARSaveResultCode::Success;
		return true;
	}

	// Persist to the current slot (no forced new revision unless configured in SaveCurrentGame).
	const bool bSaved = SaveCurrentGameBlocking(CurrentSlotBaseName, true, OutResult);
	return bSaved;
}

bool UARSaveSubsystem::GetTimeSinceLastSave(FTimespan& OutElapsed) const
{
	if (!CurrentSaveGame || CurrentSaveGame->LastSaved.GetTicks() == 0)
	{
		return false;
	}

	const FDateTime NowUtc = FDateTime::UtcNow();
	OutElapsed = NowUtc - CurrentSaveGame->LastSaved;
	return OutElapsed.GetTicks() >= 0;
}

bool UARSaveSubsystem::GetLastSaveTimestamp(FDateTime& OutTimestampUtc) const
{
	if (!CurrentSaveGame || CurrentSaveGame->LastSaved.GetTicks() == 0)
	{
		return false;
	}
	OutTimestampUtc = CurrentSaveGame->LastSaved;
	return true;
}

bool UARSaveSubsystem::FormatTimeSinceLastSave(FText& OutText) const
{
	FTimespan Elapsed;
	if (!GetTimeSinceLastSave(Elapsed))
	{
		return false;
	}
	OutText = FText::AsTimespan(Elapsed);
	return true;
}

void UARSaveSubsystem::MarkSaveDirty()
{
	bSaveDirty = true;
}

FGameplayTagContainer UARSaveSubsystem::GetProgressionTags() const
{
	if (!CurrentSaveGame)
	{
		return FGameplayTagContainer();
	}

	return CurrentSaveGame->ProgressionTags;
}

bool UARSaveSubsystem::HasProgressionTag(FGameplayTag ProgressionTag) const
{
	return CurrentSaveGame && ProgressionTag.IsValid() && CurrentSaveGame->ProgressionTags.HasTag(ProgressionTag);
}

bool UARSaveSubsystem::GetPlayerProgressionTags(AARPlayerStateBase* Requester, FGameplayTagContainer& OutTags) const
{
	OutTags.Reset();
	if (!Requester || !CurrentSaveGame)
	{
		return false;
	}

	const FARPlayerIdentity QueryIdentity = BuildRuntimePlayerIdentity(Requester);
	int32 PlayerIndex = INDEX_NONE;
	if (!ResolvePlayerSaveDataIndex(CurrentSaveGame, QueryIdentity, PlayerIndex))
	{
		return false;
	}

	OutTags = CurrentSaveGame->PlayerStates[PlayerIndex].ProgressionTags;
	return true;
}

bool UARSaveSubsystem::HasPlayerProgressionTag(AARPlayerStateBase* Requester, const FGameplayTag ProgressionTag) const
{
	if (!ProgressionTag.IsValid())
	{
		return false;
	}

	FGameplayTagContainer OutTags;
	return GetPlayerProgressionTags(Requester, OutTags) && OutTags.HasTag(ProgressionTag);
}

bool UARSaveSubsystem::AddProgressionTag(FGameplayTag ProgressionTag)
{
	if (!CurrentSaveGame || !ProgressionTag.IsValid())
	{
		return false;
	}

	if (CurrentSaveGame->ProgressionTags.HasTagExact(ProgressionTag))
	{
		return false;
	}

	CurrentSaveGame->ProgressionTags.AddTag(ProgressionTag);
	MarkSaveDirty();
	return true;
}

bool UARSaveSubsystem::AddPlayerProgressionTag(AARPlayerStateBase* Requester, const FGameplayTag ProgressionTag)
{
	if (!Requester || !CurrentSaveGame || !ProgressionTag.IsValid())
	{
		return false;
	}

	const FARPlayerIdentity QueryIdentity = BuildRuntimePlayerIdentity(Requester);
	int32 PlayerIndex = INDEX_NONE;
	if (!ResolvePlayerSaveDataIndex(CurrentSaveGame, QueryIdentity, PlayerIndex))
	{
		FARPlayerStateSaveData& AddedPlayerData = CurrentSaveGame->PlayerStates.AddDefaulted_GetRef();
		AddedPlayerData.Identity = QueryIdentity;
		const AARCharacterStateRuntime* Runtime = Requester->GetCurrentCharacterRuntime();
		AddedPlayerData.CurrentCharacterTag = Runtime
			? ARPlayer::NormalizeCharacterTag(Runtime->GetCharacterTag())
			: ARPlayer::NormalizeCharacterTag(Requester->GetCurrentCharacterTag());
		AddedPlayerData.bDialogueAutoAdvanceEnabled = Requester->IsDialogueAutoAdvanceEnabled();
		if (AddedPlayerData.CurrentCharacterTag.IsValid())
		{
			FARCharacterSaveData& ActiveCharacterState = CurrentSaveGame->FindOrAddCharacterStateData(AddedPlayerData.CurrentCharacterTag);
			if (const AARCharacterStateRuntime* CurrentRuntime = Requester->GetCurrentCharacterRuntime())
			{
				CurrentRuntime->WriteSaveData(ActiveCharacterState);
			}
		}
		AddedPlayerData.SyncCharacterSelectionFromCurrentTag();
		PlayerIndex = CurrentSaveGame->PlayerStates.Num() - 1;
	}

	FARPlayerStateSaveData& PlayerData = CurrentSaveGame->PlayerStates[PlayerIndex];
	if (PlayerData.ProgressionTags.HasTagExact(ProgressionTag))
	{
		return false;
	}

	PlayerData.ProgressionTags.AddTag(ProgressionTag);
	MarkSaveDirty();
	return true;
}

bool UARSaveSubsystem::RemoveProgressionTag(FGameplayTag ProgressionTag)
{
	if (!CurrentSaveGame || !ProgressionTag.IsValid())
	{
		return false;
	}

	if (!CurrentSaveGame->ProgressionTags.HasTagExact(ProgressionTag))
	{
		return false;
	}

	CurrentSaveGame->ProgressionTags.RemoveTag(ProgressionTag);
	MarkSaveDirty();
	return true;
}

bool UARSaveSubsystem::RemovePlayerProgressionTag(AARPlayerStateBase* Requester, const FGameplayTag ProgressionTag)
{
	if (!Requester || !CurrentSaveGame || !ProgressionTag.IsValid())
	{
		return false;
	}

	const FARPlayerIdentity QueryIdentity = BuildRuntimePlayerIdentity(Requester);
	int32 PlayerIndex = INDEX_NONE;
	if (!ResolvePlayerSaveDataIndex(CurrentSaveGame, QueryIdentity, PlayerIndex))
	{
		return false;
	}

	FARPlayerStateSaveData& PlayerData = CurrentSaveGame->PlayerStates[PlayerIndex];
	if (!PlayerData.ProgressionTags.HasTagExact(ProgressionTag))
	{
		return false;
	}

	PlayerData.ProgressionTags.RemoveTag(ProgressionTag);
	MarkSaveDirty();
	return true;
}

int32 UARSaveSubsystem::GetFactionClout() const
{
	return CurrentSaveGame ? FMath::Max(0, CurrentSaveGame->FactionClout) : 0;
}

void UARSaveSubsystem::SetFactionClout(int32 NewFactionClout)
{
	if (!CurrentSaveGame)
	{
		return;
	}

	const int32 Clamped = FMath::Max(0, NewFactionClout);
	if (CurrentSaveGame->FactionClout == Clamped)
	{
		return;
	}

	CurrentSaveGame->FactionClout = Clamped;
	MarkSaveDirty();
}

bool UARSaveSubsystem::RequestAutosaveIfDirty(bool bCreateNewRevision, FARSaveResult& OutResult)
{
	if (!bSaveDirty)
	{
		OutResult = FARSaveResult();
		OutResult.ResultCode = EARSaveResultCode::Success;
		return false;
	}

	const bool bSaved = SaveCurrentGame(CurrentSlotBaseName, bCreateNewRevision, OutResult);
	return bSaved;
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
	LastSaveTimestampUtc = LoadedSave->LastSaved;
	bSaveDirty = false;
	bPendingFreshLoadEntry = true;
	PendingLoadedSaveModeTag = LoadedSave->LastSavedModeTag;
	PendingLoadedSaveMapPath = LoadedSave->LastSavedMapPath;
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

	if (UWorld* World = GetWorld())
	{
		if (AARGameStateBase* GameState = World->GetGameState<AARGameStateBase>())
		{
			if (GameState->HasAuthority())
			{
				UE_LOG(
					ARLog,
					Log,
					TEXT("[SaveSubsystem] Rehydrating live GameState '%s' after load of slot '%s' rev=%d."),
					*GetNameSafe(GameState),
					*LoadResult.SlotName.ToString(),
					LoadResult.SlotNumber);
				RequestGameStateHydration(GameState);
			}
		}
	}

	// Loading a save can change dialogue availability; refresh speaker talkable caches/widgets immediately.
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UParleySpeakerSubsystem* SpeakerSubsystem = GI->GetSubsystem<UParleySpeakerSubsystem>())
		{
			SpeakerSubsystem->RefreshAllSpeakerTalkableStates();
		}
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
