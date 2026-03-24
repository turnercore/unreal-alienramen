#include "Misc/AutomationTest.h"

#include "ARPlayerTypes.h"
#include "ARTestGameplayTagHelpers.h"
#include "GameplayTagContainer.h"
#include "ParleyDialogueSettings.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FParley_CharacterOwnershipTagContractTest,
	"AlienRamen.Parley.CharacterOwnership.TagContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FParley_CharacterOwnershipTagContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FGameplayTag BrotherTag = ARPlayer::GetBrotherShopCharacterTag();
	const FGameplayTag SisterTag = ARPlayer::GetSisterShopCharacterTag();

	TestTrue(TEXT("Brother character tag should resolve"), BrotherTag.IsValid());
	TestTrue(TEXT("Sister character tag should resolve"), SisterTag.IsValid());
	TestTrue(TEXT("Brother Parley speaker tag should remain authored"), ARPlayer::GetBrotherParleySpeakerTag().IsValid());
	TestTrue(TEXT("Sister Parley speaker tag should remain authored"), ARPlayer::GetSisterParleySpeakerTag().IsValid());

	TestEqual(
		TEXT("ARPlayer helper should resolve Brother choice from canonical character tag"),
		ARPlayer::GetCharacterChoiceForTag(BrotherTag),
		EARCharacterChoice::Brother);
	TestEqual(
		TEXT("ARPlayer helper should resolve Sister choice from canonical character tag"),
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
	TestEqual(
		TEXT("Speaker offer cycle policy defaults to Unlimited"),
		Settings->DefaultSpeakerOfferCyclePolicy,
		EParleySpeakerOfferCyclePolicy::Unlimited);
	TestEqual(TEXT("Speaker offer cycle default limit count is 1"), Settings->DefaultSpeakerOfferCycleLimitCount, 1);

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
		const FGameplayTag Tag = ARTestGameplayTags::RequestTagNoCrash(TagName);
		TestTrue(
			*FString::Printf(TEXT("Required Parley tag should exist: %s"), TagName),
			Tag.IsValid());
	}

	return true;
}
