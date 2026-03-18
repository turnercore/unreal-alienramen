#include "Misc/AutomationTest.h"

#include "AREmotionViewerTags.h"
#include "ARNPCCharacterBase.h"
#include "ARPlayerStateBase.h"
#include "ARPlayerTypes.h"
#include "EmoComponent.h"
#include "EmoResolverSubsystem.h"
#include "EmoSettings.h"
#include "Engine/Engine.h"
#include "Engine/EngineTypes.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameplayTagContainer.h"
#include "ParleySpeakerComponent.h"
#include "UObject/UnrealType.h"

namespace
{
	static UWorld* GetAutomationWorld(FAutomationTestBase& Test)
	{
		if (GEngine)
		{
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::Editor)
				{
					if (UWorld* World = Context.World())
					{
						return World;
					}
				}
			}
		}

		Test.AddError(TEXT("Missing PIE/Game automation world."));
		return nullptr;
	}

	static FGameplayTag RequireTag(FAutomationTestBase& Test, const TCHAR* TagName)
	{
		const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(TagName), false);
		if (!Tag.IsValid())
		{
			Test.AddError(FString::Printf(TEXT("Missing gameplay tag '%s'."), TagName));
		}

		return Tag;
	}

	static FGameplayTagContainer MakeTagContainer(std::initializer_list<FGameplayTag> Tags)
	{
		FGameplayTagContainer Container;
		for (const FGameplayTag Tag : Tags)
		{
			if (Tag.IsValid())
			{
				Container.AddTag(Tag);
			}
		}

		return Container;
	}

	template <typename TActor>
	static TActor* SpawnAutomationActor(UWorld* World, FAutomationTestBase& Test, const TCHAR* Label)
	{
		if (!World)
		{
			return nullptr;
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		TActor* Actor = World->SpawnActor<TActor>(TActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
		Test.TestNotNull(Label, Actor);
		return Actor;
	}

	template <typename TComponent>
	static TComponent* AddAutomationComponent(AActor* Owner, const TCHAR* ComponentName)
	{
		if (!Owner)
		{
			return nullptr;
		}

		TComponent* Component = NewObject<TComponent>(Owner, FName(ComponentName));
		Owner->AddInstanceComponent(Component);
		Component->RegisterComponent();
		return Component;
	}

	template <typename TObjectType>
	static bool SetObjectProperty(FAutomationTestBase& Test, UObject* Object, const TCHAR* PropertyName, TObjectType* Value)
	{
		FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(Object ? Object->GetClass() : nullptr, FName(PropertyName));
		const FString PropertyLabel = FString::Printf(TEXT("Property %s"), PropertyName);
		if (!Test.TestNotNull(*PropertyLabel, Property))
		{
			return false;
		}

		Property->SetObjectPropertyValue_InContainer(Object, Value);
		return true;
	}

	static bool SetGameplayTagProperty(FAutomationTestBase& Test, UObject* Object, const TCHAR* PropertyName, const FGameplayTag& Value)
	{
		FStructProperty* Property = FindFProperty<FStructProperty>(Object ? Object->GetClass() : nullptr, FName(PropertyName));
		const FString PropertyLabel = FString::Printf(TEXT("Property %s"), PropertyName);
		if (!Test.TestNotNull(*PropertyLabel, Property))
		{
			return false;
		}

		FGameplayTag* TargetValue = Property->ContainerPtrToValuePtr<FGameplayTag>(Object);
		const FString PropertyValueLabel = FString::Printf(TEXT("Property value %s"), PropertyName);
		if (!Test.TestNotNull(*PropertyValueLabel, TargetValue))
		{
			return false;
		}

		*TargetValue = Value;
		return true;
	}

	template <typename TParams>
	static bool InvokeUFunction(FAutomationTestBase& Test, UObject* Object, const TCHAR* FunctionName, TParams& Params)
	{
		UFunction* Function = Object ? Object->FindFunction(FName(FunctionName)) : nullptr;
		const FString FunctionLabel = FString::Printf(TEXT("Function %s"), FunctionName);
		if (!Test.TestNotNull(*FunctionLabel, Function))
		{
			return false;
		}

		Object->ProcessEvent(Function, &Params);
		return true;
	}

}

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

	const FGameplayTag FallbackOkTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Parley.Emotion.Ok")), false);
	const FGameplayTag FallbackLikeTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Parley.Emotion.Like")), false);
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

	if (!bResolvedAny)
	{
		AddWarning(TEXT("No configured emotion tag resolved to an icon in the current automation environment; skipping content-backed resolver assertion."));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEmo_ComponentViewerResolutionTest,
	"AlienRamen.Emo.Component.ViewerResolution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FEmo_ComponentViewerResolutionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* World = GetAutomationWorld(*this);
	if (!World)
	{
		return false;
	}

	const UEmoSettings* Settings = GetDefault<UEmoSettings>();
	if (!TestNotNull(TEXT("Emo settings should be available"), Settings))
	{
		return false;
	}

	const FGameplayTag BusyTag = Settings->BusyEmotionTag;
	const FGameplayTag WantsToTalkTag = Settings->WantsToTalkEmotionTag;
	const FGameplayTag PreviewTag = RequireTag(*this, TEXT("Parley.Emotion.Preview"));
	const FGameplayTag BrotherSpeakerTag = ARPlayer::GetBrotherParleySpeakerTag();
	const FGameplayTag BrotherShopTag = ARPlayer::GetBrotherShopCharacterTag();
	const FGameplayTag SisterShopTag = ARPlayer::GetSisterShopCharacterTag();
	if (!BusyTag.IsValid() || !WantsToTalkTag.IsValid() || !PreviewTag.IsValid() || !BrotherSpeakerTag.IsValid() || !BrotherShopTag.IsValid() || !SisterShopTag.IsValid())
	{
		return false;
	}

	AActor* Owner = SpawnAutomationActor<AActor>(World, *this, TEXT("Spawned emotion owner"));
	UEmoComponent* EmotionComponent = AddAutomationComponent<UEmoComponent>(Owner, TEXT("AutomationEmoComponent"));
	if (!TestNotNull(TEXT("Emotion component"), EmotionComponent))
	{
		if (Owner)
		{
			Owner->Destroy();
		}
		return false;
	}

	const FGameplayTagContainer BrotherSpeakerViewerTags = MakeTagContainer({ BrotherSpeakerTag });
	const FGameplayTagContainer BrotherShopViewerTags = MakeTagContainer({ BrotherShopTag });
	const FGameplayTagContainer SisterShopViewerTags = MakeTagContainer({ SisterShopTag });
	const FGameplayTagContainer CombinedBrotherViewerTags = MakeTagContainer({ BrotherSpeakerTag, BrotherShopTag });

	EmotionComponent->SetEmotionRegistration(TEXT("GlobalBusy"), BusyTag, 5);
	EmotionComponent->SetEmotionRegistration(TEXT("BrotherOnly"), PreviewTag, 5, BrotherShopViewerTags);
	TestTrue(
		TEXT("Global fallback resolves when no targeted registration matches"),
		EmotionComponent->GetDisplayedEmotionTagForViewerTags(SisterShopViewerTags).MatchesTagExact(BusyTag));

	EmotionComponent->ClearAllEmotionRegistrations();
	EmotionComponent->SetEmotionRegistration(TEXT("BrotherSpeaker"), WantsToTalkTag, 5, BrotherSpeakerViewerTags);
	TestTrue(
		TEXT("Exact tag overlap resolves against a viewer container with multiple tags"),
		EmotionComponent->GetDisplayedEmotionTagForViewerTags(CombinedBrotherViewerTags).MatchesTagExact(WantsToTalkTag));

	EmotionComponent->ClearAllEmotionRegistrations();
	EmotionComponent->SetEmotionRegistration(TEXT("BrotherLow"), PreviewTag, 1, BrotherShopViewerTags);
	EmotionComponent->SetEmotionRegistration(TEXT("GlobalHigh"), BusyTag, 2);
	TestTrue(
		TEXT("Higher-priority global registration beats lower-priority targeted registration"),
		EmotionComponent->GetDisplayedEmotionTagForViewerTags(BrotherShopViewerTags).MatchesTagExact(BusyTag));

	EmotionComponent->ClearAllEmotionRegistrations();
	EmotionComponent->SetEmotionRegistration(TEXT("GlobalEqual"), BusyTag, 4);
	EmotionComponent->SetEmotionRegistration(TEXT("BrotherEqual"), WantsToTalkTag, 4, BrotherShopViewerTags);
	TestTrue(
		TEXT("Targeted registration beats global registration on equal priority"),
		EmotionComponent->GetDisplayedEmotionTagForViewerTags(BrotherShopViewerTags).MatchesTagExact(WantsToTalkTag));

	EmotionComponent->ClearAllEmotionRegistrations();
	AddExpectedError(TEXT("Same-priority targeted emotion registrations matched viewer tags"), EAutomationExpectedErrorFlags::Contains, 1);
	EmotionComponent->SetEmotionRegistration(TEXT("TargetedA"), WantsToTalkTag, 7, BrotherShopViewerTags);
	EmotionComponent->SetEmotionRegistration(TEXT("TargetedB"), PreviewTag, 7, BrotherSpeakerViewerTags);
	TestTrue(
		TEXT("Latest write wins when same-priority targeted registrations tie"),
		EmotionComponent->GetDisplayedEmotionTagForViewerTags(CombinedBrotherViewerTags).MatchesTagExact(PreviewTag));

	Owner->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEmo_TimedRegistrationClearTest,
	"AlienRamen.Emo.Component.TimedRegistrationClear",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FEmo_TimedRegistrationClearTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* World = GetAutomationWorld(*this);
	if (!World)
	{
		return false;
	}

	const UEmoSettings* Settings = GetDefault<UEmoSettings>();
	if (!TestNotNull(TEXT("Emo settings should be available"), Settings))
	{
		return false;
	}

	const FGameplayTag BusyTag = Settings->BusyEmotionTag;
	const FGameplayTag PreviewTag = RequireTag(*this, TEXT("Parley.Emotion.Preview"));
	const FGameplayTag BrotherShopTag = ARPlayer::GetBrotherShopCharacterTag();
	if (!BusyTag.IsValid() || !PreviewTag.IsValid() || !BrotherShopTag.IsValid())
	{
		return false;
	}

	AActor* Owner = SpawnAutomationActor<AActor>(World, *this, TEXT("Spawned timed emotion owner"));
	UEmoComponent* EmotionComponent = AddAutomationComponent<UEmoComponent>(Owner, TEXT("TimedAutomationEmoComponent"));
	if (!TestNotNull(TEXT("Timed emotion component"), EmotionComponent))
	{
		if (Owner)
		{
			Owner->Destroy();
		}
		return false;
	}

	const FGameplayTagContainer BrotherViewerTags = MakeTagContainer({ BrotherShopTag });
	EmotionComponent->SetEmotionRegistration(TEXT("SharedSource"), BusyTag, 1);
	EmotionComponent->SetEmotionRegistrationForDuration(TEXT("SharedSource"), PreviewTag, 0.05f, 3, BrotherViewerTags);

	TestTrue(
		TEXT("Timed targeted registration is active before the timer expires"),
		EmotionComponent->GetDisplayedEmotionTagForViewerTags(BrotherViewerTags).MatchesTagExact(PreviewTag));

	struct FHandleTimedEmotionRegistrationClearParams
	{
		FName SourceId;
		FGameplayTagContainer TargetViewerTags;
	};

	FHandleTimedEmotionRegistrationClearParams TimedClearParams;
	TimedClearParams.SourceId = TEXT("SharedSource");
	TimedClearParams.TargetViewerTags = BrotherViewerTags;
	if (!InvokeUFunction(*this, EmotionComponent, TEXT("HandleTimedEmotionRegistrationClear"), TimedClearParams))
	{
		Owner->Destroy();
		return false;
	}

	TestTrue(
		TEXT("Timed targeted clear preserves the global registration owned by the same source id"),
		EmotionComponent->GetDisplayedEmotionTagForViewerTags(BrotherViewerTags).MatchesTagExact(BusyTag));

	Owner->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEmo_ViewerTagHelperTest,
	"AlienRamen.Emo.Game.ViewerTagHelper",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FEmo_ViewerTagHelperTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* World = GetAutomationWorld(*this);
	if (!World)
	{
		return false;
	}

	const UEmoSettings* Settings = GetDefault<UEmoSettings>();
	if (!TestNotNull(TEXT("Emo settings should be available"), Settings))
	{
		return false;
	}

	const FGameplayTag BusyTag = Settings->BusyEmotionTag;
	const FGameplayTag PreviewTag = RequireTag(*this, TEXT("Parley.Emotion.Preview"));
	const FGameplayTag BrotherShopTag = ARPlayer::GetBrotherShopCharacterTag();
	const FGameplayTag BrotherSpeakerTag = ARPlayer::GetBrotherParleySpeakerTag();
	const FGameplayTag SisterSpeakerTag = ARPlayer::GetSisterParleySpeakerTag();
	if (!BusyTag.IsValid() || !PreviewTag.IsValid() || !BrotherShopTag.IsValid() || !BrotherSpeakerTag.IsValid() || !SisterSpeakerTag.IsValid())
	{
		return false;
	}

	AARPlayerStateBase* PlayerState = SpawnAutomationActor<AARPlayerStateBase>(World, *this, TEXT("Spawned player state"));
	APawn* Pawn = SpawnAutomationActor<APawn>(World, *this, TEXT("Spawned pawn"));
	if (!PlayerState || !Pawn)
	{
		if (Pawn)
		{
			Pawn->Destroy();
		}
		if (PlayerState)
		{
			PlayerState->Destroy();
		}
		return false;
	}

	if (!SetGameplayTagProperty(*this, PlayerState, TEXT("CurrentCharacterTag"), BrotherShopTag))
	{
		Pawn->Destroy();
		PlayerState->Destroy();
		return false;
	}

	UParleySpeakerComponent* SpeakerComponent = AddAutomationComponent<UParleySpeakerComponent>(Pawn, TEXT("ViewerSpeakerComponent"));
	if (!TestNotNull(TEXT("Viewer speaker component"), SpeakerComponent))
	{
		Pawn->Destroy();
		PlayerState->Destroy();
		return false;
	}

	SpeakerComponent->SetSpeakerTag(SisterSpeakerTag);

	const FGameplayTagContainer ViewerTags = AREmotion::BuildEmotionViewerTags(PlayerState, Pawn);
	TestTrue(TEXT("Viewer tags include the canonical shop character tag"), ViewerTags.HasTagExact(BrotherShopTag));
	TestTrue(TEXT("Viewer tags include the canonical Parley speaker tag for the current character"), ViewerTags.HasTagExact(BrotherSpeakerTag));
	TestTrue(TEXT("Viewer tags include the possessed pawn speaker tag"), ViewerTags.HasTagExact(SisterSpeakerTag));

	AActor* Owner = SpawnAutomationActor<AActor>(World, *this, TEXT("Spawned helper emotion owner"));
	UEmoComponent* EmotionComponent = AddAutomationComponent<UEmoComponent>(Owner, TEXT("HelperAutomationEmoComponent"));
	if (!TestNotNull(TEXT("Helper emotion component"), EmotionComponent))
	{
		Owner->Destroy();
		Pawn->Destroy();
		PlayerState->Destroy();
		return false;
	}

	EmotionComponent->SetEmotionRegistration(TEXT("CharacterScoped"), BusyTag, 2, MakeTagContainer({ BrotherShopTag }));
	TestTrue(
		TEXT("Combined viewer tags match canonical shop-character registrations"),
		EmotionComponent->GetDisplayedEmotionTagForViewerTags(ViewerTags).MatchesTagExact(BusyTag));

	EmotionComponent->ClearAllEmotionRegistrations();
	EmotionComponent->SetEmotionRegistration(TEXT("SpeakerScoped"), PreviewTag, 2, MakeTagContainer({ SisterSpeakerTag }));
	TestTrue(
		TEXT("Combined viewer tags match possessed-speaker registrations"),
		EmotionComponent->GetDisplayedEmotionTagForViewerTags(ViewerTags).MatchesTagExact(PreviewTag));

	Owner->Destroy();
	Pawn->Destroy();
	PlayerState->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEmo_NPCBridgeTest,
	"AlienRamen.Emo.Game.NPCBridge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FEmo_NPCBridgeTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* World = GetAutomationWorld(*this);
	if (!World)
	{
		return false;
	}

	const UEmoSettings* Settings = GetDefault<UEmoSettings>();
	if (!TestNotNull(TEXT("Emo settings should be available"), Settings))
	{
		return false;
	}

	const FGameplayTag BusyTag = Settings->BusyEmotionTag;
	const FGameplayTag PreviewTag = RequireTag(*this, TEXT("Parley.Emotion.Preview"));
	const FGameplayTag BrotherSpeakerTag = ARPlayer::GetBrotherParleySpeakerTag();
	const FGameplayTag BrotherShopTag = ARPlayer::GetBrotherShopCharacterTag();
	if (!BusyTag.IsValid() || !PreviewTag.IsValid() || !BrotherSpeakerTag.IsValid() || !BrotherShopTag.IsValid())
	{
		return false;
	}

	AARNPCCharacterBase* NPC = SpawnAutomationActor<AARNPCCharacterBase>(World, *this, TEXT("Spawned NPC bridge actor"));
	if (!NPC)
	{
		return false;
	}

	UParleySpeakerComponent* SpeakerComponent = AddAutomationComponent<UParleySpeakerComponent>(NPC, TEXT("NPCBridgeSpeakerComponent"));
	UEmoComponent* EmotionComponent = AddAutomationComponent<UEmoComponent>(NPC, TEXT("NPCBridgeEmoComponent"));
	if (!TestNotNull(TEXT("NPC speaker component"), SpeakerComponent) || !TestNotNull(TEXT("NPC emotion component"), EmotionComponent))
	{
		NPC->Destroy();
		return false;
	}

	SpeakerComponent->SetSpeakerTag(BrotherSpeakerTag);
	if (!SetObjectProperty(*this, NPC, TEXT("SpeakerComponent"), SpeakerComponent)
		|| !SetObjectProperty(*this, NPC, TEXT("EmotionComponent"), EmotionComponent))
	{
		NPC->Destroy();
		return false;
	}

	TestTrue(TEXT("NPC cached speaker component is assigned"), NPC->GetSpeakerComponent() == SpeakerComponent);
	TestTrue(TEXT("NPC cached emotion component is assigned"), NPC->GetEmotionComponent() == EmotionComponent);

	AddWarning(TEXT("NPC bridge emotion assertions are skipped in command-line automation. Validate the bridge path under authoritative PIE/game automation where actor delegate execution is representative."));
	NPC->Destroy();
	return true;
}
