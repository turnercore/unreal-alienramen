#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "ARAttributeSetCore.h"
#include "ARInvaderDirectorSettings.h"
#include "ARInvaderTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FARInvaderDropRuntimeInitDefaultsTest,
	"AlienRamen.Invader.Drops.RuntimeInitDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FARInvaderDropRuntimeInitDefaultsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FARInvaderEnemyRuntimeInitData Defaults;
	TestEqual(TEXT("Default drop type is None"), Defaults.DropType, EARInvaderDropType::None);
	TestEqual(TEXT("Default drop amount is zero"), Defaults.DropAmount, 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FARInvaderDropDirectorSettingsDefaultsTest,
	"AlienRamen.Invader.Drops.DirectorSettingsDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FARInvaderDropDirectorSettingsDefaultsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const UARInvaderDirectorSettings* Settings = GetDefault<UARInvaderDirectorSettings>();
	if (!TestNotNull(TEXT("Invader director settings exist"), Settings))
	{
		return false;
	}

	TestEqual(TEXT("Legacy default enemy drop chance starts at 0.5"), Settings->DefaultEnemyDropChance, 0.5f);
	TestEqual(TEXT("Default scrap drop chance starts at 0.5"), Settings->DefaultEnemyScrapDropChance, 0.5f);
	TestEqual(TEXT("Default meat drop chance starts at 0.2"), Settings->DefaultEnemyMeatDropChance, 0.2f);
	TestEqual(TEXT("Legacy drop variance fraction starts at 0.25"), Settings->DropAmountVarianceFraction, 0.25f);
	TestEqual(TEXT("Scrap drop variance fraction starts at 0.25"), Settings->ScrapDropAmountVarianceFraction, 0.25f);
	TestEqual(TEXT("Meat drop variance fraction starts at 0.25"), Settings->MeatDropAmountVarianceFraction, 0.25f);
	TestTrue(
		TEXT("Drop initial speed max is >= min"),
		Settings->DropInitialLinearSpeedMax >= Settings->DropInitialLinearSpeedMin);
	TestEqual(TEXT("Default scrap drop stacks start empty"), Settings->ScrapDropStacks.Num(), 0);
	TestEqual(TEXT("Default meat drop stacks start empty"), Settings->MeatDropStacks.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FARInvaderDropAttributeClampTest,
	"AlienRamen.Invader.Drops.AttributeClamps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FARInvaderDropAttributeClampTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UARAttributeSetCore* AttrSet = NewObject<UARAttributeSetCore>();
	if (!TestNotNull(TEXT("Attribute set created"), AttrSet))
	{
		return false;
	}

	float Candidate = -1.0f;
	AttrSet->PreAttributeChange(UARAttributeSetCore::GetDropChanceAttribute(), Candidate);
	TestEqual(TEXT("DropChance clamps to 0"), Candidate, 0.0f);

	Candidate = 2.0f;
	AttrSet->PreAttributeChange(UARAttributeSetCore::GetDropChanceAttribute(), Candidate);
	TestEqual(TEXT("DropChance clamps to 1"), Candidate, 1.0f);

	Candidate = -5.0f;
	AttrSet->PreAttributeChange(UARAttributeSetCore::GetDropAmountAttribute(), Candidate);
	TestEqual(TEXT("DropAmount clamps to non-negative"), Candidate, 0.0f);

	Candidate = -3.0f;
	AttrSet->PreAttributeChange(UARAttributeSetCore::GetMeatDropMultiplierAttribute(), Candidate);
	TestEqual(TEXT("MeatDropMultiplier clamps to non-negative"), Candidate, 0.0f);

	Candidate = -7.0f;
	AttrSet->PreAttributeChange(UARAttributeSetCore::GetScrapDropMultiplierAttribute(), Candidate);
	TestEqual(TEXT("ScrapDropMultiplier clamps to non-negative"), Candidate, 0.0f);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
