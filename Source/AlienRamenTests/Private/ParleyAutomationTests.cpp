#include "Misc/AutomationTest.h"

#include "ARPlayerTypes.h"
#include "GameplayTagContainer.h"
#include "ParleyDialogueSettings.h"

namespace
{
	static FGameplayTag RequestTagNoCrash(const TCHAR* TagName)
	{
		return FGameplayTag::RequestGameplayTag(FName(TagName), false);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FParley_CharacterOwnershipTagContractTest,
	"AlienRamen.Parley.CharacterOwnership.TagContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FParley_CharacterOwnershipTagContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FGameplayTag BrotherTag = RequestTagNoCrash(TEXT("Parley.Speaker.Brother"));
	const FGameplayTag SisterTag = RequestTagNoCrash(TEXT("Parley.Speaker.Sister"));

	TestTrue(TEXT("Brother character tag should resolve"), BrotherTag.IsValid());
	TestTrue(TEXT("Sister character tag should resolve"), SisterTag.IsValid());

	TestEqual(
		TEXT("ARPlayer helper should resolve Brother choice"),
		ARPlayer::GetCharacterChoiceForTag(BrotherTag),
		EARCharacterChoice::Brother);
	TestEqual(
		TEXT("ARPlayer helper should resolve Sister choice"),
		ARPlayer::GetCharacterChoiceForTag(SisterTag),
		EARCharacterChoice::Sister);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FParley_DialogueSettingsContractTest,
	"AlienRamen.Parley.Settings.DialogueContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FParley_DialogueSettingsContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const UParleyDialogueSettings* Settings = GetDefault<UParleyDialogueSettings>();
	TestNotNull(TEXT("Parley dialogue settings should be available"), Settings);
	if (!Settings)
	{
		return false;
	}

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
