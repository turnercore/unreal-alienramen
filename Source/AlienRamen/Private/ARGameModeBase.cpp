#include "ARGameModeBase.h"

#include "ARGameStateBase.h"
#include "ARCharacterStateRuntime.h"
#include "ARCharacterSubsystem.h"
#include "ARLoadoutSettings.h"
#include "ARLog.h"
#include "ARNetworkUserSettings.h"
#include "ARTaggedPlayerStart.h"
#include "ParleyDialogueSubsystem.h"
#include "ParleySpeakerSubsystem.h"
#include "ARPlayerController.h"
#include "ARPlayerStateBase.h"
#include "ARSaveSubsystem.h"
#include "ARSessionSubsystem.h"
#include "ARTravelSubsystem.h"
#include "ARTransitionTypes.h"
#include "EngineUtils.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpectatorPawn.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/PlayerStart.h"

namespace
{
	static bool IsIdentityRelevantPlayerState(const AARPlayerStateBase* PlayerState)
	{
		if (!PlayerState)
		{
			return false;
		}

		const AController* OwnerController = Cast<AController>(PlayerState->GetOwner());
		return OwnerController && OwnerController->PlayerState == PlayerState;
	}

	static bool AreSameLogicalPlayerState(const AARPlayerStateBase* A, const AARPlayerStateBase* B)
	{
		if (!A || !B)
		{
			return false;
		}

		if (A == B)
		{
			return true;
		}

		const int32 APlayerId = A->GetPlayerId();
		const int32 BPlayerId = B->GetPlayerId();
		if (APlayerId > 0 && BPlayerId > 0 && APlayerId == BPlayerId)
		{
			return true;
		}

		const FUniqueNetIdRepl AUniqueId = A->GetUniqueId();
		const FUniqueNetIdRepl BUniqueId = B->GetUniqueId();
		if (AUniqueId.IsValid() && BUniqueId.IsValid() && AUniqueId == BUniqueId)
		{
			// Treat matching online identities as the same logical player when player-id data
			// is unavailable (common in offline/null-subsystem test flows).
			return (APlayerId <= 0 || BPlayerId <= 0);
		}

		return false;
	}

	static bool ShouldRouteViaTransitionMap(const bool bModeDefaultRouteEnabled, const EARTravelRoutePolicy RoutePolicy)
	{
		switch (RoutePolicy)
		{
		case EARTravelRoutePolicy::ForceTransitionMap:
			return true;
		case EARTravelRoutePolicy::ForceDirect:
			return false;
		case EARTravelRoutePolicy::ModeDefault:
		default:
			return bModeDefaultRouteEnabled;
		}
	}

	static FGameplayTag ResolveCharacterTagForSpawnIdentity(const AARPlayerStateBase* PlayerState)
	{
		if (!PlayerState)
		{
			return FGameplayTag();
		}

		return ARPlayer::NormalizeCharacterTag(PlayerState->GetCurrentCharacterTag());
	}

	static void PrepareControllerForGameplaySpawn(AController* Controller)
	{
		if (!Controller)
		{
			return;
		}

		Controller->StartSpot.Reset();

		if (APlayerState* PlayerState = Controller->PlayerState)
		{
			PlayerState->SetIsSpectator(false);
			PlayerState->SetIsOnlyASpectator(false);
		}

		if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
		{
			PlayerController->ChangeState(NAME_Playing);

			if (AARPlayerController* ARPlayerController = Cast<AARPlayerController>(PlayerController))
			{
				ARPlayerController->ClientApplyGameplayInputModeDefaults();
			}
		}
	}

	struct FCharacterRuntimePossessionRepair
	{
		TWeakObjectPtr<AController> Controller = nullptr;
		TWeakObjectPtr<AARPlayerStateBase> PlayerState = nullptr;
		TWeakObjectPtr<AARCharacterStateRuntime> Runtime = nullptr;
		TWeakObjectPtr<APawn> CurrentPawn = nullptr;
		TWeakObjectPtr<APawn> DesiredPawn = nullptr;
		FGameplayTag CharacterTag;
		bool bNeedsPossessionSwap = false;
	};
}

AARGameModeBase::AARGameModeBase()
{
	bUseSeamlessTravel = true;
	DefaultPlayerName = FText::FromString(TEXT("Tenshu"));
	PlayableCharacterSwitchOrder.Add(ARPlayer::GetCharacterTagForChoice(EARCharacterChoice::Brother));
	PlayableCharacterSwitchOrder.Add(ARPlayer::GetCharacterTagForChoice(EARCharacterChoice::Sister));
}

bool AARGameModeBase::SubmitCharacterSwitchHoldRequest(APlayerController* RequestingController, const bool bIsRequesting)
{
	if (!HasAuthority())
	{
		UE_LOG(ARLog, Warning, TEXT("[GameMode] SubmitCharacterSwitchHoldRequest ignored: not authority."));
		return false;
	}

	if (!RequestingController)
	{
		return false;
	}

	CleanupCharacterSwitchRequests();

	const TWeakObjectPtr<APlayerController> RequestingControllerKey(RequestingController);
	if (!bIsRequesting)
	{
		ActiveCharacterSwitchRequests.Remove(RequestingControllerKey);
		CharacterSwitchRequestLatchUntilRelease.Remove(RequestingControllerKey);
		return true;
	}

	if (CharacterSwitchRequestLatchUntilRelease.Contains(RequestingControllerKey))
	{
		return false;
	}

	ActiveCharacterSwitchRequests.Add(RequestingControllerKey);
	return TryResolveQueuedCharacterSwitches();
}

void AARGameModeBase::CleanupCharacterSwitchRequests()
{
	for (auto It = ActiveCharacterSwitchRequests.CreateIterator(); It; ++It)
	{
		const APlayerController* Controller = It->Get();
		if (!Controller || Controller->IsPendingKillPending())
		{
			It.RemoveCurrent();
		}
	}

	for (auto It = CharacterSwitchRequestLatchUntilRelease.CreateIterator(); It; ++It)
	{
		const APlayerController* Controller = It->Get();
		if (!Controller || Controller->IsPendingKillPending())
		{
			It.RemoveCurrent();
		}
	}
}

bool AARGameModeBase::CollectSwitchEligibleControllers(TArray<APlayerController*>& OutEligibleControllers) const
{
	OutEligibleControllers.Reset();

	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PlayerController = It->Get();
		if (!PlayerController || PlayerController->IsPendingKillPending())
		{
			continue;
		}

		const AARPlayerStateBase* PlayerState = PlayerController->GetPlayerState<AARPlayerStateBase>();
		if (!IsIdentityRelevantPlayerState(PlayerState))
		{
			continue;
		}

		const FGameplayTag CharacterTag = ARPlayer::NormalizeCharacterTag(PlayerState->GetCurrentCharacterTag());
		if (!CharacterTag.IsValid())
		{
			continue;
		}

		OutEligibleControllers.Add(PlayerController);
	}

	OutEligibleControllers.Sort(
		[](const APlayerController& Left, const APlayerController& Right)
		{
			const AARPlayerStateBase* LeftState = Left.GetPlayerState<AARPlayerStateBase>();
			const AARPlayerStateBase* RightState = Right.GetPlayerState<AARPlayerStateBase>();
			const int32 LeftSlotId = LeftState ? LeftState->GetPlayerSlotId() : MAX_int32;
			const int32 RightSlotId = RightState ? RightState->GetPlayerSlotId() : MAX_int32;
			if (LeftSlotId != RightSlotId)
			{
				return LeftSlotId < RightSlotId;
			}

			return Left.GetName() < Right.GetName();
		});

	return OutEligibleControllers.Num() > 0;
}

void AARGameModeBase::BuildPlayableCharacterSwitchList(const TArray<APlayerController*>& EligibleControllers, TArray<FGameplayTag>& OutPlayableCharacterTags) const
{
	OutPlayableCharacterTags.Reset();

	auto AddCharacterTagIfMissing = [&OutPlayableCharacterTags](const FGameplayTag& InCharacterTag)
	{
		const FGameplayTag CanonicalTag = ARPlayer::NormalizeCharacterTag(InCharacterTag);
		if (!CanonicalTag.IsValid() || OutPlayableCharacterTags.Contains(CanonicalTag))
		{
			return;
		}

		OutPlayableCharacterTags.Add(CanonicalTag);
	};

	for (const FGameplayTag& ConfiguredCharacterTag : PlayableCharacterSwitchOrder)
	{
		AddCharacterTagIfMissing(ConfiguredCharacterTag);
	}

	for (APlayerController* PlayerController : EligibleControllers)
	{
		const AARPlayerStateBase* PlayerState = PlayerController ? PlayerController->GetPlayerState<AARPlayerStateBase>() : nullptr;
		AddCharacterTagIfMissing(PlayerState ? PlayerState->GetCurrentCharacterTag() : FGameplayTag());
	}
}

bool AARGameModeBase::TryFindNextFreeSwitchTargetTag(
	const FGameplayTag& CurrentCharacterTag,
	const TArray<FGameplayTag>& OrderedCharacterTags,
	const TMap<FGameplayTag, TWeakObjectPtr<APlayerController>>& OccupancyByCharacterTag,
	const APlayerController* RequestingController,
	FGameplayTag& OutTargetCharacterTag) const
{
	OutTargetCharacterTag = FGameplayTag();

	if (!RequestingController || OrderedCharacterTags.Num() < 2)
	{
		return false;
	}

	const FGameplayTag CanonicalCurrentTag = ARPlayer::NormalizeCharacterTag(CurrentCharacterTag);
	if (!CanonicalCurrentTag.IsValid())
	{
		return false;
	}

	const int32 CurrentTagIndex = OrderedCharacterTags.IndexOfByPredicate(
		[&CanonicalCurrentTag](const FGameplayTag& CandidateTag)
		{
			return CandidateTag.MatchesTagExact(CanonicalCurrentTag);
		});
	if (CurrentTagIndex == INDEX_NONE)
	{
		return false;
	}

	const int32 StartIndex = CurrentTagIndex;
	const int32 IterationCount = OrderedCharacterTags.Num() - 1;

	for (int32 Step = 1; Step <= IterationCount; ++Step)
	{
		const int32 CandidateIndex = (StartIndex + Step) % OrderedCharacterTags.Num();
		const FGameplayTag CandidateTag = OrderedCharacterTags[CandidateIndex];
		if (!CandidateTag.IsValid() || CandidateTag.MatchesTagExact(CanonicalCurrentTag))
		{
			continue;
		}

		const TWeakObjectPtr<APlayerController>* OccupyingController = OccupancyByCharacterTag.Find(CandidateTag);
		if (!OccupyingController || !OccupyingController->IsValid() || OccupyingController->Get() == RequestingController)
		{
			OutTargetCharacterTag = CandidateTag;
			return true;
		}
	}

	return false;
}

