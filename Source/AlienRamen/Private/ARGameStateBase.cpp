#include "ARGameStateBase.h"

#include "ARItemDefinitionSubsystem.h"
#include "ARLog.h"
#include "ARPlayerStateBase.h"
#include "ARSaveGame.h"
#include "ARSaveSubsystem.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagsManager.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

namespace
{
	static FGameplayTag GetGameUnlockRootTag()
	{
		return FGameplayTag::RequestGameplayTag(TEXT("Progression.Game.Unlock"), false);
	}

	static bool IsGameUnlockTag(const FGameplayTag& Tag)
	{
		const FGameplayTag UnlockRootTag = GetGameUnlockRootTag();
		return Tag.IsValid() && UnlockRootTag.IsValid() && Tag.MatchesTag(UnlockRootTag);
	}

	static FGameplayTagContainer FilterUnlockTagsFromGameProgression(const FGameplayTagContainer& SourceTags)
	{
		FGameplayTagContainer FilteredTags;
		for (const FGameplayTag Tag : SourceTags)
		{
			if (IsGameUnlockTag(Tag))
			{
				FilteredTags.AddTag(Tag);
			}
		}

		return FilteredTags;
	}

	static void ReplaceUnlockSubsetInGameProgression(const FGameplayTagContainer& UnlockTags, FGameplayTagContainer& InOutGameProgressionTags)
	{
		TArray<FGameplayTag> ExistingTags;
		InOutGameProgressionTags.GetGameplayTagArray(ExistingTags);
		for (const FGameplayTag ExistingTag : ExistingTags)
		{
			if (IsGameUnlockTag(ExistingTag))
			{
				InOutGameProgressionTags.RemoveTag(ExistingTag);
			}
		}

		InOutGameProgressionTags.AppendTags(UnlockTags);
	}

	static uint8 GetPauseVoteBitForPlayerSlotId(const int32 PlayerSlotId)
	{
		if (PlayerSlotId < 1 || PlayerSlotId > 8)
		{
			return 0;
		}

		return static_cast<uint8>(1u << (PlayerSlotId - 1));
	}

	static uint8 GetExternalPauseReasonBit(const EARPauseExternalReason Reason)
	{
		switch (Reason)
		{
		case EARPauseExternalReason::DialogueShared:
			return 1 << 0;
		case EARPauseExternalReason::InvaderFullBlast:
			return 1 << 1;
		default:
			return 0;
		}
	}

	static FARMeatState SanitizeMeatState(const FARMeatState& InMeat)
	{
		FARMeatState OutMeat = InMeat;
		for (FARMeatTypeAmount& Entry : OutMeat.AdditionalAmountsByType)
		{
			Entry.Amount = FMath::Max(0, Entry.Amount);
			Entry.MeatColor = Entry.MeatColor == EARAffinityColor::Unknown ? EARAffinityColor::None : Entry.MeatColor;
			if (!StaticEnum<EARVendingQualityTier>()->IsValidEnumValue(static_cast<int64>(Entry.MeatQualityTier)))
			{
				Entry.MeatQualityTier = EARVendingQualityTier::Standard;
			}
		}
		OutMeat.NormalizeAdditionalAmounts();

		return OutMeat;
	}

	static bool AreMeatStatesEqual(const FARMeatState& A, const FARMeatState& B)
	{
		const FARMeatState Left = SanitizeMeatState(A);
		const FARMeatState Right = SanitizeMeatState(B);

		if (Left.AdditionalAmountsByType.Num() != Right.AdditionalAmountsByType.Num())
		{
			return false;
		}

		for (int32 Index = 0; Index < Left.AdditionalAmountsByType.Num(); ++Index)
		{
			const FARMeatTypeAmount& LeftEntry = Left.AdditionalAmountsByType[Index];
			const FARMeatTypeAmount& RightEntry = Right.AdditionalAmountsByType[Index];
			if (LeftEntry.MeatType != RightEntry.MeatType
				|| LeftEntry.MeatColor != RightEntry.MeatColor
				|| LeftEntry.MeatQualityTier != RightEntry.MeatQualityTier
				|| LeftEntry.Amount != RightEntry.Amount)
			{
				return false;
			}
		}

		return true;
	}

	static bool TryParseMeatColorToken(const FString& Token, EARAffinityColor& OutColor)
	{
		const FString Normalized = Token.TrimStartAndEnd().ToLower();
		if (Normalized == TEXT("red"))
		{
			OutColor = EARAffinityColor::Red;
			return true;
		}
		if (Normalized == TEXT("blue"))
		{
			OutColor = EARAffinityColor::Blue;
			return true;
		}
		if (Normalized == TEXT("white"))
		{
			OutColor = EARAffinityColor::White;
			return true;
		}
		if (Normalized == TEXT("colorless"))
		{
			OutColor = EARAffinityColor::Colorless;
			return true;
		}
		if (Normalized == TEXT("none"))
		{
			OutColor = EARAffinityColor::None;
			return true;
		}

		return false;
	}

	static bool TryParseMeatQualityToken(const FString& Token, EARVendingQualityTier& OutQualityTier)
	{
		const FString Normalized = Token.TrimStartAndEnd().ToLower();
		if (Normalized == TEXT("low"))
		{
			OutQualityTier = EARVendingQualityTier::Low;
			return true;
		}
		if (Normalized == TEXT("standard"))
		{
			OutQualityTier = EARVendingQualityTier::Standard;
			return true;
		}
		if (Normalized == TEXT("high"))
		{
			OutQualityTier = EARVendingQualityTier::High;
			return true;
		}
		if (Normalized == TEXT("premium"))
		{
			OutQualityTier = EARVendingQualityTier::Premium;
			return true;
		}

		return false;
	}

