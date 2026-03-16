#include "Misc/AutomationTest.h"

#include "ARPlayerTypes.h"
#include "GameplayTagContainer.h"
#include "ParleyDialogueSettings.h"
#include "ParleyPlayerSlotHelpers.h"

namespace
{
	static FGameplayTag RequestTagNoCrash(const TCHAR* TagName)
	{
		return FGameplayTag::RequestGameplayTag(FName(TagName), false);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FParley_PlayerSlotTagRoundTripTest,
	"AlienRamen.Parley.PlayerSlot.RoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FParley_PlayerSlotTagRoundTripTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FGameplayTag P1Tag = ParleyPlayerSlot::SlotToTag(EParleyPlayerSlot::P1);
	const FGameplayTag P2Tag = ParleyPlayerSlot::SlotToTag(EParleyPlayerSlot::P2);

	TestTrue(TEXT("P1 slot tag should resolve"), P1Tag.IsValid());
	TestTrue(TEXT("P2 slot tag should resolve"), P2Tag.IsValid());
	TestEqual(TEXT("P1 slot tag should map back to P1"), ParleyPlayerSlot::TagToSlot(P1Tag), EParleyPlayerSlot::P1);
	TestEqual(TEXT("P2 slot tag should map back to P2"), ParleyPlayerSlot::TagToSlot(P2Tag), EParleyPlayerSlot::P2);

	TestEqual(
		TEXT("ARPlayer helper should agree on P1 slot mapping"),
		ARPlayer::GetPlayerSlotForTag(P1Tag),
		EARPlayerSlot::P1);
	TestEqual(
		TEXT("ARPlayer helper should agree on P2 slot mapping"),
		ARPlayer::GetPlayerSlotForTag(P2Tag),
		EARPlayerSlot::P2);

	TestEqual(
		TEXT("Invalid slot tag should map to Unknown"),
		ParleyPlayerSlot::TagToSlot(FGameplayTag()),
		EParleyPlayerSlot::Unknown);
	TestFalse(
		TEXT("Invalid slot tag should fail helper validity check"),
		ParleyPlayerSlot::IsValidSlotTag(FGameplayTag()));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FParley_DialogueSettingsSlotTagContractTest,
	"AlienRamen.Parley.Settings.SlotTagContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FParley_DialogueSettingsSlotTagContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const UParleyDialogueSettings* Settings = GetDefault<UParleyDialogueSettings>();
	TestNotNull(TEXT("Parley dialogue settings should be available"), Settings);
	if (!Settings)
	{
		return false;
	}

	TestTrue(TEXT("Parley slot tag list should contain P1 + P2"), Settings->PlayerSlotTags.Num() >= 2);
	if (Settings->PlayerSlotTags.Num() < 2)
	{
		return false;
	}

	const FGameplayTag ConfiguredP1Tag = Settings->PlayerSlotTags[0];
	const FGameplayTag ConfiguredP2Tag = Settings->PlayerSlotTags[1];

	TestTrue(TEXT("Configured P1 slot tag should be valid"), ConfiguredP1Tag.IsValid());
	TestTrue(TEXT("Configured P2 slot tag should be valid"), ConfiguredP2Tag.IsValid());
	TestEqual(TEXT("Configured P1 slot tag should map to P1"), ParleyPlayerSlot::TagToSlot(ConfiguredP1Tag), EParleyPlayerSlot::P1);
	TestEqual(TEXT("Configured P2 slot tag should map to P2"), ParleyPlayerSlot::TagToSlot(ConfiguredP2Tag), EParleyPlayerSlot::P2);
	TestEqual(TEXT("Dialogue audio mode defaults to NativeAudio"), Settings->DialogueAudioMode, EParleyDialogueAudioMode::NativeAudio);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FParley_RequiredTagsPresentTest,
	"AlienRamen.Parley.Tags.RequiredTagsPresent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FParley_RequiredTagsPresentTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	static const TCHAR* RequiredTags[] =
	{
		TEXT("Player.Slot"),
		TEXT("Player.Slot.P1"),
		TEXT("Player.Slot.P2"),
		TEXT("Parley.Speaker"),
		TEXT("Parley.Speaker.Requester"),
		TEXT("Parley.Speaker.Owner"),
		TEXT("Parley.Speaker.Brother"),
		TEXT("Parley.Speaker.Sister"),
		TEXT("Parley.Conversations"),
		TEXT("Parley.Emotion"),
		TEXT("Parley.Emotion.Busy"),
		TEXT("Parley.Emotion.Default"),
		TEXT("Parley.AudioCue"),
		TEXT("Parley.Factions"),
		TEXT("Parley.Factions.Effect")
	};

	for (const TCHAR* TagName : RequiredTags)
	{
		const FGameplayTag Tag = RequestTagNoCrash(TagName);
		TestTrue(
			*FString::Printf(TEXT("Required Parley tag should exist: %s"), TagName),
			Tag.IsValid());
	}

	return true;
}