bool AARGameModeBase::TryResolveQueuedCharacterSwitches()
{
	TArray<APlayerController*> EligibleControllers;
	if (!CollectSwitchEligibleControllers(EligibleControllers))
	{
		return false;
	}

	TArray<FGameplayTag> OrderedCharacterTags;
	BuildPlayableCharacterSwitchList(EligibleControllers, OrderedCharacterTags);
	if (OrderedCharacterTags.Num() < 2)
	{
		return false;
	}

	TSet<TWeakObjectPtr<APlayerController>> EligibleControllerKeys;
	for (APlayerController* EligibleController : EligibleControllers)
	{
		EligibleControllerKeys.Add(TWeakObjectPtr<APlayerController>(EligibleController));
	}

	for (auto It = ActiveCharacterSwitchRequests.CreateIterator(); It; ++It)
	{
		if (!EligibleControllerKeys.Contains(*It))
		{
			It.RemoveCurrent();
		}
	}

	for (auto It = CharacterSwitchRequestLatchUntilRelease.CreateIterator(); It; ++It)
	{
		if (!EligibleControllerKeys.Contains(*It))
		{
			It.RemoveCurrent();
		}
	}

	TMap<FGameplayTag, TWeakObjectPtr<APlayerController>> OccupancyByCharacterTag;
	for (APlayerController* EligibleController : EligibleControllers)
	{
		const AARPlayerStateBase* PlayerState = EligibleController ? EligibleController->GetPlayerState<AARPlayerStateBase>() : nullptr;
		const FGameplayTag CurrentTag = ARPlayer::NormalizeCharacterTag(PlayerState ? PlayerState->GetCurrentCharacterTag() : FGameplayTag());
		if (!CurrentTag.IsValid())
		{
			continue;
		}

		const TWeakObjectPtr<APlayerController> ControllerKey(EligibleController);
		if (const TWeakObjectPtr<APlayerController>* ExistingOccupant = OccupancyByCharacterTag.Find(CurrentTag))
		{
			if (ExistingOccupant->IsValid() && ExistingOccupant->Get() != EligibleController)
			{
				UE_LOG(
					ARLog,
					Warning,
					TEXT("[GameMode] Duplicate occupied character '%s' detected while resolving switch request; preserving first occupant '%s'."),
					*CurrentTag.ToString(),
					*GetNameSafe(ExistingOccupant->Get()));
			}
			continue;
		}

		OccupancyByCharacterTag.Add(CurrentTag, ControllerKey);
	}

	TArray<APlayerController*> ActiveRequestControllers;
	for (APlayerController* EligibleController : EligibleControllers)
	{
		const TWeakObjectPtr<APlayerController> ControllerKey(EligibleController);
		if (ActiveCharacterSwitchRequests.Contains(ControllerKey) && !CharacterSwitchRequestLatchUntilRelease.Contains(ControllerKey))
		{
			ActiveRequestControllers.Add(EligibleController);
		}
	}

	if (ActiveRequestControllers.IsEmpty())
	{
		return false;
	}

	for (APlayerController* RequestingController : ActiveRequestControllers)
	{
		const AARPlayerStateBase* RequestingPlayerState = RequestingController ? RequestingController->GetPlayerState<AARPlayerStateBase>() : nullptr;
		const FGameplayTag CurrentTag = ARPlayer::NormalizeCharacterTag(RequestingPlayerState ? RequestingPlayerState->GetCurrentCharacterTag() : FGameplayTag());
		FGameplayTag NextFreeTargetTag;
		if (!TryFindNextFreeSwitchTargetTag(CurrentTag, OrderedCharacterTags, OccupancyByCharacterTag, RequestingController, NextFreeTargetTag))
		{
			UE_LOG(
				ARLog,
				Verbose,
				TEXT("[GameMode] Switch request by '%s' found no free target (CurrentTag=%s)."),
				*GetNameSafe(RequestingController),
				*CurrentTag.ToString());
			continue;
		}

		UE_LOG(
			ARLog,
			Log,
			TEXT("[GameMode] Switch request by '%s': %s -> %s."),
			*GetNameSafe(RequestingController),
			*CurrentTag.ToString(),
			*NextFreeTargetTag.ToString());

		TMap<TWeakObjectPtr<APlayerController>, FGameplayTag> AssignmentByController;
		AssignmentByController.Add(TWeakObjectPtr<APlayerController>(RequestingController), NextFreeTargetTag);
		if (!ApplyCharacterSwitchAssignments(AssignmentByController))
		{
			return false;
		}

		const TWeakObjectPtr<APlayerController> RequestingControllerKey(RequestingController);
		ActiveCharacterSwitchRequests.Remove(RequestingControllerKey);
		CharacterSwitchRequestLatchUntilRelease.Add(RequestingControllerKey);
		return true;
	}

	// Single-controller flow should never fall into synchronized multi-controller rotation fallback.
	if (EligibleControllers.Num() <= 1)
	{
		return false;
	}

	for (APlayerController* EligibleController : EligibleControllers)
	{
		const TWeakObjectPtr<APlayerController> ControllerKey(EligibleController);
		if (CharacterSwitchRequestLatchUntilRelease.Contains(ControllerKey) || !ActiveCharacterSwitchRequests.Contains(ControllerKey))
		{
			return false;
		}
	}

	TMap<TWeakObjectPtr<APlayerController>, FGameplayTag> AssignmentByController;
	TSet<FGameplayTag> AssignedTargetTags;
	for (APlayerController* EligibleController : EligibleControllers)
	{
		const AARPlayerStateBase* EligiblePlayerState = EligibleController ? EligibleController->GetPlayerState<AARPlayerStateBase>() : nullptr;
		const FGameplayTag CurrentTag = ARPlayer::NormalizeCharacterTag(EligiblePlayerState ? EligiblePlayerState->GetCurrentCharacterTag() : FGameplayTag());
		const int32 CurrentTagIndex = OrderedCharacterTags.IndexOfByPredicate(
			[&CurrentTag](const FGameplayTag& CandidateTag)
			{
				return CandidateTag.MatchesTagExact(CurrentTag);
			});
		if (CurrentTagIndex == INDEX_NONE)
		{
			UE_LOG(
				ARLog,
				Warning,
				TEXT("[GameMode] Character switch synchronization aborted for '%s': current tag '%s' is not in switch order."),
				*GetNameSafe(EligibleController),
				*CurrentTag.ToString());
			return false;
		}

		const FGameplayTag NextTag = OrderedCharacterTags[(CurrentTagIndex + 1) % OrderedCharacterTags.Num()];
		if (!NextTag.IsValid() || NextTag.MatchesTagExact(CurrentTag) || AssignedTargetTags.Contains(NextTag))
		{
			UE_LOG(
				ARLog,
				Warning,
				TEXT("[GameMode] Character switch synchronization aborted: invalid/duplicate next tag '%s' for '%s'."),
				*NextTag.ToString(),
				*GetNameSafe(EligibleController));
			return false;
		}

		AssignedTargetTags.Add(NextTag);
		AssignmentByController.Add(TWeakObjectPtr<APlayerController>(EligibleController), NextTag);
	}

	if (!ApplyCharacterSwitchAssignments(AssignmentByController))
	{
		return false;
	}

	for (APlayerController* EligibleController : EligibleControllers)
	{
		const TWeakObjectPtr<APlayerController> ControllerKey(EligibleController);
		ActiveCharacterSwitchRequests.Remove(ControllerKey);
		CharacterSwitchRequestLatchUntilRelease.Add(ControllerKey);
	}

	return true;
}

bool AARGameModeBase::ApplyCharacterSwitchAssignments(const TMap<TWeakObjectPtr<APlayerController>, FGameplayTag>& AssignmentByController)
{
	if (AssignmentByController.Num() <= 0 || !HasAuthority())
	{
		return false;
	}

	struct FResolvedCharacterSwitchAssignment
	{
		TWeakObjectPtr<APlayerController> Controller;
		TWeakObjectPtr<AARPlayerStateBase> PlayerState;
		TWeakObjectPtr<AARCharacterStateRuntime> PreviousRuntime;
		TWeakObjectPtr<AARCharacterStateRuntime> TargetRuntime;
		TWeakObjectPtr<APawn> PreviousPawn;
		TWeakObjectPtr<APawn> TargetPawnBeforeRetag;
		FGameplayTag TargetTag;
		FTransform RespawnTransform = FTransform::Identity;
		bool bHasRespawnTransform = false;
	};

	UARCharacterSubsystem* CharacterSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UARCharacterSubsystem>() : nullptr;
	TArray<FResolvedCharacterSwitchAssignment> ResolvedAssignments;
	ResolvedAssignments.Reserve(AssignmentByController.Num());
	for (const TPair<TWeakObjectPtr<APlayerController>, FGameplayTag>& Entry : AssignmentByController)
	{
		APlayerController* PlayerController = Entry.Key.Get();
		AARPlayerStateBase* PlayerState = PlayerController ? PlayerController->GetPlayerState<AARPlayerStateBase>() : nullptr;
		const FGameplayTag TargetTag = ARPlayer::NormalizeCharacterTag(Entry.Value);
		if (!PlayerController || !PlayerState || !TargetTag.IsValid())
		{
			return false;
		}

		const FGameplayTag CurrentTag = ARPlayer::NormalizeCharacterTag(PlayerState->GetCurrentCharacterTag());
		if (CurrentTag.IsValid() && CurrentTag.MatchesTagExact(TargetTag))
		{
			// Never "switch" to the currently active character.
			UE_LOG(
				ARLog,
				Verbose,
				TEXT("[GameMode] Ignoring no-op switch assignment for '%s': target tag '%s' already active."),
				*GetNameSafe(PlayerController),
				*TargetTag.ToString());
			continue;
		}

		FResolvedCharacterSwitchAssignment& Assignment = ResolvedAssignments.AddDefaulted_GetRef();
		Assignment.Controller = PlayerController;
		Assignment.PlayerState = PlayerState;
		Assignment.TargetTag = TargetTag;
		Assignment.PreviousRuntime = PlayerState->GetCurrentCharacterRuntime();
		Assignment.PreviousPawn = PlayerController->GetPawn();

		if (CharacterSubsystem)
		{
			Assignment.TargetRuntime = CharacterSubsystem->EnsureCharacterRuntime(PlayerState, TargetTag);
			if (AARCharacterStateRuntime* TargetRuntime = Assignment.TargetRuntime.Get())
			{
				Assignment.TargetPawnBeforeRetag = TargetRuntime->GetCurrentPawn();
			}
		}

		if (const APawn* ExistingPawn = Assignment.PreviousPawn.Get())
		{
			Assignment.RespawnTransform = ExistingPawn->GetActorTransform();
			Assignment.bHasRespawnTransform = true;
		}
	}

	if (ResolvedAssignments.Num() <= 0)
	{
		return false;
	}

	TSet<TWeakObjectPtr<APlayerController>> AssignmentControllers;
	AssignmentControllers.Reserve(ResolvedAssignments.Num());
	for (const FResolvedCharacterSwitchAssignment& Assignment : ResolvedAssignments)
	{
		AssignmentControllers.Add(Assignment.Controller);
	}

	for (const FResolvedCharacterSwitchAssignment& Assignment : ResolvedAssignments)
	{
		APlayerController* PlayerController = Assignment.Controller.Get();
		APawn* TargetPawnBeforeRetag = Assignment.TargetPawnBeforeRetag.Get();
		if (!PlayerController || !TargetPawnBeforeRetag)
		{
			UE_LOG(
				ARLog,
				Error,
				TEXT("[GameMode] Rejecting switch assignment for '%s': target character '%s' has no existing pawn to possess."),
				*GetNameSafe(PlayerController),
				*Assignment.TargetTag.ToString());
			return false;
		}

		if (TargetPawnBeforeRetag == Assignment.PreviousPawn.Get())
		{
			UE_LOG(
				ARLog,
				Error,
				TEXT("[GameMode] Rejecting switch assignment for '%s': target character '%s' resolved to the currently possessed pawn '%s'."),
				*GetNameSafe(PlayerController),
				*Assignment.TargetTag.ToString(),
				*GetNameSafe(TargetPawnBeforeRetag));
			return false;
		}

		AController* TargetController = TargetPawnBeforeRetag->GetController();
		if (TargetController && !AssignmentControllers.Contains(TWeakObjectPtr<APlayerController>(Cast<APlayerController>(TargetController))))
		{
			UE_LOG(
				ARLog,
				Error,
				TEXT("[GameMode] Rejecting switch assignment for '%s': target pawn '%s' for '%s' is occupied by non-participant '%s'."),
				*GetNameSafe(PlayerController),
				*GetNameSafe(TargetPawnBeforeRetag),
				*Assignment.TargetTag.ToString(),
				*GetNameSafe(TargetController));
			return false;
		}
	}

	for (const FResolvedCharacterSwitchAssignment& Assignment : ResolvedAssignments)
	{
		AARPlayerStateBase* PlayerState = Assignment.PlayerState.Get();
		if (!PlayerState)
		{
			return false;
		}

		PlayerState->SetCurrentCharacterTagDirect(Assignment.TargetTag);
	}

	const FGameplayTag TransitionModeTag = FGameplayTag::RequestGameplayTag(TEXT("Mode.Transition"), false);
	const bool bIsTransitionMode = TransitionModeTag.IsValid() && ModeTag.MatchesTagExact(TransitionModeTag);

	for (const FResolvedCharacterSwitchAssignment& Assignment : ResolvedAssignments)
	{
		if (APlayerController* PlayerController = Assignment.Controller.Get())
		{
			if (APawn* CurrentPawn = PlayerController->GetPawn())
			{
				CurrentPawn->ConsumeMovementInputVector();
				if (ACharacter* CharacterPawn = Cast<ACharacter>(CurrentPawn))
				{
					if (UCharacterMovementComponent* MovementComponent = CharacterPawn->GetCharacterMovement())
					{
						MovementComponent->StopMovementImmediately();
					}
				}

				PlayerController->UnPossess();
			}
		}
	}

	for (const FResolvedCharacterSwitchAssignment& Assignment : ResolvedAssignments)
	{
		APlayerController* PlayerController = Assignment.Controller.Get();
		AARPlayerStateBase* PlayerState = Assignment.PlayerState.Get();
		if (!PlayerController)
		{
			return false;
		}

		if (CharacterSubsystem)
		{
			AARCharacterStateRuntime* PreviousRuntime = Assignment.PreviousRuntime.Get();
			AARCharacterStateRuntime* TargetRuntime = Assignment.TargetRuntime.Get();
			APawn* PreviousPawn = Assignment.PreviousPawn.Get();
			APawn* TargetPawnBeforeRetag = Assignment.TargetPawnBeforeRetag.Get();

			if (PreviousRuntime && PreviousRuntime != TargetRuntime && PreviousPawn)
			{
				CharacterSubsystem->BindRuntimePawn(PreviousRuntime, PreviousPawn);
			}

			if (TargetRuntime && TargetRuntime != PreviousRuntime)
			{
				if (TargetPawnBeforeRetag == PreviousPawn)
				{
					TargetPawnBeforeRetag = nullptr;
				}

				CharacterSubsystem->BindRuntimePawn(TargetRuntime, TargetPawnBeforeRetag);
			}
		}

		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[GameMode] Applying switch assignment for '%s': target=%s previousPawn=%s targetPawnBeforeRetag=%s."),
			*GetNameSafe(PlayerController),
			*Assignment.TargetTag.ToString(),
			*GetNameSafe(Assignment.PreviousPawn.Get()),
			*GetNameSafe(Assignment.TargetPawnBeforeRetag.Get()));

		APawn* TargetPawn = Assignment.TargetPawnBeforeRetag.Get();
		if (TargetPawn && TargetPawn->GetController() && TargetPawn->GetController() != PlayerController)
		{
			UE_LOG(
				ARLog,
				Error,
				TEXT("[GameMode] Switch assignment failed for '%s': target pawn '%s' is still controlled by '%s' after unpossess pass."),
				*GetNameSafe(TargetPawn),
				*GetNameSafe(PlayerController),
				*GetNameSafe(TargetPawn->GetController()));
			return false;
		}

		if (!TargetPawn)
		{
			UE_LOG(
				ARLog,
				Error,
				TEXT("[GameMode] Switch assignment failed for '%s': target pawn for '%s' disappeared before possession."),
				*GetNameSafe(PlayerController),
				*Assignment.TargetTag.ToString());
			return false;
		}

		if (PlayerState && Assignment.TargetRuntime.IsValid())
		{
			PlayerState->SetCurrentCharacterRuntime(Assignment.TargetRuntime.Get());
			CharacterSubsystem->BindRuntimePawn(Assignment.TargetRuntime.Get(), TargetPawn);
		}

		if (!bIsTransitionMode)
		{
			PrepareControllerForGameplaySpawn(PlayerController);
		}

		PlayerController->Possess(TargetPawn);
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UParleySpeakerSubsystem* SpeakerSubsystem = GI->GetSubsystem<UParleySpeakerSubsystem>())
		{
			SpeakerSubsystem->RefreshAllSpeakerTalkableStates();
		}
	}

	return true;
}

