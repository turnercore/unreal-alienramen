#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "ARRamenBowlActor.h"
#include "ARRamenMeatActor.h"
#include "ARShopStationActor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

namespace
{
	UWorld* ResolveAutomationWorld_Interactables()
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
	FARShopMeatStorageReturnArmingTest,
	"AlienRamen.Shop.Interactables.MeatStorageReturnArming",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FARShopMeatStorageReturnArmingTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* TestWorld = ResolveAutomationWorld_Interactables();
	if (!TestNotNull(TEXT("Automation test world available"), TestWorld))
	{
		return false;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AARRamenMeatActor* MeatActor = TestWorld->SpawnActor<AARRamenMeatActor>(
		AARRamenMeatActor::StaticClass(),
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParams);
	if (!TestNotNull(TEXT("Spawned ramen meat actor"), MeatActor))
	{
		return false;
	}

	TestFalse(TEXT("Fresh meat is not armed for storage-return distance gate"), MeatActor->HasMovedAwayForStorageReturn(200.0f));
	MeatActor->ArmStorageReturn();
	TestTrue(TEXT("Armed meat bypasses storage-return distance gate"), MeatActor->HasMovedAwayForStorageReturn(200.0f));

	MeatActor->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FARShopBowlFillOrderContractTest,
	"AlienRamen.Shop.Interactables.BowlFillOrderContract",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FARShopBowlFillOrderContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* TestWorld = ResolveAutomationWorld_Interactables();
	if (!TestNotNull(TEXT("Automation test world available"), TestWorld))
	{
		return false;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AARRamenBowlActor* BowlActor = TestWorld->SpawnActor<AARRamenBowlActor>(
		AARRamenBowlActor::StaticClass(),
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParams);
	if (!TestNotNull(TEXT("Spawned ramen bowl actor"), BowlActor))
	{
		return false;
	}

	TestFalse(TEXT("Bowl rejects out-of-order first fill"), BowlActor->TryApplyFillFromStation(EARRamenStationType::Broth, EARAffinityColor::Red, FGameplayTag()));
	TestTrue(TEXT("Bowl accepts noodles as first fill"), BowlActor->TryApplyFillFromStation(EARRamenStationType::Noodles, EARAffinityColor::Red, FGameplayTag()));
	TestTrue(TEXT("Bowl accepts broth as second fill"), BowlActor->TryApplyFillFromStation(EARRamenStationType::Broth, EARAffinityColor::Blue, FGameplayTag()));
	TestTrue(TEXT("Bowl accepts toppings as third fill"), BowlActor->TryApplyFillFromStation(EARRamenStationType::Toppings, EARAffinityColor::White, FGameplayTag()));
	TestTrue(TEXT("Bowl reports completion after third fill"), BowlActor->IsComplete());
	TestFalse(TEXT("Completed bowl rejects additional fills"), BowlActor->TryApplyFillFromStation(EARRamenStationType::Toppings, EARAffinityColor::White, FGameplayTag()));

	BowlActor->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FARShopStationDefaultUpgradeContractTest,
	"AlienRamen.Shop.Interactables.StationDefaultUpgradeContract",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FARShopStationDefaultUpgradeContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* TestWorld = ResolveAutomationWorld_Interactables();
	if (!TestNotNull(TEXT("Automation test world available"), TestWorld))
	{
		return false;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AARShopStationActor* StationActor = TestWorld->SpawnActor<AARShopStationActor>(
		AARShopStationActor::StaticClass(),
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParams);
	if (!TestNotNull(TEXT("Spawned ramen station actor"), StationActor))
	{
		return false;
	}

	TestTrue(TEXT("Stations with no upgrade tags are treated as upgraded"), StationActor->IsStationUpgraded());

	StationActor->Destroy();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
