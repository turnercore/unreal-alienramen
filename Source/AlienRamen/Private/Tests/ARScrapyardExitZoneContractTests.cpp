#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "ARPlayerStateBase.h"
#include "ARCarryItemBase.h"
#include "ARScrapyardExitZoneActor.h"
#include "ARScrapyardGameState.h"
#include "ARScrapyardPlayerController.h"
#include "ARShopCarryComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"

namespace
{
	UWorld* ResolveAutomationWorld_ScrapyardExit()
	{
		if (!GEngine)
		{
			return nullptr;
		}

		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if ((Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game) && Context.World())
			{
				return Context.World();
			}
		}

		return nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FARScrapyardExitZoneDepositWithdrawContractTest,
	"AlienRamen.Scrapyard.Exit.DepositWithdrawContract",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FARScrapyardExitZoneDepositWithdrawContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* TestWorld = ResolveAutomationWorld_ScrapyardExit();
	if (!TestNotNull(TEXT("Automation test world available"), TestWorld))
	{
		return false;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AARScrapyardGameState* ScrapyardGameState = TestWorld->SpawnActor<AARScrapyardGameState>(
		AARScrapyardGameState::StaticClass(),
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParams);
	AARScrapyardExitZoneActor* ExitZone = TestWorld->SpawnActor<AARScrapyardExitZoneActor>(
		AARScrapyardExitZoneActor::StaticClass(),
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParams);
	AARScrapyardPlayerController* Controller = TestWorld->SpawnActor<AARScrapyardPlayerController>(
		AARScrapyardPlayerController::StaticClass(),
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParams);
	AARPlayerStateBase* PlayerState = TestWorld->SpawnActor<AARPlayerStateBase>(
		AARPlayerStateBase::StaticClass(),
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParams);
	ACharacter* Pawn = TestWorld->SpawnActor<ACharacter>(
		ACharacter::StaticClass(),
		FVector(500.0f, 0.0f, 100.0f),
		FRotator::ZeroRotator,
		SpawnParams);
	AARCarryItemBase* ItemActor = TestWorld->SpawnActor<AARCarryItemBase>(
		AARCarryItemBase::StaticClass(),
		FVector(500.0f, 20.0f, 100.0f),
		FRotator::ZeroRotator,
		SpawnParams);

	if (!TestNotNull(TEXT("Spawned scrapyard game state"), ScrapyardGameState)
		|| !TestNotNull(TEXT("Spawned scrapyard exit zone"), ExitZone)
		|| !TestNotNull(TEXT("Spawned scrapyard controller"), Controller)
		|| !TestNotNull(TEXT("Spawned player state"), PlayerState)
		|| !TestNotNull(TEXT("Spawned pawn"), Pawn)
		|| !TestNotNull(TEXT("Spawned scrapyard carry item"), ItemActor))
	{
		return false;
	}

	// Exit-zone deposit uses World->GetGameState lookup.
	TestWorld->SetGameState(ScrapyardGameState);
	Controller->SetPlayerState(PlayerState);
	Pawn->SetPlayerState(PlayerState);
	Controller->Possess(Pawn);

	UARShopCarryComponent* CarryComponent = NewObject<UARShopCarryComponent>(Pawn, UARShopCarryComponent::StaticClass(), TEXT("TestCarryComponent"));
	if (!TestNotNull(TEXT("Created carry component"), CarryComponent))
	{
		return false;
	}
	CarryComponent->RegisterComponent();
	ItemActor->SetFallbackScrapCost(5);

	TestTrue(TEXT("Controller can hold scrapyard item before deposit"), CarryComponent->TrySetHeldActor(ItemActor));
	TestNotNull(TEXT("Held actor set"), CarryComponent->GetHeldActor());

	// Trigger overlap registration so this controller is eligible for zone operations.
	const bool bMovedIntoZone = Pawn->SetActorLocation(FVector(0.0f, 0.0f, 100.0f), false, nullptr, ETeleportType::TeleportPhysics);
	TestTrue(TEXT("Pawn moved into exit zone"), bMovedIntoZone);
	if (UPrimitiveComponent* PawnRootPrimitive = Cast<UPrimitiveComponent>(Pawn->GetRootComponent()))
	{
		PawnRootPrimitive->UpdateOverlaps();
	}
	if (UPrimitiveComponent* ExitRootPrimitive = Cast<UPrimitiveComponent>(ExitZone->GetRootComponent()))
	{
		ExitRootPrimitive->UpdateOverlaps();
	}

	TestTrue(TEXT("Exit zone accepts held scrapyard item"), ExitZone->TryDepositHeldItem(Controller));
	TestNull(TEXT("Carry component clears held actor after deposit"), CarryComponent->GetHeldActor());
	TestEqual(TEXT("Deposited item count increments"), ExitZone->GetDepositedItems().Num(), 1);
	TestEqual(TEXT("Deposited reserved scrap value tracks deposited item"), ExitZone->GetDepositedReservedScrapValue(), 5);

	TestTrue(TEXT("Exit zone withdraw returns deposited item"), ExitZone->TryWithdrawDepositedItem(Controller, ItemActor));
	TestEqual(TEXT("Deposited item count clears on withdraw"), ExitZone->GetDepositedItems().Num(), 0);
	TestEqual(TEXT("Reserved scrap value clears on withdraw"), ExitZone->GetDepositedReservedScrapValue(), 0);
	TestEqual(TEXT("Carry component receives withdrawn item"), CarryComponent->GetHeldActor(), static_cast<AActor*>(ItemActor));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