	static void MarkCanonicalSaveDirty(const AARGameStateBase* GameState)
	{
		if (!GameState)
		{
			return;
		}

		const UGameInstance* GameInstance = GameState->GetGameInstance();
		UARSaveSubsystem* SaveSubsystem = GameInstance ? GameInstance->GetSubsystem<UARSaveSubsystem>() : nullptr;
		if (SaveSubsystem && SaveSubsystem->GetCurrentSaveGame())
		{
			SaveSubsystem->MarkSaveDirty();
		}
	}

	static void AddTypedMeatDelta(
		FARMeatState& InOutMeatState,
		const FGameplayTag MeatTag,
		const EARAffinityColor MeatColor,
		const EARVendingQualityTier MeatQualityTier,
		const int32 Delta)
	{
		if (!MeatTag.IsValid() || Delta == 0)
		{
			return;
		}

		const EARAffinityColor SanitizedColor = MeatColor == EARAffinityColor::Unknown ? EARAffinityColor::None : MeatColor;
		const EARVendingQualityTier SanitizedQuality = StaticEnum<EARVendingQualityTier>()->IsValidEnumValue(static_cast<int64>(MeatQualityTier))
			? MeatQualityTier
			: EARVendingQualityTier::Standard;

		for (FARMeatTypeAmount& Entry : InOutMeatState.AdditionalAmountsByType)
		{
			if (!Entry.MeatType.MatchesTagExact(MeatTag)
				|| Entry.MeatColor != SanitizedColor
				|| Entry.MeatQualityTier != SanitizedQuality)
			{
				continue;
			}

			Entry.Amount = FMath::Max(0, Entry.Amount + Delta);
			InOutMeatState.NormalizeAdditionalAmounts();
			return;
		}

		if (Delta <= 0)
		{
			return;
		}

		FARMeatTypeAmount& Added = InOutMeatState.AdditionalAmountsByType.AddDefaulted_GetRef();
		Added.MeatType = MeatTag;
		Added.MeatColor = SanitizedColor;
		Added.MeatQualityTier = SanitizedQuality;
		Added.Amount = Delta;
		InOutMeatState.NormalizeAdditionalAmounts();
	}
}

AARGameStateBase::AARGameStateBase()
{
	bReplicates = true;
}

void AARGameStateBase::BeginPlay()
{
	Super::BeginPlay();

	for (APlayerState* PlayerState : PlayerArray)
	{
		BindPlayerStateSignals(Cast<AARPlayerStateBase>(PlayerState));
	}

	if (HasAuthority())
	{
		RefreshAllPlayersTravelReady();
		RefreshPauseResolution();
	}
	else
	{
		// Client-side listeners can still query immediately; replicated value remains authoritative.
		bAllPlayersTravelReady = ComputeAllPlayersTravelReady();
	}

	if (!HasAuthority())
	{
		return;
	}

	RegisterDebugConsoleCommands();

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UARSaveSubsystem* SaveSubsystem = GI->GetSubsystem<UARSaveSubsystem>())
		{
			SaveSubsystem->RequestGameStateHydration(this);
		}
	}
}

void AARGameStateBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		UnregisterDebugConsoleCommands();
	}
	Super::EndPlay(EndPlayReason);
}

void AARGameStateBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AARGameStateBase, bAllPlayersTravelReady);
	DOREPLIFETIME(AARGameStateBase, Unlocks);
	DOREPLIFETIME(AARGameStateBase, GameProgressionTags);
	DOREPLIFETIME(AARGameStateBase, bHasHydratedFromSave);
	DOREPLIFETIME(AARGameStateBase, Money);
	DOREPLIFETIME(AARGameStateBase, Scrap);
	DOREPLIFETIME(AARGameStateBase, Meat);
	DOREPLIFETIME(AARGameStateBase, RunLedgerScrap);
	DOREPLIFETIME(AARGameStateBase, RunLedgerMeat);
	DOREPLIFETIME(AARGameStateBase, Cycles);
	DOREPLIFETIME(AARGameStateBase, ActiveFactionTag);
	DOREPLIFETIME(AARGameStateBase, ActiveFactionEffectTags);
	DOREPLIFETIME(AARGameStateBase, bManualSaveAllowed);
	DOREPLIFETIME(AARGameStateBase, bShareLocalPauseAcrossControllers);
	DOREPLIFETIME(AARGameStateBase, PauseMenuVoteMask);
	DOREPLIFETIME(AARGameStateBase, ExternalPauseReasonMask);
	DOREPLIFETIME(AARGameStateBase, bAllPlayersPausedByMenu);
	DOREPLIFETIME(AARGameStateBase, bAnyExternalPauseActive);
	DOREPLIFETIME(AARGameStateBase, bEffectivePauseStateActive);
}

TArray<AARPlayerStateBase*> AARGameStateBase::GetPlayerStates() const
{
	TArray<AARPlayerStateBase*> Result;
	Result.Reserve(PlayerArray.Num());

	for (APlayerState* PlayerState : PlayerArray)
	{
		if (AARPlayerStateBase* ARPlayerState = Cast<AARPlayerStateBase>(PlayerState))
		{
			Result.Add(ARPlayerState);
		}
	}

	return Result;
}

