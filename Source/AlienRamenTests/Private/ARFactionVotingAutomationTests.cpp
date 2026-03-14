#include "Misc/AutomationTest.h"

#include "ARFactionVotingSettings.h"
#include "ARFactionVotingTypes.h"
#include "GameplayTagContainer.h"

namespace
{
	static FGameplayTag RequestTagNoCrash(const TCHAR* TagName)
	{
		return FGameplayTag::RequestGameplayTag(FName(TagName), false);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FARFactionVoting_SettingsContractTest,
	"AlienRamen.FactionVoting.Settings.Contract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FARFactionVoting_SettingsContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const UARFactionVotingSettings* Settings = GetDefault<UARFactionVotingSettings>();
	TestNotNull(TEXT("Faction voting settings should be available"), Settings);
	if (!Settings)
	{
		return false;
	}

	TestTrue(TEXT("MinCandidateCount should be >= 1"), Settings->MinCandidateCount >= 1);
	TestTrue(TEXT("MaxCandidateCount should be >= MinCandidateCount"), Settings->MaxCandidateCount >= Settings->MinCandidateCount);
	TestTrue(TEXT("CloutPerAdditionalCandidate should be >= 1"), Settings->CloutPerAdditionalCandidate >= 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FARFactionVoting_RequiredTagsPresentTest,
	"AlienRamen.FactionVoting.Tags.RequiredTagsPresent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FARFactionVoting_RequiredTagsPresentTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	static const TCHAR* RequiredTags[] =
	{
		TEXT("Faction.Identity"),
		TEXT("Faction.Effect"),
		TEXT("Player.Slot.P1"),
		TEXT("Player.Slot.P2")
	};

	for (const TCHAR* TagName : RequiredTags)
	{
		const FGameplayTag Tag = RequestTagNoCrash(TagName);
		TestTrue(
			*FString::Printf(TEXT("Required faction voting tag should exist: %s"), TagName),
			Tag.IsValid());
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FARFactionVoting_RowDefaultsTest,
	"AlienRamen.FactionVoting.Types.RowDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FARFactionVoting_RowDefaultsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FARFactionVotingDefinitionRow RowDefaults;
	TestEqual(TEXT("MaxAllowedClout default should be -1 (unbounded)"), RowDefaults.MaxAllowedClout, -1);
	TestTrue(TEXT("Voting row defaults should be enabled"), RowDefaults.bEnabled);
	TestTrue(TEXT("Voting row min clout default should be non-negative"), RowDefaults.MinRequiredClout >= 0);
	return true;
}