FString AARGameModeBase::BuildModeTravelURL(const FString& DestinationURL, const EARTravelRoutePolicy RoutePolicy) const
{
	if (!ShouldRouteViaTransitionMap(bRouteModeTravelThroughTransitionMap, RoutePolicy))
	{
		return DestinationURL;
	}

	if (DestinationURL.IsEmpty() || TransitionTravelMapURL.IsEmpty())
	{
		if (TransitionTravelMapURL.IsEmpty() && RoutePolicy == EARTravelRoutePolicy::ForceTransitionMap)
		{
			UE_LOG(ARLog, Warning, TEXT("[GameMode] BuildModeTravelURL: ForceTransitionMap requested but TransitionTravelMapURL is empty on '%s'; falling back to direct travel."), *GetNameSafe(this));
		}
		return DestinationURL;
	}

	const FString TransitionOptionTokenA = FString::Printf(TEXT("?%s="), ARTransition::OptionDestinationURL);
	const FString TransitionOptionTokenB = FString::Printf(TEXT("&%s="), ARTransition::OptionDestinationURL);
	if (DestinationURL.Contains(TransitionOptionTokenA, ESearchCase::IgnoreCase) || DestinationURL.Contains(TransitionOptionTokenB, ESearchCase::IgnoreCase))
	{
		return DestinationURL;
	}

	auto ExtractMapPath = [](const FString& URL) -> FString
	{
		int32 QueryIndex = INDEX_NONE;
		if (URL.FindChar(TEXT('?'), QueryIndex))
		{
			return URL.Left(QueryIndex);
		}

		return URL;
	};

	const FString DestinationMapPath = ExtractMapPath(DestinationURL);
	const FString TransitionMapPath = ExtractMapPath(TransitionTravelMapURL);
	if (DestinationMapPath.Equals(TransitionMapPath, ESearchCase::IgnoreCase))
	{
		return DestinationURL;
	}

	FARTransitionContext TransitionContext;
	TransitionContext.SourceMode = TransitionSourceMode;
	TransitionContext.Reason = TransitionReason;
	TransitionContext.DestinationURL = DestinationURL;
	const FString WrappedURL = ARTransition::BuildTransitionTravelURL(TransitionTravelMapURL, TransitionContext);
	return WrappedURL.IsEmpty() ? DestinationURL : WrappedURL;
}

bool AARGameModeBase::EndModeAndTravel(const FString& URL, const FString& Options, const bool bSkipReadyChecks, const bool bAbsolute, const bool bSkipGameNotify, const bool bUseOpenLevelInPIE)
{
	return TryStartTravel(URL, Options, bSkipReadyChecks, bAbsolute, bSkipGameNotify, bUseOpenLevelInPIE, EARTravelRoutePolicy::ForceTransitionMap);
}

bool AARGameModeBase::TravelDirectInMode(const FString& URL, const FString& Options, const bool bSkipReadyChecks, const bool bAbsolute, const bool bSkipGameNotify, const bool bUseOpenLevelInPIE)
{
	return TryStartTravel(URL, Options, bSkipReadyChecks, bAbsolute, bSkipGameNotify, bUseOpenLevelInPIE, EARTravelRoutePolicy::ForceDirect);
}

bool AARGameModeBase::StartParleyConversationByTagForCharacters(
	const FGameplayTag RequesterCharacterTag,
	const FGameplayTag OwnerCharacterTag,
	const FGameplayTag ConversationTag)
{
	if (!HasAuthority())
	{
		UE_LOG(
			ARLog,
			Warning,
			TEXT("[GameMode] StartParleyConversationByTagForCharacters ignored: not authority (Requester=%s Owner=%s Conversation=%s)."),
			*RequesterCharacterTag.ToString(),
			*OwnerCharacterTag.ToString(),
			*ConversationTag.ToString());
		return false;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UParleyDialogueSubsystem* DialogueSubsystem = GameInstance ? GameInstance->GetSubsystem<UParleyDialogueSubsystem>() : nullptr;
	if (!DialogueSubsystem)
	{
		UE_LOG(
			ARLog,
			Warning,
			TEXT("[GameMode] StartParleyConversationByTagForCharacters failed: ParleyDialogueSubsystem unavailable (Requester=%s Owner=%s Conversation=%s)."),
			*RequesterCharacterTag.ToString(),
			*OwnerCharacterTag.ToString(),
			*ConversationTag.ToString());
		return false;
	}

	const bool bStarted = DialogueSubsystem->StartConversationByTagForCharacters(
		RequesterCharacterTag,
		OwnerCharacterTag,
		ConversationTag);
	if (!bStarted)
	{
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[GameMode] StartParleyConversationByTagForCharacters returned false (Requester=%s Owner=%s Conversation=%s)."),
			*RequesterCharacterTag.ToString(),
			*OwnerCharacterTag.ToString(),
			*ConversationTag.ToString());
	}

	return bStarted;
}

TSubclassOf<AGameSession> AARGameModeBase::GetGameSessionClass() const
{
	// Keep runtime independent from BP-configured AdvancedGameSession classes.
	return AGameSession::StaticClass();
}

void AARGameModeBase::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);

	if (!ErrorMessage.IsEmpty())
	{
		return;
	}

	if (const UARNetworkUserSettings* NetworkSettings = GetDefault<UARNetworkUserSettings>())
	{
		if (NetworkSettings->bStayOffline && UniqueId.IsValid())
		{
			const FString NetIdType = UniqueId->GetType().ToString();
			const bool bIsLocalStyleId = NetIdType.Equals(TEXT("NULL"), ESearchCase::IgnoreCase)
				|| NetIdType.Equals(TEXT("INVALID"), ESearchCase::IgnoreCase)
				|| NetIdType.Equals(TEXT("UNSET"), ESearchCase::IgnoreCase);

			if (!bIsLocalStyleId)
			{
				ErrorMessage = TEXT("Server has StayOffline enabled.");
				UE_LOG(ARLog, Warning, TEXT("[GameMode] PreLogin denied connection from '%s': StayOffline is enabled (NetIdType=%s)."), *Address, *NetIdType);
				return;
			}
		}
	}

	const AARGameStateBase* GS = GetGameState<AARGameStateBase>();
	if (!GS)
	{
		return;
	}

	int32 PlayerCount = 0;
	for (APlayerState* PS : GS->PlayerArray)
	{
		if (Cast<AARPlayerStateBase>(PS))
		{
			++PlayerCount;
		}
	}

	if (PlayerCount >= 2)
	{
		ErrorMessage = TEXT("Server full.");
		UE_LOG(ARLog, Warning, TEXT("[GameMode] PreLogin denied connection from '%s': player cap reached (%d)."), *Address, PlayerCount);
		return;
	}
}

void AARGameModeBase::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	if (AARGameStateBase* GS = GetGameState<AARGameStateBase>())
	{
		NormalizeConnectedPlayersIdentity(GS);
		GS->SetManualSaveAllowed(bAllowManualSaveInMode);
		GS->SetShareLocalPauseAcrossControllers(bShareLocalPauseAcrossControllersInMode);
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UARSaveSubsystem* SaveSubsystem = GI->GetSubsystem<UARSaveSubsystem>())
		{
			const FGameplayTag TransitionModeTag = FGameplayTag::RequestGameplayTag(TEXT("Mode.Transition"), false);
			const FGameplayTag ShopModeTag = FGameplayTag::RequestGameplayTag(TEXT("Mode.Shop"), false);
			if (SaveSubsystem->HasPendingFreshLoadEntry()
				&& ModeTag != TransitionModeTag
				&& ModeTag != ShopModeTag)
			{
				SaveSubsystem->ClearPendingFreshLoadEntry();
			}
		}

		if (UParleySpeakerSubsystem* SpeakerSubsystem = GI->GetSubsystem<UParleySpeakerSubsystem>())
		{
			SpeakerSubsystem->RefreshAllSpeakerTalkableStates();
		}

		if (UARSessionSubsystem* SessionSubsystem = GI->GetSubsystem<UARSessionSubsystem>())
		{
			FARSessionResult SessionResult;
			if (!SessionSubsystem->RefreshJoinability(SessionResult) && SessionResult.ResultCode != EARSessionResultCode::SessionNotFound)
			{
				UE_LOG(ARLog, Verbose, TEXT("[GameMode] Session joinability refresh on mode begin failed: %s"), *SessionResult.Error);
			}
		}
	}
}