AARPlayerStateBase* AARGameStateBase::GetPlayerStateByCharacterTag(const FGameplayTag CharacterTag) const
{
	const FGameplayTag NormalizedCharacterTag = ARPlayer::NormalizeCharacterTag(CharacterTag);
	if (!NormalizedCharacterTag.IsValid())
	{
		return nullptr;
	}

	for (APlayerState* PlayerState : PlayerArray)
	{
		AARPlayerStateBase* ARPlayerState = Cast<AARPlayerStateBase>(PlayerState);
		if (!ARPlayerState)
		{
			continue;
		}

		const FGameplayTag RuntimeCharacterTag = ARPlayer::NormalizeCharacterTag(ARPlayerState->GetCurrentCharacterTag());
		if (RuntimeCharacterTag.IsValid() && RuntimeCharacterTag.MatchesTagExact(NormalizedCharacterTag))
		{
			return ARPlayerState;
		}
	}

	return nullptr;
}

APlayerController* AARGameStateBase::GetControllerByCharacterTag(const FGameplayTag CharacterTag) const
{
	const AARPlayerStateBase* PlayerState = GetPlayerStateByCharacterTag(CharacterTag);
	if (!PlayerState)
	{
		return nullptr;
	}

	return Cast<APlayerController>(PlayerState->GetOwner());
}

bool AARGameStateBase::IsCharacterControlled(const FGameplayTag CharacterTag) const
{
	return GetPlayerStateByCharacterTag(CharacterTag) != nullptr;
}

bool AARGameStateBase::AreAllPlayersTravelReady() const
{
	return bAllPlayersTravelReady;
}

AARPlayerStateBase* AARGameStateBase::GetOtherPlayerStateFromPlayerState(const AARPlayerStateBase* CurrentPlayerState) const
{
	for (APlayerState* PS : PlayerArray)
	{
		AARPlayerStateBase* Player = Cast<AARPlayerStateBase>(PS);
		if (!IsValid(Player) || Player == CurrentPlayerState)
		{
			continue;
		}
		return Player;
	}

	return nullptr;
}

AARPlayerStateBase* AARGameStateBase::GetOtherPlayerStateFromController(const APlayerController* CurrentPlayerController) const
{
	if (!CurrentPlayerController)
	{
		return GetOtherPlayerStateFromPlayerState(nullptr);
	}

	return GetOtherPlayerStateFromPlayerState(CurrentPlayerController->GetPlayerState<AARPlayerStateBase>());
}

AARPlayerStateBase* AARGameStateBase::GetOtherPlayerStateFromPawn(const APawn* CurrentPlayerPawn) const
{
	if (!CurrentPlayerPawn)
	{
		return GetOtherPlayerStateFromPlayerState(nullptr);
	}

	return GetOtherPlayerStateFromPlayerState(CurrentPlayerPawn->GetPlayerState<AARPlayerStateBase>());
}

void AARGameStateBase::AddPlayerState(APlayerState* PlayerState)
{
	Super::AddPlayerState(PlayerState);
	if (AARPlayerStateBase* ARPlayerState = Cast<AARPlayerStateBase>(PlayerState))
	{
		BindPlayerStateSignals(ARPlayerState);
		OnTrackedPlayersChanged.Broadcast();

		if (HasAuthority())
		{
			RefreshAllPlayersTravelReady();
			RefreshPauseResolution();
		}
	}
}

void AARGameStateBase::RemovePlayerState(APlayerState* PlayerState)
{
	if (HasAuthority())
	{
		if (const AARPlayerStateBase* ARPlayerState = Cast<AARPlayerStateBase>(PlayerState))
		{
			ClearPauseVoteForPlayerSlotId(ARPlayerState->GetPlayerSlotId());
		}
	}

	if (AARPlayerStateBase* ARPlayerState = Cast<AARPlayerStateBase>(PlayerState))
	{
		UnbindPlayerStateSignals(ARPlayerState);
	}

	Super::RemovePlayerState(PlayerState);
	if (Cast<AARPlayerStateBase>(PlayerState))
	{
		OnTrackedPlayersChanged.Broadcast();

		if (HasAuthority())
		{
			RefreshAllPlayersTravelReady();
			RefreshPauseResolution();
		}
	}
}

void AARGameStateBase::HandlePlayerReadyStatusChanged(AARPlayerStateBase* SourcePlayerState, FGameplayTag SourceCharacterTag, bool bNewReady, bool bOldReady)
{
	OnPlayerReadyChanged.Broadcast(SourcePlayerState, SourceCharacterTag, bNewReady, bOldReady);

	if (HasAuthority())
	{
		RefreshAllPlayersTravelReady();
	}
}

void AARGameStateBase::HandlePlayerCharacterPickedChanged(AARPlayerStateBase* SourcePlayerState, FGameplayTag SourceCharacterTag, EARCharacterChoice NewCharacter, EARCharacterChoice OldCharacter)
{
	(void)SourcePlayerState;
	(void)SourceCharacterTag;

	if (HasAuthority() && NewCharacter != OldCharacter)
	{
		RefreshAllPlayersTravelReady();
	}
}

void AARGameStateBase::OnRep_AllPlayersTravelReady(bool bOldAllPlayersTravelReady)
{
	OnAllPlayersTravelReadyChanged.Broadcast(bAllPlayersTravelReady, bOldAllPlayersTravelReady);
}

bool AARGameStateBase::ApplyStateFromStruct_Implementation(const FInstancedStruct& SavedState)
{
	if (!SavedState.IsValid())
	{
		return false;
	}

	if (!HasAuthority())
	{
		UE_LOG(ARLog, Warning, TEXT("[GameState] ApplyStateFromStruct rejected on non-authority '%s'."), *GetNameSafe(this));
		return false;
	}

	return IStructSerializable::ApplyStateFromStruct_Implementation(SavedState);
}

void AARGameStateBase::SyncCyclesFromSave(int32 NewCycles)
{
	if (!HasAuthority())
	{
		return;
	}

	const int32 Clamped = FMath::Max(0, NewCycles);
	if (Cycles == Clamped)
	{
		return;
	}

	const int32 OldCycles = Cycles;
	Cycles = Clamped;
	OnRep_Cycles(OldCycles);
	ForceNetUpdate();
}

