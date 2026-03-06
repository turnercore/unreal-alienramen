#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "ARInvaderGameState.h"
#include "ARPlayerStateBase.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameplayTagContainer.h"

namespace
{
	UWorld* ResolveAutomationWorld()
	{
		if (!GEngine)
		{
			return nullptr;
		}

		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				if (UWorld* World = Context.World())
				{
					return World;
				}
			}
		}

		return nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FARInvaderOfferPresenceLifecycleTest,
	"AlienRamen.Invader.SpiceTrack.OfferPresenceLifecycle",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FARInvaderOfferPresenceLifecycleTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* TestWorld = ResolveAutomationWorld();
	if (!TestNotNull(TEXT("Test world (PIE/Game context)"), TestWorld))
	{
		return false;
	}

	const FGameplayTag OfferedTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Hat.Activate.StickyHand")), false);
	const FGameplayTag NonOfferedTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Hat.Activate.Vacuum")), false);
	if (!OfferedTag.IsValid() || !NonOfferedTag.IsValid())
	{
		AddError(TEXT("Missing gameplay tags required by offer-session tests."));
		return false;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AARInvaderGameState* InvaderGameState = TestWorld->SpawnActor<AARInvaderGameState>(
		AARInvaderGameState::StaticClass(),
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParams);
	AARPlayerStateBase* P1State = TestWorld->SpawnActor<AARPlayerStateBase>(
		AARPlayerStateBase::StaticClass(),
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParams);
	if (!TestNotNull(TEXT("Spawned invader game state"), InvaderGameState) ||
		!TestNotNull(TEXT("Spawned player state"), P1State))
	{
		if (P1State)
		{
			P1State->Destroy();
		}
		if (InvaderGameState)
		{
			InvaderGameState->Destroy();
		}
		return false;
	}

	P1State->SetPlayerSlot(EARPlayerSlot::P1);
	TestEqual(TEXT("Player slot was assigned"), P1State->GetPlayerSlot(), EARPlayerSlot::P1);

	TestFalse(
		TEXT("Presence update is rejected while session is inactive"),
		InvaderGameState->SetOfferPresence(P1State, OfferedTag, 1, FVector2D(0.5f, 0.5f), true));

	FARInvaderFullBlastSessionState ActiveSession = InvaderGameState->GetFullBlastSession();
	ActiveSession = FARInvaderFullBlastSessionState();
	ActiveSession.bIsActive = true;
	ActiveSession.RequestingPlayerSlot = EARPlayerSlot::P1;
	ActiveSession.ActivationTier = 2;
	{
		FARInvaderUpgradeOffer OfferedUpgrade;
		OfferedUpgrade.UpgradeTag = OfferedTag;
		OfferedUpgrade.OfferedLevel = 2;
		ActiveSession.Offers.Add(OfferedUpgrade);
	}
	const_cast<FARInvaderFullBlastSessionState&>(InvaderGameState->GetFullBlastSession()) = ActiveSession;

	TestTrue(
		TEXT("Presence update accepted during active session"),
		InvaderGameState->SetOfferPresence(P1State, NonOfferedTag, 2, FVector2D(2.0f, -1.0f), true));

	const TArray<FARInvaderOfferPresenceState>& PresenceAfterInvalidHover = InvaderGameState->GetOfferPresenceStates();
	if (!TestEqual(TEXT("Presence entry count after first update"), PresenceAfterInvalidHover.Num(), 1))
	{
		P1State->Destroy();
		InvaderGameState->Destroy();
		return false;
	}

	TestEqual(TEXT("Presence player slot is P1"), PresenceAfterInvalidHover[0].PlayerSlot, EARPlayerSlot::P1);
	TestFalse(TEXT("Non-offered hover tag is scrubbed"), PresenceAfterInvalidHover[0].HoveredUpgradeTag.IsValid());
	TestEqual(TEXT("Hovered destination slot persisted"), PresenceAfterInvalidHover[0].HoveredDestinationSlot, 2);
	TestTrue(TEXT("Presence cursor marked as present"), PresenceAfterInvalidHover[0].bHasCursor);
	TestTrue(
		TEXT("Presence cursor X clamped to 1"),
		FMath::IsNearlyEqual(PresenceAfterInvalidHover[0].CursorNormalized.X, 1.0, UE_KINDA_SMALL_NUMBER));
	TestTrue(
		TEXT("Presence cursor Y clamped to 0"),
		FMath::IsNearlyEqual(PresenceAfterInvalidHover[0].CursorNormalized.Y, 0.0, UE_KINDA_SMALL_NUMBER));

	TestTrue(
		TEXT("Presence update accepts offered tag"),
		InvaderGameState->SetOfferPresence(P1State, OfferedTag, 3, FVector2D(0.25f, 0.75f), false));
	const TArray<FARInvaderOfferPresenceState>& PresenceAfterOfferHover = InvaderGameState->GetOfferPresenceStates();
	TestEqual(TEXT("Presence entry count remains one after update"), PresenceAfterOfferHover.Num(), 1);
	TestEqual(TEXT("Offered hover tag persisted"), PresenceAfterOfferHover[0].HoveredUpgradeTag, OfferedTag);
	TestEqual(TEXT("Hovered destination slot updated"), PresenceAfterOfferHover[0].HoveredDestinationSlot, 3);
	TestFalse(TEXT("Presence cursor cleared when bHasCursor is false"), PresenceAfterOfferHover[0].bHasCursor);
	TestTrue(
		TEXT("Presence cursor reset X when disabled"),
		FMath::IsNearlyEqual(PresenceAfterOfferHover[0].CursorNormalized.X, 0.0, UE_KINDA_SMALL_NUMBER));
	TestTrue(
		TEXT("Presence cursor reset Y when disabled"),
		FMath::IsNearlyEqual(PresenceAfterOfferHover[0].CursorNormalized.Y, 0.0, UE_KINDA_SMALL_NUMBER));

	TestTrue(
		TEXT("Idempotent presence update still succeeds"),
		InvaderGameState->SetOfferPresence(P1State, OfferedTag, 3, FVector2D(0.25f, 0.75f), false));
	TestEqual(
		TEXT("Presence entry count stays one after idempotent update"),
		InvaderGameState->GetOfferPresenceStates().Num(),
		1);

	TestTrue(TEXT("Presence clear succeeds"), InvaderGameState->ClearOfferPresence(P1State));
	TestEqual(TEXT("Presence entry count is zero after clear"), InvaderGameState->GetOfferPresenceStates().Num(), 0);
	TestFalse(TEXT("Clearing already-empty presence fails"), InvaderGameState->ClearOfferPresence(P1State));

	P1State->Destroy();
	InvaderGameState->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FARInvaderOfferChooserRestrictionTest,
	"AlienRamen.Invader.SpiceTrack.OfferChooserRestriction",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FARInvaderOfferChooserRestrictionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* TestWorld = ResolveAutomationWorld();
	if (!TestNotNull(TEXT("Test world (PIE/Game context)"), TestWorld))
	{
		return false;
	}

	const FGameplayTag OfferedTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Hat.Activate.StickyHand")), false);
	if (!OfferedTag.IsValid())
	{
		AddError(TEXT("Missing gameplay tag 'Ability.Hat.Activate.StickyHand'."));
		return false;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AARInvaderGameState* InvaderGameState = TestWorld->SpawnActor<AARInvaderGameState>(
		AARInvaderGameState::StaticClass(),
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParams);
	AARPlayerStateBase* P1State = TestWorld->SpawnActor<AARPlayerStateBase>(
		AARPlayerStateBase::StaticClass(),
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParams);
	AARPlayerStateBase* P2State = TestWorld->SpawnActor<AARPlayerStateBase>(
		AARPlayerStateBase::StaticClass(),
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParams);
	if (!TestNotNull(TEXT("Spawned invader game state"), InvaderGameState) ||
		!TestNotNull(TEXT("Spawned P1 player state"), P1State) ||
		!TestNotNull(TEXT("Spawned P2 player state"), P2State))
	{
		if (P2State)
		{
			P2State->Destroy();
		}
		if (P1State)
		{
			P1State->Destroy();
		}
		if (InvaderGameState)
		{
			InvaderGameState->Destroy();
		}
		return false;
	}

	P1State->SetPlayerSlot(EARPlayerSlot::P1);
	P2State->SetPlayerSlot(EARPlayerSlot::P2);

	FARInvaderFullBlastSessionState ActiveSession = FARInvaderFullBlastSessionState();
	ActiveSession.bIsActive = true;
	ActiveSession.RequestingPlayerSlot = EARPlayerSlot::P1;
	ActiveSession.ActivationTier = 3;
	{
		FARInvaderUpgradeOffer OfferedUpgrade;
		OfferedUpgrade.UpgradeTag = OfferedTag;
		OfferedUpgrade.OfferedLevel = 3;
		ActiveSession.Offers.Add(OfferedUpgrade);
	}
	const_cast<FARInvaderFullBlastSessionState&>(InvaderGameState->GetFullBlastSession()) = ActiveSession;

	TestFalse(
		TEXT("Non-chooser player cannot skip active offer session"),
		InvaderGameState->ResolveFullBlastSkip(P2State));
	TestFalse(
		TEXT("Non-chooser player cannot select from active offer session"),
		InvaderGameState->ResolveFullBlastSelection(P2State, OfferedTag, 1));

	TestTrue(TEXT("Offer session remains active after blocked non-chooser actions"), InvaderGameState->GetFullBlastSession().bIsActive);
	TestEqual(TEXT("Offer chooser remains requester slot P1"), InvaderGameState->GetFullBlastSession().RequestingPlayerSlot, EARPlayerSlot::P1);
	TestEqual(TEXT("Offer list remains unchanged"), InvaderGameState->GetFullBlastSession().Offers.Num(), 1);

	P2State->Destroy();
	P1State->Destroy();
	InvaderGameState->Destroy();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