FGameplayTag AARGameModeBase::ResolveCharacterRuntimeTagForController(const AController* Controller) const
{
	const AARPlayerStateBase* PlayerState = Controller ? Controller->GetPlayerState<AARPlayerStateBase>() : nullptr;
	if (!PlayerState)
	{
		return FGameplayTag();
	}

	const FGameplayTag CanonicalCharacterTag = ARPlayer::NormalizeCharacterTag(PlayerState->GetCurrentCharacterTag());
	const FGameplayTag ChoiceCharacterTag = ARPlayer::GetCharacterTagForChoice(PlayerState->GetCharacterPicked());
	if (ChoiceCharacterTag.IsValid()
		&& (!CanonicalCharacterTag.IsValid() || !CanonicalCharacterTag.MatchesTagExact(ChoiceCharacterTag)))
	{
		return ChoiceCharacterTag;
	}

	if (CanonicalCharacterTag.IsValid())
	{
		return CanonicalCharacterTag;
	}

	return ARPlayer::GetCharacterTagForChoice(PlayerState->GetCharacterPicked());
}

bool AARGameModeBase::ShouldRunCharacterRuntimeBootstrap() const
{
	return false;
}

void AARGameModeBase::HydrateCharacterRuntimeData(const FARCharacterRuntimeBootstrapContext& Context)
{
	(void)Context;
}

bool AARGameModeBase::ResolveCharacterRuntimePawnClass(
	const FARCharacterRuntimeBootstrapContext& Context,
	const FGameplayTag CharacterTag,
	const AARPlayerStateBase* OwnerPlayerState,
	TSubclassOf<APawn>& OutPawnClass) const
{
	(void)Context;
	(void)CharacterTag;
	(void)OwnerPlayerState;
	OutPawnClass = nullptr;
	return false;
}

bool AARGameModeBase::ResolveCharacterRuntimeSpawnTransform(
	const FARCharacterRuntimeBootstrapContext& Context,
	const FGameplayTag CharacterTag,
	const AARPlayerStateBase* OwnerPlayerState,
	FTransform& OutTransform) const
{
	(void)Context;
	OutTransform = FTransform::Identity;

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	AARTaggedPlayerStart* BestTaggedStart = nullptr;
	for (TActorIterator<AARTaggedPlayerStart> It(World); It; ++It)
	{
		AARTaggedPlayerStart* Candidate = *It;
		if (!IsValid(Candidate))
		{
			continue;
		}

		if (Candidate->MatchesSpawnIdentityTag(CharacterTag, true))
		{
			BestTaggedStart = Candidate;
			break;
		}

		if (!BestTaggedStart && Candidate->MatchesSpawnIdentityTag(CharacterTag, false))
		{
			BestTaggedStart = Candidate;
		}
	}

	if (BestTaggedStart)
	{
		OutTransform = BestTaggedStart->GetActorTransform();
		return true;
	}

	if (TActorIterator<APlayerStart> StartIt(World); StartIt)
	{
		if (APlayerStart* DefaultStart = *StartIt)
		{
			OutTransform = DefaultStart->GetActorTransform();
			return true;
		}
	}

	if (const APawn* OwnerPawn = OwnerPlayerState ? OwnerPlayerState->GetPawn() : nullptr)
	{
		OutTransform = OwnerPawn->GetActorTransform();
		FVector Location = OutTransform.GetLocation();
		Location += OutTransform.GetUnitAxis(EAxis::Y) * 150.0f;
		OutTransform.SetLocation(Location);
		return true;
	}

	return false;
}

void AARGameModeBase::PostCharacterRuntimePawnBound(
	const FARCharacterRuntimeBootstrapContext& Context,
	AARCharacterStateRuntime* Runtime,
	APawn* Pawn,
	const FGameplayTag CharacterTag,
	const bool bIsControlledCharacter) const
{
	(void)Context;
	(void)Runtime;
	(void)Pawn;
	(void)CharacterTag;
	(void)bIsControlledCharacter;
}

void AARGameModeBase::PostCharacterRuntimeBootstrap(const FARCharacterRuntimeBootstrapContext& Context)
{
	(void)Context;
}

void AARGameModeBase::RunCharacterRuntimeBootstrapSequence(const EARCharacterRuntimeBootstrapReason Reason, AController* FocusController)
{
	if (!HasAuthority() || !ShouldRunCharacterRuntimeBootstrap())
	{
		return;
	}

	UWorld* World = GetWorld();
	UARCharacterSubsystem* CharacterSubsystem = World ? World->GetSubsystem<UARCharacterSubsystem>() : nullptr;
	AARGameStateBase* SharedGameState = GetGameState<AARGameStateBase>();
	if (!World || !CharacterSubsystem)
	{
		return;
	}

	FARCharacterRuntimeBootstrapContext Context;
	Context.Reason = Reason;
	Context.FocusController = FocusController;

	const auto ResolveOwnerPlayerStateForTag = [SharedGameState](const FGameplayTag& CharacterTag) -> AARPlayerStateBase*
	{
		if (!SharedGameState)
		{
			return nullptr;
		}

		if (AARPlayerStateBase* TaggedPlayerState = SharedGameState->GetPlayerStateByCharacterTag(CharacterTag))
		{
			const AController* OwningController = Cast<AController>(TaggedPlayerState->GetOwner());
			if (OwningController && OwningController->PlayerState == TaggedPlayerState)
			{
				return TaggedPlayerState;
			}
		}

		for (AARPlayerStateBase* PlayerState : SharedGameState->GetPlayerStates())
		{
			const AController* OwningController = PlayerState ? Cast<AController>(PlayerState->GetOwner()) : nullptr;
			if (OwningController && OwningController->PlayerState == PlayerState)
			{
				return PlayerState;
			}
		}

		return nullptr;
	};

	const auto GatherCandidateCharacterTags = [this, &Context](TArray<FGameplayTag>& OutCharacterTags)
	{
		OutCharacterTags.Reset();

		for (const FGameplayTag& OrderedTag : PlayableCharacterSwitchOrder)
		{
			const FGameplayTag CanonicalTag = ARPlayer::NormalizeCharacterTag(OrderedTag);
			if (CanonicalTag.IsValid())
			{
				OutCharacterTags.AddUnique(CanonicalTag);
			}
		}

		const FGameplayTag BrotherTag = ARPlayer::GetCharacterTagForChoice(EARCharacterChoice::Brother);
		const FGameplayTag SisterTag = ARPlayer::GetCharacterTagForChoice(EARCharacterChoice::Sister);
		if (BrotherTag.IsValid())
		{
			OutCharacterTags.AddUnique(BrotherTag);
		}
		if (SisterTag.IsValid())
		{
			OutCharacterTags.AddUnique(SisterTag);
		}

		if (AController* LocalFocusController = Context.FocusController.Get())
		{
			const FGameplayTag FocusCharacterTag = ARPlayer::NormalizeCharacterTag(ResolveCharacterRuntimeTagForController(LocalFocusController));
			if (FocusCharacterTag.IsValid())
			{
				OutCharacterTags.AddUnique(FocusCharacterTag);
			}
		}
	};

	HydrateCharacterRuntimeData(Context);

	TArray<FGameplayTag> CandidateCharacterTags;
	GatherCandidateCharacterTags(CandidateCharacterTags);
	for (const FGameplayTag& CharacterTag : CandidateCharacterTags)
	{
		if (!CharacterTag.IsValid())
		{
			continue;
		}

		if (SharedGameState)
		{
			if (AARPlayerStateBase* TaggedPlayerState = SharedGameState->GetPlayerStateByCharacterTag(CharacterTag))
			{
				const AController* OwningController = Cast<AController>(TaggedPlayerState->GetOwner());
				if (OwningController && OwningController->PlayerState == TaggedPlayerState)
				{
					continue;
				}
			}
		}

		AARPlayerStateBase* OwnerPlayerState = ResolveOwnerPlayerStateForTag(CharacterTag);
		if (!OwnerPlayerState)
		{
			continue;
		}

		bool bCreatedRuntime = false;
		AARCharacterStateRuntime* Runtime = CharacterSubsystem->EnsureCharacterRuntime(OwnerPlayerState, CharacterTag, bCreatedRuntime);
		(void)bCreatedRuntime;
		if (!Runtime)
		{
			continue;
		}

		APawn* PawnToBind = Runtime->GetCurrentPawn();
		if (PawnToBind && PawnToBind->GetController())
		{
			continue;
		}

		TSubclassOf<APawn> PawnClass;
		if (!ResolveCharacterRuntimePawnClass(Context, CharacterTag, OwnerPlayerState, PawnClass) || !PawnClass)
		{
			continue;
		}

		FTransform SpawnTransform = FTransform::Identity;
		if (!ResolveCharacterRuntimeSpawnTransform(Context, CharacterTag, OwnerPlayerState, SpawnTransform))
		{
			continue;
		}

		if (!PawnToBind)
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			SpawnParams.ObjectFlags |= RF_Transient;
			PawnToBind = World->SpawnActor<APawn>(PawnClass, SpawnTransform, SpawnParams);
			if (!PawnToBind)
			{
				continue;
			}
		}
		else
		{
			PawnToBind->SetActorTransform(SpawnTransform, false, nullptr, ETeleportType::TeleportPhysics);
		}

		CharacterSubsystem->BindRuntimePawn(Runtime, PawnToBind);
		PostCharacterRuntimePawnBound(Context, Runtime, PawnToBind, CharacterTag, false);
	}

	TArray<FCharacterRuntimePossessionRepair> PendingRepairs;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		AController* Controller = It->Get();
		if (!Controller || (Context.FocusController.IsValid() && Controller != Context.FocusController.Get()))
		{
			continue;
		}

		AARPlayerStateBase* PlayerState = Controller->GetPlayerState<AARPlayerStateBase>();
		const FGameplayTag CharacterTag = ARPlayer::NormalizeCharacterTag(ResolveCharacterRuntimeTagForController(Controller));
		if (!PlayerState || !CharacterTag.IsValid())
		{
			continue;
		}

		bool bCreatedRuntime = false;
		AARCharacterStateRuntime* Runtime = CharacterSubsystem->EnsureCharacterRuntime(PlayerState, CharacterTag, bCreatedRuntime);
		(void)bCreatedRuntime;
		if (!Runtime)
		{
			continue;
		}

		PlayerState->SetCurrentCharacterRuntime(Runtime);

		APawn* CurrentPawn = Controller->GetPawn();
		APawn* DesiredPawn = Runtime->GetCurrentPawn();
		if (!DesiredPawn)
		{
			DesiredPawn = CurrentPawn;
		}

		if (!DesiredPawn)
		{
			continue;
		}

		if (DesiredPawn->GetController() && DesiredPawn->GetController() != Controller)
		{
			continue;
		}

		FTransform DesiredTransform = FTransform::Identity;
		if (ResolveCharacterRuntimeSpawnTransform(Context, CharacterTag, PlayerState, DesiredTransform))
		{
			DesiredPawn->SetActorTransform(DesiredTransform, false, nullptr, ETeleportType::TeleportPhysics);
		}

		CharacterSubsystem->BindRuntimePawn(Runtime, DesiredPawn);
		PostCharacterRuntimePawnBound(Context, Runtime, DesiredPawn, CharacterTag, true);

		FCharacterRuntimePossessionRepair& PendingRepair = PendingRepairs.AddDefaulted_GetRef();
		PendingRepair.Controller = Controller;
		PendingRepair.PlayerState = PlayerState;
		PendingRepair.Runtime = Runtime;
		PendingRepair.CurrentPawn = CurrentPawn;
		PendingRepair.DesiredPawn = DesiredPawn;
		PendingRepair.CharacterTag = CharacterTag;
		PendingRepair.bNeedsPossessionSwap = CurrentPawn != DesiredPawn;
	}

	for (const FCharacterRuntimePossessionRepair& PendingRepair : PendingRepairs)
	{
		AController* Controller = PendingRepair.Controller.Get();
		APawn* DesiredPawn = PendingRepair.DesiredPawn.Get();
		APawn* CurrentPawn = PendingRepair.CurrentPawn.Get();
		if (!Controller || !DesiredPawn)
		{
			continue;
		}

		if (PendingRepair.bNeedsPossessionSwap)
		{
			if (CurrentPawn)
			{
				Controller->UnPossess();
			}

			Controller->Possess(DesiredPawn);

			if (CurrentPawn && CurrentPawn != DesiredPawn && !CurrentPawn->GetController())
			{
				CurrentPawn->Destroy();
			}
		}

		if (AARPlayerStateBase* PlayerState = PendingRepair.PlayerState.Get())
		{
			PlayerState->SetCurrentCharacterRuntime(PendingRepair.Runtime.Get());
		}
	}

	PostCharacterRuntimeBootstrap(Context);
}