void AARGameStateBase::SetUnlocksFromSave(const FGameplayTagContainer& NewUnlocks)
{
	if (!HasAuthority())
	{
		return;
	}

	if (Unlocks == NewUnlocks)
	{
		return;
	}

	const FGameplayTagContainer OldUnlocks = Unlocks;
	Unlocks = NewUnlocks;
	ReplaceUnlockSubsetInGameProgression(Unlocks, GameProgressionTags);
	OnRep_Unlocks(OldUnlocks);
	ForceNetUpdate();

	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (UARSaveSubsystem* SaveSubsystem = GameInstance->GetSubsystem<UARSaveSubsystem>())
		{
			if (UARSaveGame* SaveGame = SaveSubsystem->GetCurrentSaveGame())
			{
				SaveGame->Unlocks = Unlocks;
				SaveGame->GameProgressionTags = GameProgressionTags;
			}
		}
	}

	MarkCanonicalSaveDirty(this);
}

void AARGameStateBase::SetGameProgressionTagsFromSave(const FGameplayTagContainer& NewGameProgressionTags)
{
	if (!HasAuthority())
	{
		return;
	}

	if (GameProgressionTags == NewGameProgressionTags)
	{
		return;
	}

	GameProgressionTags = NewGameProgressionTags;
	Unlocks = FilterUnlockTagsFromGameProgression(GameProgressionTags);
	ForceNetUpdate();

	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (UARSaveSubsystem* SaveSubsystem = GameInstance->GetSubsystem<UARSaveSubsystem>())
		{
			if (UARSaveGame* SaveGame = SaveSubsystem->GetCurrentSaveGame())
			{
				SaveGame->GameProgressionTags = GameProgressionTags;
				SaveGame->Unlocks = Unlocks;
			}
		}
	}

	MarkCanonicalSaveDirty(this);
}

bool AARGameStateBase::AddGameProgressionTag(const FGameplayTag& ProgressionTag)
{
	if (!HasAuthority() || !ProgressionTag.IsValid() || GameProgressionTags.HasTagExact(ProgressionTag))
	{
		return false;
	}

	GameProgressionTags.AddTag(ProgressionTag);
	if (IsGameUnlockTag(ProgressionTag))
	{
		const FGameplayTagContainer OldUnlocks = Unlocks;
		Unlocks.AddTag(ProgressionTag);
		OnRep_Unlocks(OldUnlocks);
	}

	ForceNetUpdate();

	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (UARSaveSubsystem* SaveSubsystem = GameInstance->GetSubsystem<UARSaveSubsystem>())
		{
			if (UARSaveGame* SaveGame = SaveSubsystem->GetCurrentSaveGame())
			{
				SaveGame->GameProgressionTags = GameProgressionTags;
				SaveGame->Unlocks = Unlocks;
			}
		}
	}

	MarkCanonicalSaveDirty(this);
	return true;
}

bool AARGameStateBase::RemoveGameProgressionTag(const FGameplayTag& ProgressionTag)
{
	if (!HasAuthority() || !ProgressionTag.IsValid() || !GameProgressionTags.HasTagExact(ProgressionTag))
	{
		return false;
	}

	GameProgressionTags.RemoveTag(ProgressionTag);
	if (IsGameUnlockTag(ProgressionTag))
	{
		const FGameplayTagContainer OldUnlocks = Unlocks;
		Unlocks.RemoveTag(ProgressionTag);
		OnRep_Unlocks(OldUnlocks);
	}

	ForceNetUpdate();

	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (UARSaveSubsystem* SaveSubsystem = GameInstance->GetSubsystem<UARSaveSubsystem>())
		{
			if (UARSaveGame* SaveGame = SaveSubsystem->GetCurrentSaveGame())
			{
				SaveGame->GameProgressionTags = GameProgressionTags;
				SaveGame->Unlocks = Unlocks;
			}
		}
	}

	MarkCanonicalSaveDirty(this);
	return true;
}

bool AARGameStateBase::HasGameProgressionTag(const FGameplayTag& ProgressionTag) const
{
	return ProgressionTag.IsValid() && GameProgressionTags.HasTag(ProgressionTag);
}

bool AARGameStateBase::AddUnlockTag(const FGameplayTag& UnlockTag)
{
	if (!HasAuthority() || !UnlockTag.IsValid())
	{
		return false;
	}

	if (Unlocks.HasTagExact(UnlockTag))
	{
		return false;
	}

	return AddGameProgressionTag(UnlockTag);
}

bool AARGameStateBase::RemoveUnlockTag(const FGameplayTag& UnlockTag)
{
	if (!HasAuthority() || !UnlockTag.IsValid())
	{
		return false;
	}

	if (!Unlocks.HasTagExact(UnlockTag))
	{
		return false;
	}

	return RemoveGameProgressionTag(UnlockTag);
}

bool AARGameStateBase::HasUnlockTag(const FGameplayTag& UnlockTag) const
{
	return UnlockTag.IsValid() && Unlocks.HasTag(UnlockTag);
}

void AARGameStateBase::SetMoneyFromSave(int32 NewMoney)
{
	if (!HasAuthority())
	{
		return;
	}

	const int32 Clamped = FMath::Max(0, NewMoney);
	if (Money == Clamped)
	{
		return;
	}

	const int32 OldMoney = Money;
	Money = Clamped;
	OnRep_Money(OldMoney);
	ForceNetUpdate();
	MarkCanonicalSaveDirty(this);
}

