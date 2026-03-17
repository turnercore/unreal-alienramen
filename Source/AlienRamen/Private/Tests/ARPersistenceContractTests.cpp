#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "ARPlayerTypes.h"
#include "ARSaveGame.h"
#include "ARTravelSubsystem.h"
#include "ARTransitionTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FARPersistencePlayerProjectionTest,
	"AlienRamen.Save.PlayerState.ProjectionContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FARPersistencePlayerProjectionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FARPlayerStateSaveData PlayerData;
	PlayerData.CurrentCharacterTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Parley.Speaker.Brother")), false);
	PlayerData.ProgressionTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Progression.Dialogue")), false));
	PlayerData.SyncCharacterSelectionFromCurrentTag();

	TestEqual(TEXT("Current character resolves to brother tag"), PlayerData.ResolveCurrentCharacterTag(), FGameplayTag::RequestGameplayTag(FName(TEXT("Parley.Speaker.Brother")), false));
	TestEqual(TEXT("Compatibility enum mirrors canonical tag"), PlayerData.CharacterPicked, EARCharacterChoice::Brother);
	TestTrue(TEXT("Player-owned progression tags remain on the player row"), PlayerData.ProgressionTags.HasTagExact(FGameplayTag::RequestGameplayTag(FName(TEXT("Progression.Dialogue")), false)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FARPersistenceSaveSanitizePlayerStateTest,
	"AlienRamen.Save.ValidateAndSanitize.PlayerStateOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FARPersistenceSaveSanitizePlayerStateTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UARSaveGame* Save = NewObject<UARSaveGame>();
	if (!TestNotNull(TEXT("Created save object"), Save))
	{
		return false;
	}

	FARPlayerStateSaveData& PlayerData = Save->PlayerStates.AddDefaulted_GetRef();
	PlayerData.CurrentCharacterTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Parley.Speaker.Brother")), false);
	PlayerData.ProgressionTags.AddTag(FGameplayTag());
	PlayerData.ProgressionTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Progression.Dialogue")), false));

	FARCharacterSaveData& ValidCharacterState = Save->CharacterStates.AddDefaulted_GetRef();
	ValidCharacterState.CharacterTag = PlayerData.CurrentCharacterTag;
	ValidCharacterState.LoadoutTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.Ability.FirePrimary")), false));

	FARCharacterSaveData& InvalidCharacterState = Save->CharacterStates.AddDefaulted_GetRef();
	InvalidCharacterState.CharacterTag = FGameplayTag();
	InvalidCharacterState.LoadoutTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.Ability.FireSecondary")), false));

	TArray<FString> Warnings;
	const int32 ClampedCount = Save->ValidateAndSanitize(&Warnings);

	TestTrue(TEXT("Sanitize performs corrections"), ClampedCount > 0);
	TestTrue(TEXT("Valid player-owned progression tag preserved"), PlayerData.ProgressionTags.HasTagExact(FGameplayTag::RequestGameplayTag(FName(TEXT("Progression.Dialogue")), false)));
	TestEqual(TEXT("Invalid character-owned row removed"), Save->CharacterStates.Num(), 1);
	TestTrue(TEXT("Remaining character row keeps original loadout tags"), Save->CharacterStates[0].LoadoutTags.HasTagExact(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.Ability.FirePrimary")), false)));
	TestFalse(TEXT("Removed invalid row does not leak loadout tags"), Save->CharacterStates[0].LoadoutTags.HasTagExact(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.Ability.FireSecondary")), false)));
	TestEqual(TEXT("Current character tag remains canonical brother tag"), PlayerData.CurrentCharacterTag, FGameplayTag::RequestGameplayTag(FName(TEXT("Parley.Speaker.Brother")), false));
	TestTrue(TEXT("Warnings emitted for sanitization"), Warnings.Num() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FARPersistenceSaveLoadTransitionContextTest,
	"AlienRamen.Transition.SaveLoadEntry.ContextRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FARPersistenceSaveLoadTransitionContextTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FARTransitionContext Context;
	Context.SourceMode = EARTransitionSourceMode::SaveLoad;
	Context.Reason = EARTransitionReason::SaveLoadEntry;
	Context.DestinationURL = TEXT("/Game/Maps/Lvl_Shop");
	Context.bFreshLoadEntry = true;

	const FString TransitionURL = ARTransition::BuildTransitionTravelURL(TEXT("/Game/Maps/Lvl_Loading"), Context);
	TestTrue(TEXT("Transition URL contains source mode option"), TransitionURL.Contains(TEXT("ARTrSource=")));
	TestTrue(TEXT("Transition URL contains reason option"), TransitionURL.Contains(TEXT("ARTrReason=")));
	TestTrue(TEXT("Transition URL contains destination option"), TransitionURL.Contains(TEXT("ARTrDest=")));
	TestTrue(TEXT("Transition URL contains fresh-load option"), TransitionURL.Contains(TEXT("ARTrFresh=1")));

	FARTransitionContext ParsedTransitionContext;
	ARTransition::ApplyTransitionContextFromTravelOptions(TransitionURL, ParsedTransitionContext);
	TestEqual(TEXT("Parsed source mode round-trips"), ParsedTransitionContext.SourceMode, EARTransitionSourceMode::SaveLoad);
	TestEqual(TEXT("Parsed reason round-trips"), ParsedTransitionContext.Reason, EARTransitionReason::SaveLoadEntry);
	TestEqual(TEXT("Parsed destination round-trips"), ParsedTransitionContext.DestinationURL, FString(TEXT("/Game/Maps/Lvl_Shop")));
	TestTrue(TEXT("Parsed fresh-load flag round-trips"), ParsedTransitionContext.bFreshLoadEntry);

	const FString FinalTravelURL = ARTransition::AppendTransitionContextOptions(Context.DestinationURL, Context);
	FARTransitionContext ParsedFinalTravelContext;
	ARTransition::ApplyTransitionContextFromTravelOptions(FinalTravelURL, ParsedFinalTravelContext);
	TestEqual(TEXT("Final-leg source mode preserved"), ParsedFinalTravelContext.SourceMode, EARTransitionSourceMode::SaveLoad);
	TestEqual(TEXT("Final-leg reason preserved"), ParsedFinalTravelContext.Reason, EARTransitionReason::SaveLoadEntry);
	TestTrue(TEXT("Final-leg fresh-load flag preserved"), ParsedFinalTravelContext.bFreshLoadEntry);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FARPersistenceTransitionDestinationOptionsPreservedTest,
	"AlienRamen.Transition.ModeExit.DestinationOptionsPreservedThroughTransitionMap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FARPersistenceTransitionDestinationOptionsPreservedTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FARTransitionContext Context;
	Context.SourceMode = EARTransitionSourceMode::Shop;
	Context.Reason = EARTransitionReason::ShopToInvader;
	Context.DestinationURL = TEXT("/Game/Maps/Lvl_Invader?Game=/Script/AlienRamen.ARInvaderGameMode?Portal=Dock?CustomFlag=1");

	const FString TransitionURL = ARTransition::BuildTransitionTravelURL(TEXT("/Game/Maps/Lvl_Loading"), Context);
	FARTransitionContext ParsedTransitionContext;
	ARTransition::ApplyTransitionContextFromTravelOptions(TransitionURL, ParsedTransitionContext);
	TestEqual(TEXT("Wrapped transition preserves destination URL with options"), ParsedTransitionContext.DestinationURL, Context.DestinationURL);

	const FString FinalTravelURL = ARTransition::AppendTransitionContextOptions(ParsedTransitionContext.DestinationURL, ParsedTransitionContext);
	TestTrue(TEXT("Final travel URL preserves Game option"), FinalTravelURL.Contains(TEXT("?Game=/Script/AlienRamen.ARInvaderGameMode")));
	TestTrue(TEXT("Final travel URL preserves Portal option"), FinalTravelURL.Contains(TEXT("?Portal=Dock")));
	TestTrue(TEXT("Final travel URL preserves custom option"), FinalTravelURL.Contains(TEXT("?CustomFlag=1")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FARPersistenceSaveLoadFallbackDestinationTravelURLTest,
	"AlienRamen.Save.LoadTravel.FallbackModeTagDestinationUsedForDirectTravel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FARPersistenceSaveLoadFallbackDestinationTravelURLTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FARTransitionContext TransitionContext;
	TransitionContext.SourceMode = EARTransitionSourceMode::SaveLoad;
	TransitionContext.Reason = EARTransitionReason::SaveLoadEntry;
	TransitionContext.bFreshLoadEntry = true;

	const FGameplayTag ShopModeTag = FGameplayTag::RequestGameplayTag(TEXT("Mode.Shop"), false);
	const FString TravelURL = UARTravelSubsystem::BuildLoadedSaveTravelURL(TEXT(""), ShopModeTag, TEXT(""), TransitionContext);

	TestTrue(TEXT("Fallback mode tag resolves direct-travel URL"), !TravelURL.IsEmpty());
	TestTrue(TEXT("Fallback mode tag resolves to ramen shop map"), TravelURL.StartsWith(TEXT("/Game/Maps/Lvl_RamenShop")));
	TestEqual(TEXT("Context destination is set to resolved fallback"), TransitionContext.DestinationURL, FString(TEXT("/Game/Maps/Lvl_RamenShop")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FARPersistenceTravelOptionNormalizationTest,
	"AlienRamen.Transition.TravelOptions.ListenNormalizationUsesQuestionSeparator",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FARPersistenceTravelOptionNormalizationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FString URLWithOptions = TEXT("/Game/Maps/Lvl_RamenShop?Game=/Script/AlienRamen.ARShopGameMode?Portal=Kitchen");
	const FString NormalizedURL = ARTransition::EnsureTravelOption(URLWithOptions, TEXT("listen"));
	TestTrue(TEXT("Listen option appended with UE '?' separator"), NormalizedURL.Contains(TEXT("?Portal=Kitchen?listen")));
	TestFalse(TEXT("Listen option is never appended with '&'"), NormalizedURL.Contains(TEXT("&listen")));

	const FString AlreadyHasListen = ARTransition::EnsureTravelOption(TEXT("/Game/Maps/Lvl_RamenShop?listen"), TEXT("listen"));
	TestEqual(TEXT("Existing listen option is not duplicated"), AlreadyHasListen, FString(TEXT("/Game/Maps/Lvl_RamenShop?listen")));

	TestTrue(
		TEXT("Options-only strings still detect first-token options"),
		ARTransition::HasTravelOption(TEXT("listen?Portal=Dock"), TEXT("listen")));
	const FString OptionOnlyWithListen = ARTransition::EnsureTravelOption(TEXT("listen?Portal=Dock"), TEXT("listen"));
	TestEqual(
		TEXT("Options-only strings do not duplicate existing first-token options"),
		OptionOnlyWithListen,
		FString(TEXT("listen?Portal=Dock")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FARPersistencePlayerIdentityResolutionTest,
	"AlienRamen.Save.PlayerState.IdentityResolution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FARPersistencePlayerIdentityResolutionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UARSaveGame* Save = NewObject<UARSaveGame>();
	if (!TestNotNull(TEXT("Created save object"), Save))
	{
		return false;
	}

	FARPlayerStateSaveData& P1 = Save->PlayerStates.AddDefaulted_GetRef();
	P1.Identity.UniqueNetIdString = TEXT("SharedLocalId");
	P1.Identity.UniqueNetIdType = TEXT("LOCAL");
	P1.Identity.bSharedOnlineIdSecondaryProfile = false;
	P1.Identity.DisplayName = FText::FromString(TEXT("Player One"));
	P1.CurrentCharacterTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Parley.Speaker.Brother")), false);

	FARPlayerStateSaveData& P2 = Save->PlayerStates.AddDefaulted_GetRef();
	P2.Identity.UniqueNetIdString = TEXT("SharedLocalId");
	P2.Identity.UniqueNetIdType = TEXT("LOCAL");
	P2.Identity.bSharedOnlineIdSecondaryProfile = true;
	P2.Identity.DisplayName = FText::FromString(TEXT("Player Two"));
	P2.CurrentCharacterTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Parley.Speaker.Sister")), false);

	FARPlayerStateSaveData Matched;
	int32 MatchedIndex = INDEX_NONE;

	FARPlayerIdentity LookupIdentity;
	LookupIdentity.UniqueNetIdString = TEXT("SharedLocalId");
	LookupIdentity.UniqueNetIdType = TEXT("LOCAL");
	LookupIdentity.bSharedOnlineIdSecondaryProfile = true;
	TestTrue(TEXT("Identity lookup succeeds"), Save->FindPlayerStateDataByIdentity(LookupIdentity, Matched, MatchedIndex));
	TestEqual(TEXT("Identity lookup resolves shared-id secondary profile row"), MatchedIndex, 1);
	TestEqual(TEXT("Identity lookup returns secondary profile character row"), Matched.CurrentCharacterTag, P2.CurrentCharacterTag);

	FARPlayerIdentity PrimaryLookup = LookupIdentity;
	PrimaryLookup.bSharedOnlineIdSecondaryProfile = false;
	TestTrue(TEXT("Primary identity lookup succeeds when runtime character assignment changed"), Save->FindPlayerStateDataByIdentity(PrimaryLookup, Matched, MatchedIndex));
	TestEqual(TEXT("Primary identity lookup remains on primary profile row"), MatchedIndex, 0);
	TestEqual(TEXT("Primary identity lookup preserves primary character row"), Matched.CurrentCharacterTag, P1.CurrentCharacterTag);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FARPersistenceSharedAccountSecondaryProfileResolutionTest,
	"AlienRamen.Save.PlayerState.SharedAccountSecondaryProfileResolution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FARPersistenceSharedAccountSecondaryProfileResolutionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UARSaveGame* Save = NewObject<UARSaveGame>();
	if (!TestNotNull(TEXT("Created save object"), Save))
	{
		return false;
	}

	FARPlayerStateSaveData& Primary = Save->PlayerStates.AddDefaulted_GetRef();
	Primary.Identity.UniqueNetIdString = TEXT("SharedLocalId");
	Primary.Identity.UniqueNetIdType = TEXT("LOCAL");
	Primary.Identity.bSharedOnlineIdSecondaryProfile = false;
	Primary.CurrentCharacterTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Parley.Speaker.Brother")), false);

	FARPlayerIdentity SecondaryQuery;
	SecondaryQuery.UniqueNetIdString = TEXT("SharedLocalId");
	SecondaryQuery.UniqueNetIdType = TEXT("LOCAL");
	SecondaryQuery.bSharedOnlineIdSecondaryProfile = true;
	FARPlayerStateSaveData MatchedData;
	int32 MatchedIndex = INDEX_NONE;
	TestFalse(TEXT("Secondary shared-id profile is absent before creation"), Save->FindPlayerStateDataByIdentity(SecondaryQuery, MatchedData, MatchedIndex));

	FARPlayerStateSaveData& Secondary = Save->PlayerStates.AddDefaulted_GetRef();
	Secondary.Identity.UniqueNetIdString = TEXT("SharedLocalId");
	Secondary.Identity.UniqueNetIdType = TEXT("LOCAL");
	Secondary.Identity.bSharedOnlineIdSecondaryProfile = true;
	Secondary.CurrentCharacterTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Parley.Speaker.Sister")), false);

	TestTrue(TEXT("Secondary shared-id profile resolves once created"), Save->FindPlayerStateDataByIdentity(SecondaryQuery, MatchedData, MatchedIndex));
	TestEqual(TEXT("Secondary shared-id profile resolves to second row"), MatchedIndex, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FARPersistenceLegacyDialogueMergeTest,
	"AlienRamen.Save.MigrateLegacyDialogueRows.MergeByCharacter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FARPersistenceLegacyDialogueMergeTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UARSaveGame* Save = NewObject<UARSaveGame>();
	if (!TestNotNull(TEXT("Created save object"), Save))
	{
		return false;
	}

	Save->SaveGameVersion = 11;

	FARPlayerStateSaveData& PlayerData = Save->PlayerStates.AddDefaulted_GetRef();
	PlayerData.CurrentCharacterTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Parley.Speaker.Brother")), false);
	PlayerData.CharacterPicked = EARCharacterChoice::Brother;

	FDialoguePlayerPersistentState& LegacyA = Save->DialoguePlayerPersistentStates.AddDefaulted_GetRef();
	LegacyA.OwnerCharacterTag = ARPlayer::GetBrotherCharacterTag();
	LegacyA.ProgressionTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Progression.Dialogue")), false));
	LegacyA.CompletedConversationTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Parley.Conversations.Id.TestCactus.1")), false));

	FDialogueChoiceMemoryRecord SharedRecord;
	SharedRecord.ConversationTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Parley.Conversations.Id.TestCactus.1")), false);
	SharedRecord.ChoiceNodeId = FGuid::NewGuid();
	SharedRecord.SelectedBranchId = FGuid::NewGuid();
	LegacyA.CompletedChoiceRecords.Add(SharedRecord);

	FDialoguePlayerPersistentState& LegacyB = Save->DialoguePlayerPersistentStates.AddDefaulted_GetRef();
	LegacyB.OwnerCharacterTag = ARPlayer::GetBrotherCharacterTag();
	LegacyB.ProgressionTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Progression.Dialogue.Choice")), false));
	LegacyB.CompletedConversationTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Parley.Conversations.Id.TestCactus.2")), false));
	LegacyB.CompletedChoiceRecords.Add(SharedRecord);

	FDialogueChoiceMemoryRecord UniqueRecord;
	UniqueRecord.ConversationTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Parley.Conversations.Id.TestCactus.2")), false);
	UniqueRecord.ChoiceNodeId = FGuid::NewGuid();
	UniqueRecord.SelectedBranchId = FGuid::NewGuid();
	LegacyB.CompletedChoiceRecords.Add(UniqueRecord);

	TArray<FString> Warnings;
	const int32 ClampedCount = Save->ValidateAndSanitize(&Warnings);

	TestTrue(TEXT("Migration performs corrections"), ClampedCount > 0);
	TestEqual(TEXT("Legacy dialogue rows are cleared"), Save->DialoguePlayerPersistentStates.Num(), 0);
	TestEqual(TEXT("Merged legacy rows collapse into one character row"), Save->CharacterStates.Num(), 1);
	TestTrue(TEXT("Merged row keeps first progression tag"), Save->CharacterStates[0].DialogueState.ProgressionTags.HasTagExact(FGameplayTag::RequestGameplayTag(FName(TEXT("Progression.Dialogue")), false)));
	TestTrue(TEXT("Merged row keeps second progression tag"), Save->CharacterStates[0].DialogueState.ProgressionTags.HasTagExact(FGameplayTag::RequestGameplayTag(FName(TEXT("Progression.Dialogue.Choice")), false)));
	TestTrue(TEXT("Merged row keeps first completed conversation"), Save->CharacterStates[0].DialogueState.CompletedConversationTags.HasTagExact(FGameplayTag::RequestGameplayTag(FName(TEXT("Parley.Conversations.Id.TestCactus.1")), false)));
	TestTrue(TEXT("Merged row keeps second completed conversation"), Save->CharacterStates[0].DialogueState.CompletedConversationTags.HasTagExact(FGameplayTag::RequestGameplayTag(FName(TEXT("Parley.Conversations.Id.TestCactus.2")), false)));
	TestEqual(TEXT("Duplicate choice-memory rows are deduplicated during merge"), Save->CharacterStates[0].DialogueState.CompletedChoiceRecords.Num(), 2);
	TestTrue(TEXT("Migration emits merge warning summary"), Warnings.Num() > 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