int32 AARGameModeBase::FindFirstFreePlayerSlotId(const AARGameStateBase* GameState, const AARPlayerStateBase* IgnorePlayerState)
{
	if (!GameState)
	{
		return 1;
	}

	TSet<int32> OccupiedSlotIds;
	for (APlayerState* PS : GameState->PlayerArray)
	{
		const AARPlayerStateBase* Player = Cast<AARPlayerStateBase>(PS);
		if (!IsIdentityRelevantPlayerState(Player)
			|| Player == IgnorePlayerState
			|| (IgnorePlayerState && AreSameLogicalPlayerState(Player, IgnorePlayerState)))
		{
			continue;
		}

		const int32 RuntimeSlotId = Player->GetPlayerSlotId();
		if (RuntimeSlotId > 0)
		{
			OccupiedSlotIds.Add(RuntimeSlotId);
		}
	}

	int32 CandidateSlotId = 1;
	while (OccupiedSlotIds.Contains(CandidateSlotId))
	{
		++CandidateSlotId;
	}

	return CandidateSlotId;
}

EARAffinityColor AARGameModeBase::ResolveExpectedInvaderPlayerColor(const EARCharacterChoice CharacterChoice)
{
	switch (CharacterChoice)
	{
	case EARCharacterChoice::Brother:
		return EARAffinityColor::Blue;
	case EARCharacterChoice::Sister:
		return EARAffinityColor::Red;
	default:
		// Mode-load baseline should still be a valid team color even if character is temporarily unset.
		return EARAffinityColor::Blue;
	}
}

EARCharacterChoice AARGameModeBase::GetAlternateCharacterChoice(const EARCharacterChoice CurrentChoice)
{
	switch (CurrentChoice)
	{
	case EARCharacterChoice::Brother:
		return EARCharacterChoice::Sister;
	case EARCharacterChoice::Sister:
		return EARCharacterChoice::Brother;
	default:
		return EARCharacterChoice::None;
	}
}

bool AARGameModeBase::IsCharacterChoiceTakenByOther(const AARGameStateBase* InGameState, const AARPlayerStateBase* CurrentPlayerState, const EARCharacterChoice CharacterChoice)
{
	if (!InGameState || !CurrentPlayerState || CharacterChoice == EARCharacterChoice::None)
	{
		return false;
	}

	for (APlayerState* PS : InGameState->PlayerArray)
	{
		AARPlayerStateBase* Player = Cast<AARPlayerStateBase>(PS);
		if (!IsIdentityRelevantPlayerState(Player) || Player == CurrentPlayerState)
		{
			continue;
		}

		if (Player->GetCharacterPicked() == CharacterChoice)
		{
			return true;
		}
	}

	return false;
}

void AARGameModeBase::ResolveCharacterChoiceConflict(const AARGameStateBase* InGameState, AARPlayerStateBase* CurrentPlayerState) const
{
	if (!InGameState || !CurrentPlayerState)
	{
		return;
	}

	const EARCharacterChoice CurrentChoice = CurrentPlayerState->GetCharacterPicked();
	if (CurrentChoice == EARCharacterChoice::None || !IsCharacterChoiceTakenByOther(InGameState, CurrentPlayerState, CurrentChoice))
	{
		return;
	}

	const EARCharacterChoice AlternateChoice = GetAlternateCharacterChoice(CurrentChoice);
	if (AlternateChoice != EARCharacterChoice::None && !IsCharacterChoiceTakenByOther(InGameState, CurrentPlayerState, AlternateChoice))
	{
		CurrentPlayerState->SetCharacterPicked(AlternateChoice);
		UE_LOG(ARLog, Log, TEXT("[GameMode] Character conflict resolved for '%s': %d -> %d"), *GetNameSafe(CurrentPlayerState), static_cast<int32>(CurrentChoice), static_cast<int32>(AlternateChoice));
		return;
	}

	// Never force unset; keep current assignment if both choices are occupied.
	UE_LOG(ARLog, Warning, TEXT("[GameMode] Character conflict unresolved for '%s'; keeping current assignment %d."),
		*GetNameSafe(CurrentPlayerState), static_cast<int32>(CurrentChoice));
}

void AARGameModeBase::HandleFirstSessionJoinSetup(AARGameStateBase* InGameState, AARPlayerStateBase* JoinedPlayerState, UARSaveSubsystem* SaveSubsystem) const
{
	if (!InGameState || !JoinedPlayerState)
	{
		return;
	}

	if (JoinedPlayerState->GetPlayerSlotId() <= 0)
	{
		JoinedPlayerState->SetPlayerSlotId(FindFirstFreePlayerSlotId(InGameState, JoinedPlayerState));
	}

	bool bHydratedFromSave = false;
	if (SaveSubsystem)
	{
		bHydratedFromSave = SaveSubsystem->TryHydratePlayerStateFromCurrentSave(JoinedPlayerState);
	}

	if (!bHydratedFromSave)
	{
		JoinedPlayerState->InitializeForFirstSessionJoin();

		// Fresh/no-save join policy: assign a random available canonical character.
		TArray<EARCharacterChoice> AvailableChoices;
		if (!IsCharacterChoiceTakenByOther(InGameState, JoinedPlayerState, EARCharacterChoice::Brother))
		{
			AvailableChoices.Add(EARCharacterChoice::Brother);
		}
		if (!IsCharacterChoiceTakenByOther(InGameState, JoinedPlayerState, EARCharacterChoice::Sister))
		{
			AvailableChoices.Add(EARCharacterChoice::Sister);
		}

		if (AvailableChoices.Num() > 0)
		{
			const int32 RandomIndex = FMath::RandHelper(AvailableChoices.Num());
			const EARCharacterChoice RandomChoice = AvailableChoices[RandomIndex];
			if (RandomChoice != EARCharacterChoice::None && RandomChoice != JoinedPlayerState->GetCharacterPicked())
			{
				JoinedPlayerState->SetCharacterPicked(RandomChoice);
			}
		}
	}

	ResolveCharacterChoiceConflict(InGameState, JoinedPlayerState);
	JoinedPlayerState->SetIsSetupComplete(true);
}

void AARGameModeBase::EnsureJoinedPlayerHasUniqueIdentity(AARGameStateBase* InGameState, AARPlayerStateBase* JoinedPlayerState) const
{
	if (!InGameState || !JoinedPlayerState)
	{
		return;
	}

	const int32 CurrentSlotId = JoinedPlayerState->GetPlayerSlotId();
	bool bSlotIdConflict = false;
	if (CurrentSlotId > 0)
	{
		for (APlayerState* PS : InGameState->PlayerArray)
		{
			const AARPlayerStateBase* OtherPlayer = Cast<AARPlayerStateBase>(PS);
			if (!IsIdentityRelevantPlayerState(OtherPlayer)
				|| OtherPlayer == JoinedPlayerState
				|| AreSameLogicalPlayerState(OtherPlayer, JoinedPlayerState))
			{
				continue;
			}

			if (OtherPlayer->GetPlayerSlotId() == CurrentSlotId)
			{
				bSlotIdConflict = true;
				break;
			}
		}
	}

	if (CurrentSlotId <= 0 || bSlotIdConflict)
	{
		const int32 ResolvedSlotId = FindFirstFreePlayerSlotId(InGameState, JoinedPlayerState);
		JoinedPlayerState->SetPlayerSlotId(ResolvedSlotId);
		UE_LOG(
			ARLog,
			Log,
			TEXT("[GameMode] Normalized runtime player slot id for '%s': %d -> %d"),
			*GetNameSafe(JoinedPlayerState),
			CurrentSlotId,
			ResolvedSlotId);
	}

	const FGameplayTag CurrentCharacterTag = ARPlayer::NormalizeCharacterTag(JoinedPlayerState->GetCurrentCharacterTag());
	if (!CurrentCharacterTag.IsValid())
	{
		EARCharacterChoice PreferredChoice = (JoinedPlayerState->GetPlayerSlotId() > 0 && JoinedPlayerState->GetPlayerSlotId() % 2 == 0)
			? EARCharacterChoice::Sister
			: EARCharacterChoice::Brother;
		EARCharacterChoice AlternateChoice = GetAlternateCharacterChoice(PreferredChoice);

		if (IsCharacterChoiceTakenByOther(InGameState, JoinedPlayerState, PreferredChoice)
			&& AlternateChoice != EARCharacterChoice::None
			&& !IsCharacterChoiceTakenByOther(InGameState, JoinedPlayerState, AlternateChoice))
		{
			PreferredChoice = AlternateChoice;
		}

		JoinedPlayerState->SetCharacterPicked(PreferredChoice);
	}

	ResolveCharacterChoiceConflict(InGameState, JoinedPlayerState);
}

FGameplayTag AARGameModeBase::GetPendingSpawnCharacterTagForController(const AController* Controller) const
{
	if (!Controller)
	{
		return FGameplayTag();
	}

	const TWeakObjectPtr<const AController> ControllerKey(Controller);
	if (const FGameplayTag* FoundTag = PendingSpawnCharacterTagsByController.Find(ControllerKey))
	{
		return *FoundTag;
	}

	return FGameplayTag();
}

void AARGameModeBase::CachePendingSpawnCharacterTagForController(const AController* Controller, const FGameplayTag& CharacterTag)
{
	for (auto It = PendingSpawnCharacterTagsByController.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}

	if (!Controller)
	{
		return;
	}

	const TWeakObjectPtr<const AController> ControllerKey(Controller);
	const FGameplayTag NormalizedCharacterTag = ARPlayer::NormalizeCharacterTag(CharacterTag);
	if (!NormalizedCharacterTag.IsValid())
	{
		PendingSpawnCharacterTagsByController.Remove(ControllerKey);
		return;
	}

	PendingSpawnCharacterTagsByController.FindOrAdd(ControllerKey) = NormalizedCharacterTag;
}

