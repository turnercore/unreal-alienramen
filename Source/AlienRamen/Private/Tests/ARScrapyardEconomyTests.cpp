#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "ARScrapyardCarryItemBase.h"
#include "ARScrapyardGameState.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"

namespace
{
	UWorld* ResolveAutomationWorld_Scrapyard()
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
	FARScrapyardReserveRefundScrapTest,
	"AlienRamen.Scrapyard.Economy.ReserveRefund",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FARScrapyardReserveRefundScrapTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* TestWorld = ResolveAutomationWorld_Scrapyard();
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
	AARScrapyardCarryItemBase* ItemActor = TestWorld->SpawnActor<AARScrapyardCarryItemBase>(
		AARScrapyardCarryItemBase::StaticClass(),
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParams);

	if (!TestNotNull(TEXT("Spawned scrapyard game state"), ScrapyardGameState)
		|| !TestNotNull(TEXT("Spawned scrapyard carry item"), ItemActor))
	{
		if (ItemActor)
		{
			ItemActor->Destroy();
		}
		if (ScrapyardGameState)
		{
			ScrapyardGameState->Destroy();
		}
		return false;
	}

	FIntProperty* FallbackCostProperty = FindFProperty<FIntProperty>(AARScrapyardCarryItemBase::StaticClass(), TEXT("FallbackScrapCost"));
	if (!TestNotNull(TEXT("FallbackScrapCost property found"), FallbackCostProperty))
	{
		ItemActor->Destroy();
		ScrapyardGameState->Destroy();
		return false;
	}
	FallbackCostProperty->SetPropertyValue_InContainer(ItemActor, 12);

	ScrapyardGameState->SetScrapFromSave(0);

	int32 ReservedCost = 0;
	TestTrue(TEXT("Reserve succeeds for carry item"), ScrapyardGameState->ReserveScrapForItem(ItemActor, ReservedCost));
	TestEqual(TEXT("Reserved cost uses fallback value"), ReservedCost, 12);
	TestEqual(TEXT("Scrap can go negative in scrapyard reserve"), ScrapyardGameState->GetScrap(), -12);
	TestEqual(TEXT("Extraction summary tracks reserved cost"), ScrapyardGameState->GetExtractionSummary().ReservedCostTotal, 12);
	TestEqual(TEXT("Extraction summary tracks reserved item count"), ScrapyardGameState->GetExtractionSummary().ReservedItemCount, 1);

	int32 RefundCost = 0;
	TestTrue(TEXT("Refund succeeds for previously reserved item"), ScrapyardGameState->RefundScrapForItem(ItemActor, RefundCost));
	TestEqual(TEXT("Refund cost equals reserved cost"), RefundCost, 12);
	TestEqual(TEXT("Scrap refund returns balance to zero"), ScrapyardGameState->GetScrap(), 0);
	TestEqual(TEXT("Reserved cost clears after refund"), ScrapyardGameState->GetExtractionSummary().ReservedCostTotal, 0);
	TestEqual(TEXT("Reserved item count clears after refund"), ScrapyardGameState->GetExtractionSummary().ReservedItemCount, 0);

	ItemActor->Destroy();
	ScrapyardGameState->Destroy();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