void AARGameStateBase::SetScrapFromSave(int32 NewScrap)
{
	if (!HasAuthority())
	{
		return;
	}

	const int32 Clamped = FMath::Max(0, NewScrap);
	if (Scrap == Clamped)
	{
		return;
	}

	const int32 OldScrap = Scrap;
	Scrap = Clamped;
	OnRep_Scrap(OldScrap);
	ForceNetUpdate();
	MarkCanonicalSaveDirty(this);
}

void AARGameStateBase::SetMeatFromSave(const FARMeatState& NewMeat)
{
	if (!HasAuthority())
	{
		return;
	}

	const FARMeatState Sanitized = SanitizeMeatState(NewMeat);
	if (AreMeatStatesEqual(Meat, Sanitized))
	{
		return;
	}

	const FARMeatState OldMeat = Meat;
	Meat = Sanitized;
	OnRep_Meat(OldMeat);
	ForceNetUpdate();
	MarkCanonicalSaveDirty(this);
}

void AARGameStateBase::SetRunLedgerScrap(int32 NewRunLedgerScrap)
{
	if (!HasAuthority())
	{
		return;
	}

	const int32 Clamped = FMath::Max(0, NewRunLedgerScrap);
	if (RunLedgerScrap == Clamped)
	{
		return;
	}

	const int32 OldRunLedgerScrap = RunLedgerScrap;
	RunLedgerScrap = Clamped;
	OnRep_RunLedgerScrap(OldRunLedgerScrap);
	ForceNetUpdate();
}

void AARGameStateBase::SetRunLedgerMeat(const FARMeatState& NewRunLedgerMeat)
{
	if (!HasAuthority())
	{
		return;
	}

	const FARMeatState Sanitized = SanitizeMeatState(NewRunLedgerMeat);
	if (AreMeatStatesEqual(RunLedgerMeat, Sanitized))
	{
		return;
	}

	const FARMeatState OldRunLedgerMeat = RunLedgerMeat;
	RunLedgerMeat = Sanitized;
	OnRep_RunLedgerMeat(OldRunLedgerMeat);
	ForceNetUpdate();
}

void AARGameStateBase::AddRunLedgerScrap(const int32 ScrapDelta)
{
	if (!HasAuthority() || ScrapDelta == 0)
	{
		return;
	}

	SetRunLedgerScrap(RunLedgerScrap + ScrapDelta);
}

void AARGameStateBase::AddRunLedgerMeat(
	const FGameplayTag MeatTag,
	const EARAffinityColor MeatColor,
	const EARVendingQualityTier MeatQualityTier,
	const int32 MeatAmount)
{
	if (!HasAuthority() || !MeatTag.IsValid() || MeatAmount == 0)
	{
		return;
	}

	FARMeatState NewLedger = RunLedgerMeat;
	AddTypedMeatDelta(NewLedger, MeatTag, MeatColor, MeatQualityTier, MeatAmount);
	SetRunLedgerMeat(NewLedger);
}

void AARGameStateBase::ApplyRunLedgerPercentPenalty(const float PenaltyFraction)
{
	if (!HasAuthority())
	{
		return;
	}

	const float ClampedPenalty = FMath::Clamp(PenaltyFraction, 0.0f, 1.0f);
	const float KeepFraction = 1.0f - ClampedPenalty;

	SetRunLedgerScrap(FMath::FloorToInt(RunLedgerScrap * KeepFraction));

	FARMeatState PenalizedMeat = RunLedgerMeat;
	for (FARMeatTypeAmount& Entry : PenalizedMeat.AdditionalAmountsByType)
	{
		Entry.Amount = FMath::FloorToInt(Entry.Amount * KeepFraction);
	}
	PenalizedMeat.NormalizeAdditionalAmounts();
	SetRunLedgerMeat(PenalizedMeat);
}

void AARGameStateBase::ClearRunLedger()
{
	if (!HasAuthority())
	{
		return;
	}

	SetRunLedgerScrap(0);
	SetRunLedgerMeat(FARMeatState());
}

void AARGameStateBase::SetActiveFactionTagFromSave(FGameplayTag NewActiveFactionTag)
{
	if (!HasAuthority())
	{
		return;
	}

	if (ActiveFactionTag == NewActiveFactionTag)
	{
		return;
	}

	const FGameplayTag OldTag = ActiveFactionTag;
	ActiveFactionTag = NewActiveFactionTag;
	OnRep_ActiveFactionTag(OldTag);
	ForceNetUpdate();
	MarkCanonicalSaveDirty(this);
}

void AARGameStateBase::SetActiveFactionEffectTagsFromSave(const FGameplayTagContainer& NewActiveFactionEffectTags)
{
	if (!HasAuthority())
	{
		return;
	}

	if (ActiveFactionEffectTags == NewActiveFactionEffectTags)
	{
		return;
	}

	const FGameplayTagContainer OldTags = ActiveFactionEffectTags;
	ActiveFactionEffectTags = NewActiveFactionEffectTags;
	OnRep_ActiveFactionEffectTags(OldTags);
	ForceNetUpdate();
	MarkCanonicalSaveDirty(this);
}

void AARGameStateBase::SetManualSaveAllowed(const bool bAllowed)
{
	if (!HasAuthority())
	{
		return;
	}

	if (bManualSaveAllowed == bAllowed)
	{
		return;
	}

	const bool bOldAllowed = bManualSaveAllowed;
	bManualSaveAllowed = bAllowed;
	OnRep_ManualSaveAllowed(bOldAllowed);
	ForceNetUpdate();
}