void AARGameModeBase::NormalizeConnectedPlayersIdentity(AARGameStateBase* InGameState) const
{
	if (!InGameState)
	{
		return;
	}

	TArray<AARPlayerStateBase*> Players;
	for (APlayerState* PS : InGameState->PlayerArray)
	{
		if (AARPlayerStateBase* Player = Cast<AARPlayerStateBase>(PS))
		{
			if (IsIdentityRelevantPlayerState(Player))
			{
				Players.Add(Player);
			}
		}
	}

	if (Players.IsEmpty())
	{
		return;
	}

	// Controller/profile identity normalization: runtime slot ids must be unique and non-zero.
	TSet<int32> UsedPlayerSlotIds;
	int32 NextAvailableSlotId = 1;
	for (AARPlayerStateBase* Player : Players)
	{
		if (!Player)
		{
			continue;
		}

		int32 RuntimeSlotId = Player->GetPlayerSlotId();
		if (RuntimeSlotId <= 0 || UsedPlayerSlotIds.Contains(RuntimeSlotId))
		{
			while (UsedPlayerSlotIds.Contains(NextAvailableSlotId))
			{
				++NextAvailableSlotId;
			}

			Player->SetPlayerSlotId(NextAvailableSlotId);
			RuntimeSlotId = NextAvailableSlotId;
			++NextAvailableSlotId;
		}
		else
		{
			NextAvailableSlotId = FMath::Max(NextAvailableSlotId, RuntimeSlotId + 1);
		}

		UsedPlayerSlotIds.Add(RuntimeSlotId);
	}

	// Character normalization: ensure every connected player has a valid canonical character assignment.
	for (AARPlayerStateBase* Player : Players)
	{
		if (!Player)
		{
			continue;
		}

		const FGameplayTag CurrentCharacterTag = ARPlayer::NormalizeCharacterTag(Player->GetCurrentCharacterTag());
		EARCharacterChoice ResolvedChoice = ARPlayer::GetCharacterChoiceForTag(CurrentCharacterTag);
		if (ResolvedChoice == EARCharacterChoice::None)
		{
			ResolvedChoice = Player->GetCharacterPicked();
		}
		if (ResolvedChoice == EARCharacterChoice::None)
		{
			ResolvedChoice = (Player->GetPlayerSlotId() > 0 && Player->GetPlayerSlotId() % 2 == 0)
				? EARCharacterChoice::Sister
				: EARCharacterChoice::Brother;
		}

		if (IsCharacterChoiceTakenByOther(InGameState, Player, ResolvedChoice))
		{
			const EARCharacterChoice AlternateChoice = GetAlternateCharacterChoice(ResolvedChoice);
			if (AlternateChoice != EARCharacterChoice::None && !IsCharacterChoiceTakenByOther(InGameState, Player, AlternateChoice))
			{
				ResolvedChoice = AlternateChoice;
			}
		}

		const FGameplayTag ResolvedTag = ARPlayer::GetCharacterTagForChoice(ResolvedChoice);
		if (ResolvedChoice != Player->GetCharacterPicked())
		{
			Player->SetCharacterPicked(ResolvedChoice);
		}
		else if (ResolvedTag.IsValid() && !CurrentCharacterTag.MatchesTagExact(ResolvedTag))
		{
			Player->SetCurrentCharacterTag(ResolvedTag);
		}
	}

	for (AARPlayerStateBase* Player : Players)
	{
		ResolveCharacterChoiceConflict(InGameState, Player);
	}

	// Loadout normalization: gameplay modes require at least one ship tag in loadout.
	const FGameplayTag TransitionModeTag = FGameplayTag::RequestGameplayTag(TEXT("Mode.Transition"), false);
	const bool bIsTransitionMode = TransitionModeTag.IsValid() && ModeTag.MatchesTagExact(TransitionModeTag);
	if (!bIsTransitionMode)
	{
		const FGameplayTag ShipRootTag = FGameplayTag::RequestGameplayTag(TEXT("Unlock.Ship"), false);
		const UARLoadoutSettings* LoadoutSettings = GetDefault<UARLoadoutSettings>();
		FGameplayTag DefaultShipTag;
		if (ShipRootTag.IsValid() && LoadoutSettings)
		{
			for (const FGameplayTag& DefaultTag : LoadoutSettings->DefaultPlayerLoadoutTags)
			{
				if (DefaultTag.IsValid() && DefaultTag.MatchesTag(ShipRootTag))
				{
					DefaultShipTag = DefaultTag;
					break;
				}
			}
		}

		for (AARPlayerStateBase* Player : Players)
		{
			if (!Player)
			{
				continue;
			}

			bool bHasShipTag = false;
			TArray<FGameplayTag> PlayerLoadoutTags;
			Player->GetCurrentCharacterLoadoutTags().GetGameplayTagArray(PlayerLoadoutTags);
			for (const FGameplayTag& LoadoutTag : PlayerLoadoutTags)
			{
				if (LoadoutTag.IsValid() && ShipRootTag.IsValid() && LoadoutTag.MatchesTag(ShipRootTag))
				{
					bHasShipTag = true;
					break;
				}
			}

			if (bHasShipTag)
			{
				continue;
			}

			if (DefaultShipTag.IsValid())
			{
				Player->UpdateLoadoutWithTag(DefaultShipTag);
				UE_LOG(
					ARLog,
					Warning,
					TEXT("[GameMode] Repaired missing ship loadout tag for '%s' by applying default '%s'."),
					*GetNameSafe(Player),
					*DefaultShipTag.ToString());
			}
			else
			{
				UE_LOG(
					ARLog,
					Error,
					TEXT("[GameMode] Player '%s' has no ship loadout tag and no default ship tag is configured in LoadoutSettings."),
					*GetNameSafe(Player));
			}
		}
	}

	// Color normalization: keep player color synchronized with character assignment.
	for (AARPlayerStateBase* Player : Players)
	{
		if (!Player)
		{
			continue;
		}

			const EARAffinityColor ExpectedColor = ResolveExpectedInvaderPlayerColor(Player->GetCharacterPicked());
		const EARAffinityColor CurrentColor = Player->GetInvaderPlayerColor();
		if (CurrentColor != ExpectedColor)
		{
			Player->SetInvaderPlayerColor(ExpectedColor);
			UE_LOG(ARLog, Verbose, TEXT("[GameMode] Identity normalize color for '%s': %d -> %d"),
				*GetNameSafe(Player), static_cast<int32>(CurrentColor), static_cast<int32>(ExpectedColor));
		}
	}
}

void AARGameModeBase::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	if (!HasAuthority() || !NewPlayer)
	{
		Super::HandleStartingNewPlayer_Implementation(NewPlayer);
		return;
	}

	const FGameplayTag TransitionModeTag = FGameplayTag::RequestGameplayTag(TEXT("Mode.Transition"), false);
	const bool bIsTransitionMode = TransitionModeTag.IsValid() && ModeTag.MatchesTagExact(TransitionModeTag);
	if (!bIsTransitionMode)
	{
		PrepareControllerForGameplaySpawn(NewPlayer);
	}

	AARPlayerStateBase* JoinedPS = NewPlayer->GetPlayerState<AARPlayerStateBase>();
	AARGameStateBase* GS = GetGameState<AARGameStateBase>();
	if (!JoinedPS || !GS)
	{
		CachePendingSpawnCharacterTagForController(NewPlayer, FGameplayTag());
		Super::HandleStartingNewPlayer_Implementation(NewPlayer);
		return;
	}

	UARSaveSubsystem* SaveSubsystem = nullptr;
	if (UGameInstance* GI = GetGameInstance())
	{
		SaveSubsystem = GI->GetSubsystem<UARSaveSubsystem>();
	}

	if (!JoinedPS->IsSetupComplete())
	{
		HandleFirstSessionJoinSetup(GS, JoinedPS, SaveSubsystem);
	}

	// Enforce stable unique runtime controller/profile id + character identity even when setup is already complete.
	EnsureJoinedPlayerHasUniqueIdentity(GS, JoinedPS);
	NormalizeConnectedPlayersIdentity(GS);
	const int32 PreSuperSlotId = JoinedPS->GetPlayerSlotId();
	const EARCharacterChoice PreSuperChoice = JoinedPS->GetCharacterPicked();
	const FGameplayTag PreSuperCharacterTag = ResolveCharacterTagForSpawnIdentity(JoinedPS);
	CachePendingSpawnCharacterTagForController(NewPlayer, PreSuperCharacterTag);
	if (UARCharacterSubsystem* CharacterSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UARCharacterSubsystem>() : nullptr)
	{
		bool bCreatedRuntime = false;
		if (AARCharacterStateRuntime* Runtime = CharacterSubsystem->EnsureCharacterRuntime(JoinedPS, PreSuperCharacterTag, bCreatedRuntime))
		{
			(void)bCreatedRuntime;
			JoinedPS->SetCurrentCharacterRuntime(Runtime);
		}
	}

	// Identity must be normalized before Super so spawn class/start selection sees non-unknown character.
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);

	// Re-run once after Super to catch any mirrored-state drift during spawn/possess setup.
	EnsureJoinedPlayerHasUniqueIdentity(GS, JoinedPS);
	NormalizeConnectedPlayersIdentity(GS);
	const int32 PostSuperSlotId = JoinedPS->GetPlayerSlotId();
	const EARCharacterChoice PostSuperChoice = JoinedPS->GetCharacterPicked();
	const FGameplayTag PostSuperCharacterTag = ResolveCharacterTagForSpawnIdentity(JoinedPS);

	UE_LOG(
		ARLog,
		Verbose,
		TEXT("[GameMode] HandleStartingNewPlayer identity after Super for '%s': PlayerSlotId %d->%d Choice %d->%d CharacterTag %s->%s Pawn=%s."),
		*GetNameSafe(JoinedPS),
		PreSuperSlotId,
		PostSuperSlotId,
		static_cast<int32>(PreSuperChoice),
		static_cast<int32>(PostSuperChoice),
		*PreSuperCharacterTag.ToString(),
		*PostSuperCharacterTag.ToString(),
		*GetNameSafe(NewPlayer->GetPawn()));

	const bool bIdentityDriftedDuringInitialSpawn = !PostSuperCharacterTag.MatchesTagExact(PreSuperCharacterTag);
	if (bIdentityDriftedDuringInitialSpawn)
	{
		UE_LOG(
			ARLog,
			Warning,
			TEXT("[GameMode] HandleStartingNewPlayer detected spawn identity drift for '%s' (CharacterTag %s -> %s). Correcting spawn with RestartPlayer."),
			*GetNameSafe(JoinedPS),
			*PreSuperCharacterTag.ToString(),
			*PostSuperCharacterTag.ToString());
	}

	// Recovery: if initial spawn failed OR identity drifted after Super normalization,
	// perform one corrective restart using the finalized post-Super character tag.
	if (!NewPlayer->GetPawn() || bIdentityDriftedDuringInitialSpawn)
	{
		CachePendingSpawnCharacterTagForController(NewPlayer, PostSuperCharacterTag);
		if (!bIsTransitionMode)
		{
			PrepareControllerForGameplaySpawn(NewPlayer);
		}

		if (bIdentityDriftedDuringInitialSpawn)
		{
			if (APawn* ExistingPawn = NewPlayer->GetPawn())
			{
				NewPlayer->UnPossess();
				ExistingPawn->Destroy();
			}
		}

		UE_LOG(
			ARLog,
			Warning,
			TEXT("[GameMode] HandleStartingNewPlayer running corrective RestartPlayer for '%s' (HasPawn=%d PlayerSlotId=%d CharacterTag=%s)."),
			*GetNameSafe(JoinedPS),
			NewPlayer->GetPawn() ? 1 : 0,
			PostSuperSlotId,
			*PostSuperCharacterTag.ToString());
		RestartPlayer(NewPlayer);
	}

	if (UARCharacterSubsystem* CharacterSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UARCharacterSubsystem>() : nullptr)
	{
		if (AARCharacterStateRuntime* Runtime = CharacterSubsystem->EnsureCharacterRuntime(JoinedPS, PostSuperCharacterTag))
		{
			JoinedPS->SetCurrentCharacterRuntime(Runtime);
			CharacterSubsystem->BindRuntimePawn(Runtime, NewPlayer->GetPawn());
		}
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UParleySpeakerSubsystem* SpeakerSubsystem = GI->GetSubsystem<UParleySpeakerSubsystem>())
		{
			SpeakerSubsystem->RefreshAllSpeakerTalkableStates();
		}
	}

	const int32 SlotIdBeforeBPJoin = JoinedPS->GetPlayerSlotId();
	const FGameplayTag CharacterTagBeforeBPJoin = ResolveCharacterTagForSpawnIdentity(JoinedPS);
	UE_LOG(
		ARLog,
		Log,
		TEXT("[GameMode] Player joined (pre-BP): %s (PlayerSlotId=%d, CharacterTag=%s, Setup=%s)"),
		*GetNameSafe(JoinedPS),
		SlotIdBeforeBPJoin,
		*CharacterTagBeforeBPJoin.ToString(),
		JoinedPS->IsSetupComplete() ? TEXT("true") : TEXT("false"));

	BP_OnPlayerJoined(JoinedPS);

	const int32 SlotIdAfterBPJoin = JoinedPS->GetPlayerSlotId();
	const FGameplayTag CharacterTagAfterBPJoin = ResolveCharacterTagForSpawnIdentity(JoinedPS);
	if (SlotIdAfterBPJoin != SlotIdBeforeBPJoin || !CharacterTagAfterBPJoin.MatchesTagExact(CharacterTagBeforeBPJoin))
	{
		UE_LOG(
			ARLog,
			Warning,
			TEXT("[GameMode] BP_OnPlayerJoined changed identity for '%s' (PlayerSlotId %d->%d CharacterTag %s->%s)."),
			*GetNameSafe(JoinedPS),
			SlotIdBeforeBPJoin,
			SlotIdAfterBPJoin,
			*CharacterTagBeforeBPJoin.ToString(),
			*CharacterTagAfterBPJoin.ToString());
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UARSessionSubsystem* SessionSubsystem = GI->GetSubsystem<UARSessionSubsystem>())
		{
			FARSessionResult SessionResult;
			if (!SessionSubsystem->RefreshJoinability(SessionResult) && SessionResult.ResultCode != EARSessionResultCode::SessionNotFound)
			{
				UE_LOG(ARLog, Verbose, TEXT("[GameMode] Session joinability refresh after join failed: %s"), *SessionResult.Error);
			}
		}
	}

	CachePendingSpawnCharacterTagForController(NewPlayer, FGameplayTag());
}

