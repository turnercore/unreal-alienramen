#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "ARCarryItemBase.h"
#include "ARScrapyardExitZoneActor.h"
#include "ARScrapyardGameState.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"

namespace
{
	UWorld* ResolveAutomationWorld_ScrapyardDeterminism()
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

	static void SetFallbackCost(AARCarryItemBase* ItemActor, int32 FallbackCost)
	{
		if (!ItemActor)
		{
			return;
		}

		FIntProperty* FallbackCostProperty = FindFProperty<FIntProperty>(AARCarryItemBase::StaticClass(), TEXT("FallbackScrapCost"));
		if (FallbackCostProperty)
		{
			FallbackCostProperty->SetPropertyValue_InContainer(ItemActor, FMath::Max(0, FallbackCost));
		}
	}

	static bool BuildDeterminismScenario(
		UWorld* TestWorld,
		int32 Seed,
		AARScrapyardGameState*& OutGameState,
		AARScrapyardExitZoneActor*& OutExitZone,
		TArray<AARCarryItemBase*>& OutItems)
	{
		OutGameState = nullptr;
		OutExitZone = nullptr;
		OutItems.Reset();

		if (!TestWorld)
		{
			return false;
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		OutGameState = TestWorld->SpawnActor<AARScrapyardGameState>(
			AARScrapyardGameState::StaticClass(),
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			SpawnParams);
		OutExitZone = TestWorld->SpawnActor<AARScrapyardExitZoneActor>(
			AARScrapyardExitZoneActor::StaticClass(),
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			SpawnParams);
		if (!OutGameState || !OutExitZone)
		{
			return false;
		}

		const int32 ItemCosts[3] = {9, 6, 6};
		for (const int32 Cost : ItemCosts)
		{
			AARCarryItemBase* ItemActor = TestWorld->SpawnActor<AARCarryItemBase>(
				AARCarryItemBase::StaticClass(),
				FVector::ZeroVector,
				FRotator::ZeroRotator,
				SpawnParams);
			if (!ItemActor)
			{
				return false;
			}

			SetFallbackCost(ItemActor, Cost);
			OutItems.Add(ItemActor);
		}

		OutGameState->StartScrapyardRun(999.0f, Seed);
		OutGameState->SetScrapFromSave(12);
		for (AARCarryItemBase* ItemActor : OutItems)
		{
			int32 ReservedCost = 0;
			if (!OutGameState->ReserveScrapForItem(ItemActor, ReservedCost))
			{
				return false;
			}
		}

		auto& DepositedItems =
			const_cast<TArray<TObjectPtr<AARCarryItemBase>>&>(OutExitZone->GetDepositedItemsRef());
		for (AARCarryItemBase* ItemActor : OutItems)
		{
			DepositedItems.Add(ItemActor);
		}
		OutGameState->RegisterExitZone(OutExitZone);
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FARScrapyardDeterministicTrimTest,
	"AlienRamen.Scrapyard.Economy.DeterministicTrim",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FARScrapyardDeterministicTrimTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* TestWorld = ResolveAutomationWorld_ScrapyardDeterminism();
	if (!TestNotNull(TEXT("Automation test world available"), TestWorld))
	{
		return false;
	}

	int32 KeptA = -1;
	int32 TrimmedA = -1;
	int32 KeptB = -1;
	int32 TrimmedB = -1;

	for (int32 Iteration = 0; Iteration < 2; ++Iteration)
	{
		AARScrapyardGameState* GameState = nullptr;
		AARScrapyardExitZoneActor* ExitZone = nullptr;
		TArray<AARCarryItemBase*> Items;
		if (!BuildDeterminismScenario(TestWorld, 777, GameState, ExitZone, Items))
		{
			AddError(TEXT("Failed to build deterministic trim scenario."));
			return false;
		}

		const bool bFinalized = GameState->FinalizeScrapyardRun();
		TestTrue(TEXT("Scrapyard finalization succeeded"), bFinalized);
		const FARScrapyardExtractionSummary Summary = GameState->GetExtractionSummary();

		if (Iteration == 0)
		{
			KeptA = Summary.KeptItemCount;
			TrimmedA = Summary.TrimmedItemCount;
		}
		else
		{
			KeptB = Summary.KeptItemCount;
			TrimmedB = Summary.TrimmedItemCount;
		}

		TestEqual(TEXT("Finalization always considers all candidates"), Summary.KeptItemCount + Summary.TrimmedItemCount, 3);
		TestEqual(TEXT("Scrap is zeroed at run end"), GameState->GetScrap(), 0);

		if (ExitZone)
		{
			ExitZone->Destroy();
		}
		if (GameState)
		{
			GameState->Destroy();
		}
		for (AARCarryItemBase* ItemActor : Items)
		{
			if (IsValid(ItemActor))
			{
				ItemActor->Destroy();
			}
		}
	}

	TestEqual(TEXT("Kept item count is deterministic for fixed seed/state"), KeptA, KeptB);
	TestEqual(TEXT("Trimmed item count is deterministic for fixed seed/state"), TrimmedA, TrimmedB);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