void AARGameStateBase::SetShareLocalPauseAcrossControllers(const bool bShareAcrossControllers)
{
	if (!HasAuthority())
	{
		return;
	}

	if (bShareLocalPauseAcrossControllers == bShareAcrossControllers)
	{
		return;
	}

	const bool bOldValue = bShareLocalPauseAcrossControllers;
	bShareLocalPauseAcrossControllers = bShareAcrossControllers;
	OnRep_ShareLocalPauseAcrossControllers(bOldValue);
	ForceNetUpdate();
}

void AARGameStateBase::NotifyHydratedFromSave()
{
	bHasHydratedFromSave = true;
	OnRep_HydratedFromSave();
}

void AARGameStateBase::OnRep_HydratedFromSave()
{
	OnHydratedFromSave.Broadcast();
}

void AARGameStateBase::OnRep_Cycles(int32 OldCycles)
{
	OnCyclesChanged.Broadcast(Cycles, OldCycles);
}

void AARGameStateBase::OnRep_Unlocks(FGameplayTagContainer OldUnlocks)
{
	OnUnlocksChanged.Broadcast(Unlocks, OldUnlocks);
}

void AARGameStateBase::OnRep_Money(int32 OldMoney)
{
	OnMoneyChanged.Broadcast(Money, OldMoney);
}

void AARGameStateBase::OnRep_Scrap(int32 OldScrap)
{
	UE_LOG(ARLog, Verbose, TEXT("[Save|Currency] Scrap changed old=%d new=%d"), OldScrap, Scrap);
	OnScrapChanged.Broadcast(Scrap, OldScrap);
}

void AARGameStateBase::OnRep_Meat(FARMeatState OldMeat)
{
	UE_LOG(
		ARLog,
		Verbose,
		TEXT("[Save|Currency] Meat changed oldTotal=%d newTotal=%d"),
		OldMeat.GetTotalAmount(),
		Meat.GetTotalAmount());
	OnMeatChanged.Broadcast(Meat, OldMeat);
}

void AARGameStateBase::OnRep_RunLedgerScrap(int32 OldRunLedgerScrap)
{
	UE_LOG(ARLog, Verbose, TEXT("[Save|RunLedger] Scrap changed old=%d new=%d"), OldRunLedgerScrap, RunLedgerScrap);
	OnRunLedgerScrapChanged.Broadcast(RunLedgerScrap, OldRunLedgerScrap);
}

void AARGameStateBase::OnRep_RunLedgerMeat(FARMeatState OldRunLedgerMeat)
{
	UE_LOG(
		ARLog,
		Verbose,
		TEXT("[Save|RunLedger] Meat changed oldTotal=%d newTotal=%d"),
		OldRunLedgerMeat.GetTotalAmount(),
		RunLedgerMeat.GetTotalAmount());
	OnRunLedgerMeatChanged.Broadcast(RunLedgerMeat, OldRunLedgerMeat);
}

void AARGameStateBase::OnRep_ActiveFactionTag(FGameplayTag OldActiveFactionTag)
{
	OnActiveFactionTagChanged.Broadcast(ActiveFactionTag, OldActiveFactionTag);
}

void AARGameStateBase::OnRep_ActiveFactionEffectTags(FGameplayTagContainer OldActiveFactionEffectTags)
{
	OnActiveFactionEffectTagsChanged.Broadcast(ActiveFactionEffectTags, OldActiveFactionEffectTags);
}

void AARGameStateBase::OnRep_ManualSaveAllowed(const bool bOldManualSaveAllowed)
{
	OnManualSaveAllowedChanged.Broadcast(bManualSaveAllowed, bOldManualSaveAllowed);
}

void AARGameStateBase::OnRep_ShareLocalPauseAcrossControllers(const bool bOldShareLocalPauseAcrossControllers)
{
	OnShareLocalPauseAcrossControllersChanged.Broadcast(bShareLocalPauseAcrossControllers, bOldShareLocalPauseAcrossControllers);
}

void AARGameStateBase::OnRep_PauseMenuVoteMask(const uint8 /*OldPauseMenuVoteMask*/)
{
	// Intentionally no-op; aggregate pause delegates are emitted from replicated aggregate fields.
}

void AARGameStateBase::OnRep_ExternalPauseReasonMask(const uint8 /*OldExternalPauseReasonMask*/)
{
	// Intentionally no-op; aggregate pause delegates are emitted from replicated aggregate fields.
}

void AARGameStateBase::OnRep_AllPlayersPausedByMenu(const bool bOldAllPlayersPausedByMenu)
{
	OnAllPlayersPausedByMenuChanged.Broadcast(bAllPlayersPausedByMenu, bOldAllPlayersPausedByMenu);
}

void AARGameStateBase::OnRep_AnyExternalPauseActive(const bool bOldAnyExternalPauseActive)
{
	OnAnyExternalPauseActiveChanged.Broadcast(bAnyExternalPauseActive, bOldAnyExternalPauseActive);
}

void AARGameStateBase::OnRep_EffectivePauseStateActive(const bool bOldEffectivePauseStateActive)
{
	OnEffectivePauseStateChanged.Broadcast(bEffectivePauseStateActive, bOldEffectivePauseStateActive);
}