void AARGameModeBase::HandleSeamlessTravelPlayer(AController*& C)
{
	Super::HandleSeamlessTravelPlayer(C);

	if (!HasAuthority())
	{
		return;
	}

	APlayerController* PlayerController = Cast<APlayerController>(C);
	if (!PlayerController)
	{
		return;
	}

	const TSubclassOf<APlayerController> DesiredControllerClass = GetPlayerControllerClassToSpawnForSeamlessTravel(PlayerController);
	if (DesiredControllerClass && !PlayerController->IsA(DesiredControllerClass))
	{
		APlayerController* ReplacementController = SpawnPlayerControllerCommon(
			PlayerController->IsLocalPlayerController() ? ROLE_SimulatedProxy : ROLE_AutonomousProxy,
			PlayerController->GetFocalLocation(),
			PlayerController->GetControlRotation(),
			DesiredControllerClass);
		if (ReplacementController)
		{
			PlayerController->SeamlessTravelTo(ReplacementController);
			ReplacementController->SeamlessTravelFrom(PlayerController);
			SwapPlayerControllers(PlayerController, ReplacementController);
			C = ReplacementController;
			PlayerController = ReplacementController;
			UE_LOG(
				ARLog,
				Log,
				TEXT("[GameMode] Seamless controller class corrected for mode '%s': now '%s'."),
				*ModeTag.ToString(),
				*GetNameSafe(PlayerController->GetClass()));
		}
		else
		{
			UE_LOG(
				ARLog,
				Warning,
				TEXT("[GameMode] Failed to spawn desired seamless controller class '%s' for mode '%s'; keeping '%s'."),
				*GetNameSafe(DesiredControllerClass.Get()),
				*ModeTag.ToString(),
				*GetNameSafe(PlayerController->GetClass()));
		}
	}

	if (PlayerController->PlayerState)
	{
		PlayerController->PlayerState->SetIsSpectator(false);
	}

	const bool bWasSpectating = PlayerController->IsInState(NAME_Spectating);
	PlayerController->ChangeState(NAME_Playing);

	const FGameplayTag TransitionModeTag = FGameplayTag::RequestGameplayTag(TEXT("Mode.Transition"), false);
	const bool bIsTransitionMode = TransitionModeTag.IsValid() && ModeTag.MatchesTagExact(TransitionModeTag);
	if (!bIsTransitionMode)
	{
		PrepareControllerForGameplaySpawn(PlayerController);
	}
	APawn* ExistingPawn = PlayerController->GetPawn();
	const bool bHasSpectatorPawn = ExistingPawn && ExistingPawn->IsA(ASpectatorPawn::StaticClass());
	const bool bNeedsGameplayPawn = !bIsTransitionMode && (!ExistingPawn || bHasSpectatorPawn || bWasSpectating);
	UE_LOG(
		ARLog,
		Verbose,
		TEXT("[GameMode] HandleSeamlessTravelPlayer mode='%s' controller='%s' class='%s' pawn='%s' wasSpectating=%d hasSpectatorPawn=%d needsGameplayPawn=%d."),
		*ModeTag.ToString(),
		*GetNameSafe(PlayerController),
		*GetNameSafe(PlayerController->GetClass()),
		*GetNameSafe(ExistingPawn),
		bWasSpectating ? 1 : 0,
		bHasSpectatorPawn ? 1 : 0,
		bNeedsGameplayPawn ? 1 : 0);
	if (bNeedsGameplayPawn)
	{
		// Ensure slot/character are valid before restart so spawn class/start selection is identity-aware.
		if (AARGameStateBase* GS = GetGameState<AARGameStateBase>())
		{
			if (AARPlayerStateBase* JoinedPS = PlayerController->GetPlayerState<AARPlayerStateBase>())
			{
				EnsureJoinedPlayerHasUniqueIdentity(GS, JoinedPS);
			}

			NormalizeConnectedPlayersIdentity(GS);
		}

		if (ExistingPawn)
		{
			PlayerController->UnPossess();
			ExistingPawn->Destroy();
		}

		RestartPlayer(PlayerController);
	}

	// Seamless travel can produce transient duplicate slot mirrors while old/new PlayerState
	// instances overlap during handoff. Re-run authoritative identity normalization immediately.
	if (AARGameStateBase* GS = GetGameState<AARGameStateBase>())
	{
		if (AARPlayerStateBase* JoinedPS = PlayerController->GetPlayerState<AARPlayerStateBase>())
		{
			EnsureJoinedPlayerHasUniqueIdentity(GS, JoinedPS);
		}

		NormalizeConnectedPlayersIdentity(GS);
	}

	if (ShouldRunCharacterRuntimeBootstrap())
	{
		RunCharacterRuntimeBootstrapSequence(EARCharacterRuntimeBootstrapReason::SeamlessTravelRepair, PlayerController);
	}
	else if (UARCharacterSubsystem* CharacterSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UARCharacterSubsystem>() : nullptr)
	{
		if (AARPlayerStateBase* JoinedPS = PlayerController->GetPlayerState<AARPlayerStateBase>())
		{
			const FGameplayTag CharacterTag = ResolveCharacterTagForSpawnIdentity(JoinedPS);
			if (AARCharacterStateRuntime* Runtime = CharacterSubsystem->EnsureCharacterRuntime(JoinedPS, CharacterTag))
			{
				JoinedPS->SetCurrentCharacterRuntime(Runtime);
				CharacterSubsystem->BindRuntimePawn(Runtime, PlayerController->GetPawn());
			}
		}
	}
}

TSubclassOf<APlayerController> AARGameModeBase::GetPlayerControllerClassToSpawnForSeamlessTravel(APlayerController* PreviousPlayerController)
{
	TSubclassOf<APlayerController> ControllerClassToSpawn = Super::GetPlayerControllerClassToSpawnForSeamlessTravel(PreviousPlayerController);
	if (!PlayerControllerClass)
	{
		return ControllerClassToSpawn;
	}

	// During normal gameplay travel (including transition-map spectator flow), destination mode must
	// drive controller class selection via PlayerControllerClass. Only preserve replay spectator class
	// selection when the world is actually playing back a replay.
	if (UWorld* World = GetWorld())
	{
		if (!World->IsPlayingReplay())
		{
			return PlayerControllerClass;
		}
	}

	return ControllerClassToSpawn;
}

void AARGameModeBase::PreparePlayerSpawnIdentity(AController* Player, AARPlayerStateBase* PlayerState) const
{
	(void)Player;

	if (!HasAuthority() || !PlayerState)
	{
		return;
	}

	if (AARGameStateBase* GS = GetGameState<AARGameStateBase>())
	{
		if (!PlayerState->IsSetupComplete())
		{
			UARSaveSubsystem* SaveSubsystem = nullptr;
			if (UGameInstance* GI = GetGameInstance())
			{
				SaveSubsystem = GI->GetSubsystem<UARSaveSubsystem>();
			}

			HandleFirstSessionJoinSetup(GS, PlayerState, SaveSubsystem);
		}

		EnsureJoinedPlayerHasUniqueIdentity(GS, PlayerState);
		NormalizeConnectedPlayersIdentity(GS);
	}
}

AActor* AARGameModeBase::ChoosePlayerStart_Implementation(AController* Player)
{
	if (!Player)
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}

	AARPlayerStateBase* PlayerState = Player->GetPlayerState<AARPlayerStateBase>();
	if (!PlayerState)
	{
		CachePendingSpawnCharacterTagForController(Player, FGameplayTag());
		return Super::ChoosePlayerStart_Implementation(Player);
	}

	const FGameplayTag PreNormalizedCharacterTag = ResolveCharacterTagForSpawnIdentity(PlayerState);
	const bool bHadUnresolvedIdentityPrePass = !PreNormalizedCharacterTag.IsValid();

	// Spawn-point selection must never run against an unresolved identity; editor/raw-map flows
	// can still reach here before earlier join normalization has finalized character assignment.
	PreparePlayerSpawnIdentity(Player, PlayerState);

	FGameplayTag CharacterTag = ResolveCharacterTagForSpawnIdentity(PlayerState);
	const FGameplayTag PendingCharacterTag = ARPlayer::NormalizeCharacterTag(GetPendingSpawnCharacterTagForController(Player));
	if (PendingCharacterTag.IsValid() && !CharacterTag.MatchesTagExact(PendingCharacterTag))
	{
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[GameMode] ChoosePlayerStart using pending cached character tag for '%s': %s -> %s."),
			*GetNameSafe(PlayerState),
			*CharacterTag.ToString(),
			*PendingCharacterTag.ToString());
		CharacterTag = PendingCharacterTag;
	}
	CachePendingSpawnCharacterTagForController(Player, CharacterTag);
	if (bHadUnresolvedIdentityPrePass)
	{
		UE_LOG(
			ARLog,
			Warning,
			TEXT("[GameMode] ChoosePlayerStart saw unresolved pre-normalization identity for '%s' (PreCharacter=%s -> PostCharacter=%s)."),
			*GetNameSafe(PlayerState),
			*PreNormalizedCharacterTag.ToString(),
			*CharacterTag.ToString());
	}

	if (!CharacterTag.IsValid())
	{
		UE_LOG(
			ARLog,
			Warning,
			TEXT("[GameMode] ChoosePlayerStart still has invalid character identity for '%s' after normalization (CurrentCharacterTag=%s)."),
			*GetNameSafe(PlayerState),
			*PlayerState->GetCurrentCharacterTag().ToString());
	}

	TArray<FGameplayTag> IdentityQueryTags;
	// Character-authored lanes should win when both character and slot starts are present.
	if (CharacterTag.IsValid() && !IdentityQueryTags.Contains(CharacterTag))
	{
		IdentityQueryTags.Add(CharacterTag);
	}
	if (IdentityQueryTags.IsEmpty())
	{
		UE_LOG(
			ARLog,
			Warning,
			TEXT("[GameMode] ChoosePlayerStart produced no identity query tags for '%s'; falling back to default PlayerStart."),
			*GetNameSafe(PlayerState));
	}

	TArray<AARTaggedPlayerStart*> TaggedStarts;
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AARTaggedPlayerStart> It(World); It; ++It)
		{
			AARTaggedPlayerStart* Start = *It;
			if (IsValid(Start))
			{
				TaggedStarts.Add(Start);
			}
		}
	}
	UE_LOG(
		ARLog,
		Verbose,
		TEXT("[GameMode] ChoosePlayerStart evaluating '%s': CharacterTag=%s QueryTags=%d TaggedStarts=%d."),
		*GetNameSafe(PlayerState),
		*CharacterTag.ToString(),
		IdentityQueryTags.Num(),
		TaggedStarts.Num());

	auto FindMatchingStart = [&TaggedStarts](const FGameplayTag& QueryTag, const bool bRequireExact) -> AActor*
	{
		if (!QueryTag.IsValid())
		{
			return nullptr;
		}

		for (AARTaggedPlayerStart* Start : TaggedStarts)
		{
			if (IsValid(Start) && Start->MatchesSpawnIdentityTag(QueryTag, bRequireExact))
			{
				return Start;
			}
		}

		return nullptr;
	};

	for (const FGameplayTag& QueryTag : IdentityQueryTags)
	{
		if (AActor* MatchedExact = FindMatchingStart(QueryTag, true))
		{
			UE_LOG(
				ARLog,
				Verbose,
				TEXT("[GameMode] ChoosePlayerStart exact match for '%s': QueryTag=%s Start=%s."),
				*GetNameSafe(PlayerState),
				*QueryTag.ToString(),
				*GetNameSafe(MatchedExact));
			return MatchedExact;
		}
	}

	for (const FGameplayTag& QueryTag : IdentityQueryTags)
	{
		if (AActor* MatchedLoose = FindMatchingStart(QueryTag, false))
		{
			UE_LOG(
				ARLog,
				Verbose,
				TEXT("[GameMode] ChoosePlayerStart loose match for '%s': QueryTag=%s Start=%s."),
				*GetNameSafe(PlayerState),
				*QueryTag.ToString(),
				*GetNameSafe(MatchedLoose));
			return MatchedLoose;
		}
	}

	UE_LOG(
		ARLog,
		Warning,
		TEXT("[GameMode] ChoosePlayerStart found no tagged match for '%s' (CharacterTag=%s). Falling back to default PlayerStart."),
		*GetNameSafe(PlayerState),
		*CharacterTag.ToString());

	if (TaggedStarts.Num() > 0)
	{
		TArray<FString> TaggedStartDebugRows;
		TaggedStartDebugRows.Reserve(TaggedStarts.Num());
		for (AARTaggedPlayerStart* Start : TaggedStarts)
		{
			if (!IsValid(Start))
			{
				continue;
			}

			TaggedStartDebugRows.Add(FString::Printf(
				TEXT("%s{SpawnIdentityTag=%s,PlayerStartTag=%s,ExactOnly=%d}"),
				*GetNameSafe(Start),
				*Start->SpawnIdentityTag.ToString(),
				*Start->PlayerStartTag.ToString(),
				Start->bExactTagMatchOnly ? 1 : 0));
		}

		UE_LOG(
			ARLog,
			Warning,
			TEXT("[GameMode] ChoosePlayerStart tagged-start snapshot (%d): %s"),
			TaggedStartDebugRows.Num(),
			*FString::Join(TaggedStartDebugRows, TEXT(" | ")));
	}

	return Super::ChoosePlayerStart_Implementation(Player);
}

