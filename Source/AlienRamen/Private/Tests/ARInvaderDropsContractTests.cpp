#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AREnemyAttributeSet.h"
#include "ARAttributeSetPlayer.h"
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

	TestEqual(TEXT("Default enemy drop chance starts at 0.5"), Settings->DefaultEnemyDropChance, 0.5f);
	TestEqual(TEXT("Default scrap drop chance starts at 0.5"), Settings->DefaultEnemyScrapDropChance, 0.5f);
	TestEqual(TEXT("Default meat drop chance starts at 0.2"), Settings->DefaultEnemyMeatDropChance, 0.2f);
	TestEqual(TEXT("Default drop variance fraction starts at 0.25"), Settings->DropAmountVarianceFraction, 0.25f);
	TestEqual(TEXT("Scrap drop variance fraction starts at 0.25"), Settings->ScrapDropAmountVarianceFraction, 0.25f);
	TestEqual(TEXT("Meat drop variance fraction starts at 0.25"), Settings->MeatDropAmountVarianceFraction, 0.25f);
	TestTrue(
		TEXT("Drop initial speed max is >= min"),
		Settings->DropInitialLinearSpeedMax >= Settings->DropInitialLinearSpeedMin);
	TestEqual(TEXT("Default scrap drop stacks keep authored denomination blueprints"), Settings->ScrapDropStacks.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FARInvaderDropEnemyAttributeClampTest,
	"AlienRamen.Invader.Drops.EnemyAttributeClamps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FARInvaderDropEnemyAttributeClampTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UAREnemyAttributeSet* AttrSet = NewObject<UAREnemyAttributeSet>();
	if (!TestNotNull(TEXT("Enemy attribute set created"), AttrSet))
	{
		return false;
	}

	float Candidate = -1.0f;
	AttrSet->PreAttributeChange(UAREnemyAttributeSet::GetDropChanceAttribute(), Candidate);
	TestEqual(TEXT("DropChance clamps to 0"), Candidate, 0.0f);

	Candidate = 2.0f;
	AttrSet->PreAttributeChange(UAREnemyAttributeSet::GetDropChanceAttribute(), Candidate);
	TestEqual(TEXT("DropChance clamps to 1"), Candidate, 1.0f);

	Candidate = -5.0f;
	AttrSet->PreAttributeChange(UAREnemyAttributeSet::GetDropAmountAttribute(), Candidate);
	TestEqual(TEXT("DropAmount clamps to non-negative"), Candidate, 0.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FARInvaderDropPlayerAttributeClampTest,
	"AlienRamen.Invader.Drops.PlayerAttributeClamps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FARInvaderDropPlayerAttributeClampTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UARAttributeSetPlayer* AttrSet = NewObject<UARAttributeSetPlayer>();
	if (!TestNotNull(TEXT("Player attribute set created"), AttrSet))
	{
		return false;
	}

	TestEqual(TEXT("Strength defaults to 10"), AttrSet->GetStrength(), 10.0f);

	float Candidate = -9.0f;
	AttrSet->PreAttributeChange(UARAttributeSetPlayer::GetStrengthAttribute(), Candidate);
	TestEqual(TEXT("Strength clamps to non-negative"), Candidate, 0.0f);

	Candidate = -2.0f;
	AttrSet->PreAttributeChange(UARAttributeSetPlayer::GetPickupRadiusAttribute(), Candidate);
	TestEqual(TEXT("PickupRadius clamps to non-negative"), Candidate, 0.0f);

	Candidate = -3.0f;
	AttrSet->PreAttributeChange(UARAttributeSetPlayer::GetMeatDropMultiplierAttribute(), Candidate);
	TestEqual(TEXT("MeatDropMultiplier clamps to non-negative"), Candidate, 0.0f);

	Candidate = -7.0f;
	AttrSet->PreAttributeChange(UARAttributeSetPlayer::GetScrapDropMultiplierAttribute(), Candidate);
	TestEqual(TEXT("ScrapDropMultiplier clamps to non-negative"), Candidate, 0.0f);

	Candidate = 2.5f;
	AttrSet->PreAttributeChange(UARAttributeSetPlayer::GetCritChanceAttribute(), Candidate);
	TestEqual(TEXT("CritChance clamps to 1"), Candidate, 1.0f);

	Candidate = -1.0f;
	AttrSet->PreAttributeChange(UARAttributeSetPlayer::GetCritChanceAttribute(), Candidate);
	TestEqual(TEXT("CritChance clamps to 0"), Candidate, 0.0f);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
