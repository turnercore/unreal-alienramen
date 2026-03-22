#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "ARItemDefinitionSubsystem.h"
#include "ARShopGameMode.h"
#include "ARShopGameState.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"

namespace
{
	UWorld* ResolveAutomationWorld_ShopEconomy()
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
	FARShopEconomyCombinedBowlValueTest,
	"AlienRamen.Shop.Economy.CombinedBowlValue",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FARShopEconomyCombinedBowlValueTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* TestWorld = ResolveAutomationWorld_ShopEconomy();
	if (!TestNotNull(TEXT("Automation test world available"), TestWorld))
	{
		return false;
	}

	UGameInstance* GameInstance = TestWorld->GetGameInstance();
	if (!TestNotNull(TEXT("Game instance available"), GameInstance))
	{
		return false;
	}

	UARItemDefinitionSubsystem* ItemDefinitions = GameInstance->GetSubsystem<UARItemDefinitionSubsystem>();
	if (!TestNotNull(TEXT("Item definition subsystem available"), ItemDefinitions))
	{
		return false;
	}

	FARMeatDefinitionRow NoodlesMeatDef;
	FARMeatDefinitionRow BrothMeatDef;
	FARMeatDefinitionRow ToppingsMeatDef;
	if (!TestTrue(TEXT("Resolved first meat definition"), ItemDefinitions->ResolveFirstMeatDefinition(NoodlesMeatDef))
		|| !TestTrue(TEXT("Resolved second meat definition"), ItemDefinitions->ResolveFirstMeatDefinition(BrothMeatDef))
		|| !TestTrue(TEXT("Resolved third meat definition"), ItemDefinitions->ResolveFirstMeatDefinition(ToppingsMeatDef)))
	{
		return false;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AARShopGameState* ShopGameState = TestWorld->SpawnActor<AARShopGameState>(
		AARShopGameState::StaticClass(),
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParams);
	AARShopGameMode* ShopGameMode = TestWorld->SpawnActor<AARShopGameMode>(
		AARShopGameMode::StaticClass(),
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParams);

	if (!TestNotNull(TEXT("Spawned shop game state"), ShopGameState)
		|| !TestNotNull(TEXT("Spawned shop game mode"), ShopGameMode))
	{
		if (ShopGameMode)
		{
			ShopGameMode->Destroy();
		}
		if (ShopGameState)
		{
			ShopGameState->Destroy();
		}
		return false;
	}

	TestWorld->SetGameState(ShopGameState);
	ShopGameState->SetBaseBowlPayout(17);

	FARRamenBowlSpec BowlSpec;
	BowlSpec.Noodles.SlotType = EARRamenStationType::Noodles;
	BowlSpec.Noodles.Color = EARAffinityColor::None;
	BowlSpec.Noodles.MeatTag = NoodlesMeatDef.MeatTag;
	BowlSpec.Noodles.QualityTier = EARVendingQualityTier::Low;

	BowlSpec.Broth.SlotType = EARRamenStationType::Broth;
	BowlSpec.Broth.Color = EARAffinityColor::None;
	BowlSpec.Broth.MeatTag = BrothMeatDef.MeatTag;
	BowlSpec.Broth.QualityTier = EARVendingQualityTier::High;

	BowlSpec.Toppings.SlotType = EARRamenStationType::Toppings;
	BowlSpec.Toppings.Color = EARAffinityColor::None;
	BowlSpec.Toppings.MeatTag = ToppingsMeatDef.MeatTag;
	BowlSpec.Toppings.QualityTier = EARVendingQualityTier::Premium;

	const int32 NoodlesBaseValue = ItemDefinitions->ResolveBowlSlotItemValue(BowlSpec.Noodles);
	const int32 BrothBaseValue = ItemDefinitions->ResolveBowlSlotItemValue(BowlSpec.Broth);
	const int32 ToppingsBaseValue = ItemDefinitions->ResolveBowlSlotItemValue(BowlSpec.Toppings);
	TestTrue(TEXT("Noodle slot has a positive base value"), NoodlesBaseValue > 0);
	TestTrue(TEXT("Broth slot has a positive base value"), BrothBaseValue > 0);
	TestTrue(TEXT("Toppings slot has a positive base value"), ToppingsBaseValue > 0);

	const FFloatProperty* LowMultiplierProperty = FindFProperty<FFloatProperty>(AARShopGameMode::StaticClass(), TEXT("ItemQualityLowMultiplier"));
	const FFloatProperty* HighMultiplierProperty = FindFProperty<FFloatProperty>(AARShopGameMode::StaticClass(), TEXT("ItemQualityHighMultiplier"));
	const FFloatProperty* PremiumMultiplierProperty = FindFProperty<FFloatProperty>(AARShopGameMode::StaticClass(), TEXT("ItemQualityPremiumMultiplier"));
	if (!TestNotNull(TEXT("Low quality multiplier property"), LowMultiplierProperty)
		|| !TestNotNull(TEXT("High quality multiplier property"), HighMultiplierProperty)
		|| !TestNotNull(TEXT("Premium quality multiplier property"), PremiumMultiplierProperty))
	{
		ShopGameMode->Destroy();
		ShopGameState->Destroy();
		return false;
	}

	const float LowMultiplier = LowMultiplierProperty->GetPropertyValue_InContainer(ShopGameMode);
	const float HighMultiplier = HighMultiplierProperty->GetPropertyValue_InContainer(ShopGameMode);
	const float PremiumMultiplier = PremiumMultiplierProperty->GetPropertyValue_InContainer(ShopGameMode);
	const int32 ExpectedCombinedMeatValue =
		FMath::Max(0, FMath::RoundToInt(static_cast<float>(NoodlesBaseValue) * LowMultiplier))
		+ FMath::Max(0, FMath::RoundToInt(static_cast<float>(BrothBaseValue) * HighMultiplier))
		+ FMath::Max(0, FMath::RoundToInt(static_cast<float>(ToppingsBaseValue) * PremiumMultiplier));

	float OutAppliedTipMultiplier = -1.0f;
	int32 OutCombinedMeatValue = -1;
	int32 OutBasePayout = -1;
	int32 OutTipPayout = -1;
	const int32 Payout = ShopGameMode->CalculateServePayout(
		BowlSpec,
		static_cast<EARRamenTasteReaction>(255),
		OutAppliedTipMultiplier,
		OutCombinedMeatValue,
		OutBasePayout,
		OutTipPayout);

	TestEqual(TEXT("Combined meat value sums per-slot quality-adjusted values"), OutCombinedMeatValue, ExpectedCombinedMeatValue);
	TestEqual(TEXT("Base payout comes from the shop game state"), OutBasePayout, 17);
	TestEqual(TEXT("Invalid reaction skips tip payout"), OutTipPayout, 0);
	TestEqual(TEXT("Invalid reaction leaves applied tip multiplier at zero"), OutAppliedTipMultiplier, 0.0f);
	TestEqual(TEXT("Total payout falls back to base payout when no tip applies"), Payout, 17);

	ShopGameMode->Destroy();
	ShopGameState->Destroy();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
