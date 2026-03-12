#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "ARRunBuffTypes.h"
#include "ARSaveGame.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FARSaveGameRunBuffSanitizeTest,
	"AlienRamen.Save.RunBuff.Sanitize",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FARSaveGameRunBuffSanitizeTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UARSaveGame* SaveGame = NewObject<UARSaveGame>();
	if (!TestNotNull(TEXT("Created save game object"), SaveGame))
	{
		return false;
	}

	const FGameplayTag ValidItemTag = FGameplayTag::RequestGameplayTag(TEXT("Scrapyard.Item"), false);
	const FGameplayTag ValidGrantedTag = FGameplayTag::RequestGameplayTag(TEXT("Unlock.Hat.Vac"), false);

	FARRunBuffItemStack InvalidStored;
	InvalidStored.Count = -4;
	SaveGame->StoredEnergyDrinkStacks.Add(InvalidStored);

	FARRunBuffItemStack StoredA;
	StoredA.ItemTag = ValidItemTag;
	StoredA.Count = 2;
	SaveGame->StoredEnergyDrinkStacks.Add(StoredA);

	FARRunBuffItemStack StoredB;
	StoredB.ItemTag = ValidItemTag;
	StoredB.Count = 3;
	SaveGame->StoredEnergyDrinkStacks.Add(StoredB);

	FARRunBuffItemStack InvalidQueued;
	InvalidQueued.Count = 0;
	SaveGame->QueuedEnergyDrinkStacks.Add(InvalidQueued);

	FARRunBuffItemStack ValidQueued;
	ValidQueued.ItemTag = ValidItemTag;
	ValidQueued.Count = 1;
	SaveGame->QueuedEnergyDrinkStacks.Add(ValidQueued);

	FARRunBuffActivePayload InvalidPayload;
	InvalidPayload.AppliedCount = 2;
	SaveGame->ActiveRunBuffPayloads.Add(InvalidPayload);

	FARRunBuffActivePayload ValidPayload;
	ValidPayload.ItemTag = ValidItemTag;
	ValidPayload.AppliedCount = 0;
	ValidPayload.GameplayEffects.Add(nullptr);
	ValidPayload.GrantedTags.AddTag(ValidGrantedTag);
	ValidPayload.GrantedTags.AddTag(FGameplayTag());
	SaveGame->ActiveRunBuffPayloads.Add(ValidPayload);

	SaveGame->ActiveRunBuffCycleId = -7;

	TArray<FString> Warnings;
	const int32 ClampedFields = SaveGame->ValidateAndSanitize(&Warnings);
	TestTrue(TEXT("Sanitize reports clamped fields"), ClampedFields > 0);
	TestTrue(TEXT("Warnings emitted"), Warnings.Num() > 0);

	TestEqual(TEXT("Stored stacks normalized to one entry"), SaveGame->StoredEnergyDrinkStacks.Num(), 1);
	TestEqual(TEXT("Stored stack merged count"), SaveGame->StoredEnergyDrinkStacks[0].Count, 5);
	TestEqual(TEXT("Queued stacks normalized to one entry"), SaveGame->QueuedEnergyDrinkStacks.Num(), 1);
	TestEqual(TEXT("Queued stack count retained"), SaveGame->QueuedEnergyDrinkStacks[0].Count, 1);

	TestEqual(TEXT("Invalid active payload removed"), SaveGame->ActiveRunBuffPayloads.Num(), 1);
	TestEqual(TEXT("AppliedCount clamped to 1"), SaveGame->ActiveRunBuffPayloads[0].AppliedCount, 1);
	TestEqual(TEXT("Null gameplay effects stripped"), SaveGame->ActiveRunBuffPayloads[0].GameplayEffects.Num(), 0);
	TestTrue(TEXT("Valid granted tag preserved"), SaveGame->ActiveRunBuffPayloads[0].GrantedTags.HasTagExact(ValidGrantedTag));
	TestEqual(TEXT("Active cycle clamped non-negative"), SaveGame->ActiveRunBuffCycleId, 0);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