void AARGameStateBase::RegisterDebugConsoleCommands()
{
	if (!HasAuthority())
	{
		return;
	}

	IConsoleManager& ConsoleManager = IConsoleManager::Get();
	UnregisterDebugConsoleCommands();

	CmdDebugAddMeat = ConsoleManager.RegisterConsoleCommand(
		TEXT("ar.debug.add_meat"),
		TEXT("Usage: ar.debug.add_meat <delta> <Item.Meat.*> <red|blue|white|colorless|none> <low|standard|high|premium>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateUObject(this, &AARGameStateBase::HandleConsoleAddMeat),
		ECVF_Cheat);
}

void AARGameStateBase::UnregisterDebugConsoleCommands()
{
	if (CmdDebugAddMeat)
	{
		IConsoleManager& ConsoleManager = IConsoleManager::Get();
		ConsoleManager.UnregisterConsoleObject(CmdDebugAddMeat, false);
		CmdDebugAddMeat = nullptr;
	}
}

void AARGameStateBase::HandleConsoleAddMeat(const TArray<FString>& Args, UWorld* /*World*/)
{
	if (!HasAuthority())
	{
		return;
	}

	if (Args.Num() < 4)
	{
		UE_LOG(ARLog, Warning, TEXT("[Save|Debug] Usage: ar.debug.add_meat <delta> <Item.Meat.*> <red|blue|white|colorless|none> <low|standard|high|premium>"));
		return;
	}

	int32 Delta = 0;
	if (!LexTryParseString(Delta, *Args[0]) || Delta == 0)
	{
		UE_LOG(ARLog, Warning, TEXT("[Save|Debug] ar.debug.add_meat failed: delta must be a non-zero integer."));
		return;
	}

	const FString MeatTagToken = Args[1];
	const FString ColorToken = Args[2];
	const FString QualityToken = Args[3];

	UGameInstance* GI = GetGameInstance();
	UARItemDefinitionSubsystem* ItemDefinitions = GI ? GI->GetSubsystem<UARItemDefinitionSubsystem>() : nullptr;
	if (!ItemDefinitions)
	{
		UE_LOG(ARLog, Warning, TEXT("[Save|Debug] ar.debug.add_meat failed: missing item definition subsystem."));
		return;
	}

	const FGameplayTag TargetMeatTag = UGameplayTagsManager::Get().RequestGameplayTag(FName(*MeatTagToken), false);
	if (!TargetMeatTag.IsValid())
	{
		UE_LOG(ARLog, Warning, TEXT("[Save|Debug] ar.debug.add_meat failed: '%s' is not a valid gameplay tag."), *MeatTagToken);
		return;
	}

	FARMeatDefinitionRow MeatDefinition;
	if (!ItemDefinitions->ResolveMeatDefinition(TargetMeatTag, MeatDefinition))
	{
		UE_LOG(ARLog, Warning, TEXT("[Save|Debug] ar.debug.add_meat failed: '%s' is not a resolved Item.Meat definition tag."), *MeatTagToken);
		return;
	}

	EARAffinityColor ParsedColor = EARAffinityColor::None;
	if (!TryParseMeatColorToken(ColorToken, ParsedColor))
	{
		UE_LOG(ARLog, Warning, TEXT("[Save|Debug] ar.debug.add_meat failed: invalid color token '%s'."), *ColorToken);
		return;
	}

	EARVendingQualityTier ParsedQuality = EARVendingQualityTier::Standard;
	if (!TryParseMeatQualityToken(QualityToken, ParsedQuality))
	{
		UE_LOG(ARLog, Warning, TEXT("[Save|Debug] ar.debug.add_meat failed: invalid quality token '%s'."), *QualityToken);
		return;
	}

	AddRunLedgerMeat(TargetMeatTag, ParsedColor, ParsedQuality, Delta);

	UE_LOG(
		ARLog,
		Log,
		TEXT("[Save|Debug] AddRunLedgerMeat type='%s' color=%d quality=%d %+d -> total=%d"),
		*TargetMeatTag.ToString(),
		static_cast<int32>(ParsedColor),
		static_cast<int32>(ParsedQuality),
		Delta,
		GetRunLedgerMeat().GetTotalAmount());
}

bool AARGameStateBase::IsPlayerPauseMenuVoteActiveById(const int32 PlayerSlotId) const
{
	const uint8 VoteBit = GetPauseVoteBitForPlayerSlotId(PlayerSlotId);
	return VoteBit != 0 && (PauseMenuVoteMask & VoteBit) != 0;
}

bool AARGameStateBase::IsExternalPauseReasonActive(const EARPauseExternalReason Reason) const
{
	const uint8 ReasonBit = GetExternalPauseReasonBit(Reason);
	return ReasonBit != 0 && (ExternalPauseReasonMask & ReasonBit) != 0;
}

void AARGameStateBase::SetPlayerPauseMenuVoteById(const int32 PlayerSlotId, const bool bPaused)
{
	if (!HasAuthority())
	{
		return;
	}

	const uint8 VoteBit = GetPauseVoteBitForPlayerSlotId(PlayerSlotId);
	if (VoteBit == 0)
	{
		return;
	}

	const uint8 NewMask = bPaused ? (PauseMenuVoteMask | VoteBit) : (PauseMenuVoteMask & ~VoteBit);
	if (NewMask == PauseMenuVoteMask)
	{
		RefreshPauseResolution();
		return;
	}

	const uint8 OldMask = PauseMenuVoteMask;
	PauseMenuVoteMask = NewMask;
	OnRep_PauseMenuVoteMask(OldMask);
	ForceNetUpdate();
	RefreshPauseResolution();
}

void AARGameStateBase::SetExternalPauseReasonActive(const EARPauseExternalReason Reason, const bool bActive)
{
	if (!HasAuthority())
	{
		return;
	}

	const uint8 ReasonBit = GetExternalPauseReasonBit(Reason);
	if (ReasonBit == 0)
	{
		return;
	}

	const uint8 NewMask = bActive ? (ExternalPauseReasonMask | ReasonBit) : (ExternalPauseReasonMask & ~ReasonBit);
	if (NewMask == ExternalPauseReasonMask)
	{
		RefreshPauseResolution();
		return;
	}

	const uint8 OldMask = ExternalPauseReasonMask;
	ExternalPauseReasonMask = NewMask;
	OnRep_ExternalPauseReasonMask(OldMask);
	ForceNetUpdate();
	RefreshPauseResolution();
}

void AARGameStateBase::BindPlayerStateSignals(AARPlayerStateBase* PlayerState)
{
	if (!PlayerState)
	{
		return;
	}

	PlayerState->OnReadyStatusChanged.AddUniqueDynamic(this, &AARGameStateBase::HandlePlayerReadyStatusChanged);
	PlayerState->OnCharacterPickedChanged.AddUniqueDynamic(this, &AARGameStateBase::HandlePlayerCharacterPickedChanged);
}

void AARGameStateBase::UnbindPlayerStateSignals(AARPlayerStateBase* PlayerState)
{
	if (!PlayerState)
	{
		return;
	}

	PlayerState->OnReadyStatusChanged.RemoveDynamic(this, &AARGameStateBase::HandlePlayerReadyStatusChanged);
	PlayerState->OnCharacterPickedChanged.RemoveDynamic(this, &AARGameStateBase::HandlePlayerCharacterPickedChanged);
}

bool AARGameStateBase::ComputeAllPlayersTravelReady() const
{
	bool bFoundAnyPlayer = false;

	for (APlayerState* PlayerState : PlayerArray)
	{
		const AARPlayerStateBase* ARPlayerState = Cast<AARPlayerStateBase>(PlayerState);
		if (!ARPlayerState)
		{
			continue;
		}

		bFoundAnyPlayer = true;
		if (!ARPlayerState->IsTravelReady())
		{
			return false;
		}
	}

	return bFoundAnyPlayer;
}

void AARGameStateBase::RefreshAllPlayersTravelReady()
{
	if (!HasAuthority())
	{
		return;
	}

	const bool bOldAllPlayersTravelReady = bAllPlayersTravelReady;
	const bool bNewAllPlayersTravelReady = ComputeAllPlayersTravelReady();
	if (bOldAllPlayersTravelReady == bNewAllPlayersTravelReady)
	{
		return;
	}

	bAllPlayersTravelReady = bNewAllPlayersTravelReady;
	OnRep_AllPlayersTravelReady(bOldAllPlayersTravelReady);
	ForceNetUpdate();
}

bool AARGameStateBase::ComputeAllPlayersPausedByMenu() const
{
	bool bFoundAnyPlayer = false;

	for (APlayerState* PlayerState : PlayerArray)
	{
		const AARPlayerStateBase* ARPlayerState = Cast<AARPlayerStateBase>(PlayerState);
		if (!ARPlayerState)
		{
			continue;
		}

		const int32 RuntimePlayerSlotId = ARPlayerState->GetPlayerSlotId();
		if (RuntimePlayerSlotId <= 0)
		{
			continue;
		}

		bFoundAnyPlayer = true;
		if (!IsPlayerPauseMenuVoteActiveById(RuntimePlayerSlotId))
		{
			return false;
		}
	}

	return bFoundAnyPlayer;
}

void AARGameStateBase::RefreshPauseResolution()
{
	if (!HasAuthority())
	{
		return;
	}

	bool bShouldForceNetUpdate = false;

	const bool bOldAllPlayersPausedByMenu = bAllPlayersPausedByMenu;
	const bool bNewAllPlayersPausedByMenu = ComputeAllPlayersPausedByMenu();
	if (bOldAllPlayersPausedByMenu != bNewAllPlayersPausedByMenu)
	{
		bAllPlayersPausedByMenu = bNewAllPlayersPausedByMenu;
		OnRep_AllPlayersPausedByMenu(bOldAllPlayersPausedByMenu);
		bShouldForceNetUpdate = true;
	}

	const bool bOldAnyExternalPauseActive = bAnyExternalPauseActive;
	const bool bNewAnyExternalPauseActive = ExternalPauseReasonMask != 0;
	if (bOldAnyExternalPauseActive != bNewAnyExternalPauseActive)
	{
		bAnyExternalPauseActive = bNewAnyExternalPauseActive;
		OnRep_AnyExternalPauseActive(bOldAnyExternalPauseActive);
		bShouldForceNetUpdate = true;
	}

	const bool bOldEffectivePauseStateActive = bEffectivePauseStateActive;
	const bool bNewEffectivePauseStateActive = bAnyExternalPauseActive || bAllPlayersPausedByMenu;
	if (bOldEffectivePauseStateActive != bNewEffectivePauseStateActive)
	{
		bEffectivePauseStateActive = bNewEffectivePauseStateActive;
		OnRep_EffectivePauseStateActive(bOldEffectivePauseStateActive);
		bShouldForceNetUpdate = true;
	}

	if (UWorld* World = GetWorld())
	{
		const bool bCurrentlyPaused = UGameplayStatics::IsGamePaused(World);
		if (bCurrentlyPaused != bEffectivePauseStateActive)
		{
			UGameplayStatics::SetGamePaused(World, bEffectivePauseStateActive);
		}
	}

	if (bShouldForceNetUpdate)
	{
		ForceNetUpdate();
	}
}

void AARGameStateBase::ClearPauseVoteForPlayerSlotId(const int32 PlayerSlotId)
{
	if (!HasAuthority())
	{
		return;
	}

	const uint8 VoteBit = GetPauseVoteBitForPlayerSlotId(PlayerSlotId);
	if (VoteBit == 0 || (PauseMenuVoteMask & VoteBit) == 0)
	{
		return;
	}

	const uint8 OldMask = PauseMenuVoteMask;
	PauseMenuVoteMask &= ~VoteBit;
	OnRep_PauseMenuVoteMask(OldMask);
	ForceNetUpdate();
	RefreshPauseResolution();
}
