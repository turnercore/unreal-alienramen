#include "Misc/AutomationTest.h"

#include "EmoResolverSubsystem.h"
#include "EmoSettings.h"
#include "Engine/Texture2D.h"
#include "GameplayTagContainer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEmo_SettingsTagsAreConfiguredTest,
	"AlienRamen.Emo.Settings.TagsConfigured",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEmo_SettingsTagsAreConfiguredTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const UEmoSettings* Settings = GetDefault<UEmoSettings>();
	TestNotNull(TEXT("Emo settings should be available"), Settings);
	if (!Settings)
	{
		return false;
	}

	TestTrue(TEXT("EmotionResolverRootTag should be configured"), Settings->EmotionResolverRootTag.IsValid());
	TestTrue(TEXT("GenericEmotionRootTag should be configured"), Settings->GenericEmotionRootTag.IsValid());
	TestTrue(TEXT("WantsToTalkEmotionTag should be configured"), Settings->WantsToTalkEmotionTag.IsValid());
	TestTrue(TEXT("BusyEmotionTag should be configured"), Settings->BusyEmotionTag.IsValid());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEmo_ResolverRejectsInvalidTagTest,
	"AlienRamen.Emo.Resolver.InvalidTagRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEmo_ResolverRejectsInvalidTagTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	TSoftObjectPtr<UTexture2D> ResolvedIcon;
	FGameplayTag ResolvedTag;
	const bool bResolved = UEmoResolverSubsystem::TryResolveEmotionIconFromConfiguredData(
		FGameplayTag(),
		ResolvedIcon,
		ResolvedTag);

	TestFalse(TEXT("Invalid emotion tag should not resolve"), bResolved);
	TestFalse(TEXT("Resolved tag should remain invalid for invalid requests"), ResolvedTag.IsValid());
	TestTrue(TEXT("Resolved icon should remain null for invalid requests"), ResolvedIcon.IsNull());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEmo_ResolverCanResolveConfiguredEmotionTest,
	"AlienRamen.Emo.Resolver.CanResolveConfiguredEmotion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEmo_ResolverCanResolveConfiguredEmotionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const UEmoSettings* Settings = GetDefault<UEmoSettings>();
	TestNotNull(TEXT("Emo settings should be available"), Settings);
	if (!Settings)
	{
		return false;
	}

	TArray<FGameplayTag> CandidateTags;
	if (Settings->BusyEmotionTag.IsValid())
	{
		CandidateTags.Add(Settings->BusyEmotionTag);
	}
	if (Settings->WantsToTalkEmotionTag.IsValid())
	{
		CandidateTags.Add(Settings->WantsToTalkEmotionTag);
	}

	const FGameplayTag FallbackOkTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Dialogue.Emotion.Ok")), false);
	const FGameplayTag FallbackLikeTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Dialogue.Emotion.Like")), false);
	if (FallbackOkTag.IsValid())
	{
		CandidateTags.AddUnique(FallbackOkTag);
	}
	if (FallbackLikeTag.IsValid())
	{
		CandidateTags.AddUnique(FallbackLikeTag);
	}

	bool bResolvedAny = false;
	for (const FGameplayTag CandidateTag : CandidateTags)
	{
		TSoftObjectPtr<UTexture2D> ResolvedIcon;
		FGameplayTag ResolvedTag;
		if (UEmoResolverSubsystem::TryResolveEmotionIconFromConfiguredData(CandidateTag, ResolvedIcon, ResolvedTag))
		{
			TestTrue(TEXT("Resolved emotion tag should be valid"), ResolvedTag.IsValid());
			TestFalse(TEXT("Resolved icon should be assigned"), ResolvedIcon.IsNull());
			bResolvedAny = true;
			break;
		}
	}

	TestTrue(
		TEXT("At least one configured emotion tag should resolve to an icon via Emo resolver"),
		bResolvedAny);

	return true;
}