APawn* AARGameModeBase::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform)
{
	if (HasAuthority() && NewPlayer)
	{
		AARPlayerStateBase* PlayerState = NewPlayer->GetPlayerState<AARPlayerStateBase>();
		UARCharacterSubsystem* CharacterSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UARCharacterSubsystem>() : nullptr;
		if (PlayerState && CharacterSubsystem)
		{
			FGameplayTag CharacterTag = ARPlayer::NormalizeCharacterTag(GetPendingSpawnCharacterTagForController(NewPlayer));
			if (!CharacterTag.IsValid())
			{
				CharacterTag = ARPlayer::NormalizeCharacterTag(PlayerState->GetCurrentCharacterTag());
			}

			if (CharacterTag.IsValid())
			{
				bool bCreatedRuntime = false;
				AARCharacterStateRuntime* Runtime = CharacterSubsystem->EnsureCharacterRuntime(PlayerState, CharacterTag, bCreatedRuntime);
				(void)bCreatedRuntime;
				if (Runtime)
				{
					PlayerState->SetCurrentCharacterRuntime(Runtime);
					if (APawn* ExistingPawn = Runtime->GetCurrentPawn())
					{
						if (IsValid(ExistingPawn) && !ExistingPawn->GetController())
						{
							ExistingPawn->SetActorTransform(SpawnTransform, false, nullptr, ETeleportType::TeleportPhysics);
							CharacterSubsystem->BindRuntimePawn(Runtime, ExistingPawn);
							return ExistingPawn;
						}
					}
				}
			}
		}
	}

	APawn* SpawnedPawn = Super::SpawnDefaultPawnAtTransform_Implementation(NewPlayer, SpawnTransform);
	if (!HasAuthority() || !NewPlayer)
	{
		return SpawnedPawn;
	}

	APawn* FinalPawn = SpawnedPawn;
	if (!FinalPawn)
	{
		UClass* PawnClass = GetDefaultPawnClassForController(NewPlayer);
		UWorld* World = GetWorld();
		if (!World || !PawnClass)
		{
			return SpawnedPawn;
		}

		FActorSpawnParameters SpawnInfo;
		SpawnInfo.Instigator = GetInstigator();
		SpawnInfo.ObjectFlags |= RF_Transient;
		SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		APawn* CollisionAdjustedPawn = World->SpawnActor<APawn>(PawnClass, SpawnTransform, SpawnInfo);
		if (CollisionAdjustedPawn)
		{
			UE_LOG(
				ARLog,
				Warning,
				TEXT("[GameMode] Super pawn spawn failed; forced collision-adjusted pawn spawn for controller '%s' using class '%s'."),
				*GetNameSafe(NewPlayer),
				*GetNameSafe(PawnClass));
		}

		FinalPawn = CollisionAdjustedPawn ? CollisionAdjustedPawn : SpawnedPawn;
	}

	if (FinalPawn)
	{
		AARPlayerStateBase* PlayerState = NewPlayer->GetPlayerState<AARPlayerStateBase>();
		UARCharacterSubsystem* CharacterSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UARCharacterSubsystem>() : nullptr;
		if (PlayerState && CharacterSubsystem)
		{
			FGameplayTag CharacterTag = ARPlayer::NormalizeCharacterTag(GetPendingSpawnCharacterTagForController(NewPlayer));
			if (!CharacterTag.IsValid())
			{
				CharacterTag = ARPlayer::NormalizeCharacterTag(PlayerState->GetCurrentCharacterTag());
			}

			if (AARCharacterStateRuntime* Runtime = CharacterSubsystem->EnsureCharacterRuntime(PlayerState, CharacterTag))
			{
				PlayerState->SetCurrentCharacterRuntime(Runtime);
				CharacterSubsystem->BindRuntimePawn(Runtime, FinalPawn);
			}
		}
	}

	return FinalPawn;
}

void AARGameModeBase::Logout(AController* Exiting)
{
	CachePendingSpawnCharacterTagForController(Exiting, FGameplayTag());
	if (APlayerController* ExitingPlayerController = Cast<APlayerController>(Exiting))
	{
		const TWeakObjectPtr<APlayerController> ExitingControllerKey(ExitingPlayerController);
		ActiveCharacterSwitchRequests.Remove(ExitingControllerKey);
		CharacterSwitchRequestLatchUntilRelease.Remove(ExitingControllerKey);
	}
	CleanupCharacterSwitchRequests();

	AARPlayerStateBase* LeavingPS = Exiting ? Exiting->GetPlayerState<AARPlayerStateBase>() : nullptr;

	BP_OnPlayerLeft(LeavingPS);
	UE_LOG(ARLog, Log, TEXT("[GameMode] Player left: %s"), *GetNameSafe(LeavingPS));

	Super::Logout(Exiting);

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UParleySpeakerSubsystem* SpeakerSubsystem = GI->GetSubsystem<UParleySpeakerSubsystem>())
		{
			SpeakerSubsystem->RefreshAllSpeakerTalkableStates();
		}

		if (UARSessionSubsystem* SessionSubsystem = GI->GetSubsystem<UARSessionSubsystem>())
		{
			FARSessionResult SessionResult;
			if (!SessionSubsystem->RefreshJoinability(SessionResult) && SessionResult.ResultCode != EARSessionResultCode::SessionNotFound)
			{
				UE_LOG(ARLog, Verbose, TEXT("[GameMode] Session joinability refresh after leave failed: %s"), *SessionResult.Error);
			}
		}
	}
}

void AARGameModeBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	PendingSpawnCharacterTagsByController.Reset();
	ActiveCharacterSwitchRequests.Reset();
	CharacterSwitchRequestLatchUntilRelease.Reset();

	if (HasAuthority() && EndPlayReason == EEndPlayReason::Quit && bAutosaveOnQuit)
	{
		if (UARSaveSubsystem* SaveSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UARSaveSubsystem>() : nullptr)
		{
			FARSaveResult SaveResult;
			const bool bSaved = SaveSubsystem->RequestAutosaveIfDirty(true, SaveResult);
			if (bSaved)
			{
				UE_LOG(ARLog, Log, TEXT("[GameMode] Autosave-on-quit succeeded for '%s' (Slot=%s Rev=%d)."),
					*GetNameSafe(this), *SaveResult.SlotName.ToString(), SaveResult.SlotNumber);
			}
			else if (!SaveResult.Error.IsEmpty())
			{
				UE_LOG(ARLog, Warning, TEXT("[GameMode] Autosave-on-quit failed for '%s': %s"),
					*GetNameSafe(this), *SaveResult.Error);
			}
		}
	}

	Super::EndPlay(EndPlayReason);
}

bool AARGameModeBase::TryStartTravel(const FString& URL, const FString& Options, bool bSkipReadyChecks, bool bAbsolute, bool bSkipGameNotify, bool bUseOpenLevelInPIE, const EARTravelRoutePolicy RoutePolicy)
{
	if (!HasAuthority())
	{
		UE_LOG(ARLog, Warning, TEXT("[GameMode] TryStartTravel ignored: not authority."));
		return false;
	}

	if (URL.IsEmpty())
	{
		UE_LOG(ARLog, Warning, TEXT("[GameMode] TryStartTravel failed: URL is empty."));
		return false;
	}

	if (!PreStartTravel(URL, Options, bSkipReadyChecks))
	{
		UE_LOG(ARLog, Warning, TEXT("[GameMode] TryStartTravel blocked by PreStartTravel hook."));
		return false;
	}

	if (!GameState)
	{
		UE_LOG(ARLog, Warning, TEXT("[GameMode] TryStartTravel failed: missing GameState."));
		return false;
	}

	TArray<FString> NotReadyPlayers;
	if (!bSkipReadyChecks)
	{
		for (APlayerState* PS : GameState->PlayerArray)
		{
			if (const AARPlayerStateBase* ARPS = Cast<AARPlayerStateBase>(PS))
			{
				if (!ARPS->IsTravelReady())
				{
					NotReadyPlayers.Add(GetNameSafe(ARPS));
				}
			}
		}
	}

	if (!bSkipReadyChecks && NotReadyPlayers.Num() > 0)
	{
		UE_LOG(ARLog, Warning, TEXT("[GameMode] TryStartTravel blocked; not ready: %s"), *FString::Join(NotReadyPlayers, TEXT(", ")));
		return false;
	}

	const FString RequestedURLWithOptions = ARTransition::AppendTravelOptions(URL, Options);
	FString TravelURL = BuildModeTravelURL(RequestedURLWithOptions, RoutePolicy);

	UARTravelSubsystem* TravelSubsystem = nullptr;
	if (UGameInstance* GI = GetGameInstance())
	{
		TravelSubsystem = GI->GetSubsystem<UARTravelSubsystem>();
	}

	if (!TravelSubsystem)
	{
		UE_LOG(ARLog, Warning, TEXT("[GameMode] TryStartTravel failed: TravelSubsystem missing."));
		return false;
	}

#if WITH_EDITOR
	if (bUseOpenLevelInPIE && GetWorld() && GetWorld()->WorldType == EWorldType::PIE)
	{
		FString LevelName = TravelURL;
		FString OpenLevelOptions;
		int32 QueryIndex = INDEX_NONE;
		if (TravelURL.FindChar(TEXT('?'), QueryIndex))
		{
			LevelName = TravelURL.Left(QueryIndex);
			OpenLevelOptions = TravelURL.Mid(QueryIndex + 1);
		}

		OpenLevelOptions = ARTransition::EnsureTravelOption(OpenLevelOptions, TEXT("listen"));

		UE_LOG(ARLog, Log, TEXT("[GameMode] TryStartTravel PIE fallback -> OpenLevel Level='%s' Options='%s'"), *LevelName, *OpenLevelOptions);
		return TravelSubsystem->RequestOpenLevel(LevelName, OpenLevelOptions, bSkipReadyChecks, bAbsolute, bSaveOnModeExit);
	}
#endif

	UE_LOG(
		ARLog,
		Log,
		TEXT("[GameMode] TryStartTravel -> RequestedURL='%s' FinalURL='%s' Options='%s' RoutePolicy=%s SkipReady=%s SaveOnExit=%s"),
		*URL,
		*TravelURL,
		*Options,
		*ARTransition::LexToString(RoutePolicy),
		bSkipReadyChecks ? TEXT("true") : TEXT("false"),
		bSaveOnModeExit ? TEXT("true") : TEXT("false"));
	return TravelSubsystem->RequestServerTravel(TravelURL, bSkipReadyChecks, bAbsolute, bSkipGameNotify, bSaveOnModeExit);
}

bool AARGameModeBase::PreStartTravel(const FString& URL, const FString& Options, bool bSkipReadyChecks)
{
	(void)URL;
	(void)Options;
	(void)bSkipReadyChecks;
	return true;
}

void AARGameModeBase::BP_OnPlayerJoined_Implementation(AARPlayerStateBase* JoinedPlayerState)
{
}

void AARGameModeBase::BP_OnPlayerLeft_Implementation(AARPlayerStateBase* LeftPlayerState)
{
}

