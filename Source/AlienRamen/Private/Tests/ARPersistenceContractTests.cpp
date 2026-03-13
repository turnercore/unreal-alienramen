#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "ARSaveGame.h"
#include "ARTransitionTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FARPersistencePlayerProjectionTest,
	"AlienRamen.Save.PlayerState.ProjectionContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FARPersistencePlayerProjectionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FARPlayerStateSaveData PlayerData;
	PlayerData.Identity.PlayerSlot = EARPlayerSlot::P1;
	PlayerData.CurrentCharacterTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Dialogue.Speaker.Brother")), false);
	PlayerData.ProgressionTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Progression.Dialogue")), false));
	PlayerData.SyncCharacterSelectionFromCurrentTag();

	TestEqual(TEXT("Current character resolves to brother tag"), PlayerData.ResolveCurrentCharacterTag(), FGameplayTag::RequestGameplayTag(FName(TEXT("Dialogue.Speaker.Brother")), false));
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
	PlayerData.Identity.PlayerSlot = EARPlayerSlot::P1;
	PlayerData.CurrentCharacterTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Dialogue.Speaker.Brother")), false);
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
	TestEqual(TEXT("Current character tag remains canonical brother tag"), PlayerData.CurrentCharacterTag, FGameplayTag::RequestGameplayTag(FName(TEXT("Dialogue.Speaker.Brother")), false));
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
	FARPersistencePlayerIdentityResolutionTest,
	"AlienRamen.Save.PlayerState.IdentityAndSlotResolution",
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
	P1.Identity.DisplayName = FText::FromString(TEXT("Player One"));
	P1.Identity.PlayerSlot = EARPlayerSlot::P1;
	P1.CurrentCharacterTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Dialogue.Speaker.Brother")), false);

	FARPlayerStateSaveData& P2 = Save->PlayerStates.AddDefaulted_GetRef();
	P2.Identity.UniqueNetIdString = TEXT("SharedLocalId");
	P2.Identity.UniqueNetIdType = TEXT("LOCAL");
	P2.Identity.DisplayName = FText::FromString(TEXT("Player Two"));
	P2.Identity.PlayerSlot = EARPlayerSlot::P2;
	P2.CurrentCharacterTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Dialogue.Speaker.Sister")), false);

	FARPlayerStateSaveData Matched;
	int32 MatchedIndex = INDEX_NONE;

	FARPlayerIdentity LookupIdentity;
	LookupIdentity.UniqueNetIdString = TEXT("SharedLocalId");
	LookupIdentity.UniqueNetIdType = TEXT("LOCAL");
	LookupIdentity.PlayerSlot = EARPlayerSlot::P2;
	TestTrue(TEXT("Identity lookup succeeds"), Save->FindPlayerStateDataByIdentity(LookupIdentity, Matched, MatchedIndex));
	TestEqual(TEXT("Identity lookup prefers matching slot when online ids collide"), MatchedIndex, 1);
	TestEqual(TEXT("Identity lookup returns slot-consistent character row"), Matched.CurrentCharacterTag, P2.CurrentCharacterTag);

	FARPlayerIdentity SlotFallbackIdentity;
	SlotFallbackIdentity.PlayerSlot = EARPlayerSlot::P1;
	TestTrue(TEXT("Slot fallback lookup succeeds"), Save->FindPlayerStateDataBySlot(SlotFallbackIdentity.PlayerSlot, Matched, MatchedIndex));
	TestEqual(TEXT("Slot fallback returns P1 row"), MatchedIndex, 0);
	TestEqual(TEXT("Slot fallback preserves current character tag"), Matched.CurrentCharacterTag, P1.CurrentCharacterTag);
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
	PlayerData.Identity.PlayerSlot = EARPlayerSlot::P1;
	PlayerData.CurrentCharacterTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Dialogue.Speaker.Brother")), false);
	PlayerData.CharacterPicked = EARCharacterChoice::Brother;

	FDialoguePlayerPersistentState& LegacyA = Save->DialoguePlayerPersistentStates.AddDefaulted_GetRef();
	LegacyA.Identity.PlayerSlot = EARPlayerSlot::P1;
	LegacyA.ProgressionTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Progression.Dialogue")), false));
	LegacyA.CompletedConversationTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Dialogue.Conversation.Id.TestCactus.1")), false));

	FDialogueChoiceMemoryRecord SharedRecord;
	SharedRecord.ConversationTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Dialogue.Conversation.Id.TestCactus.1")), false);
	SharedRecord.ChoiceNodeId = FGuid::NewGuid();
	SharedRecord.SelectedBranchId = FGuid::NewGuid();
	LegacyA.CompletedChoiceRecords.Add(SharedRecord);

	FDialoguePlayerPersistentState& LegacyB = Save->DialoguePlayerPersistentStates.AddDefaulted_GetRef();
	LegacyB.Identity.PlayerSlot = EARPlayerSlot::P1;
	LegacyB.ProgressionTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Progression.Dialogue.Choice")), false));
	LegacyB.CompletedConversationTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Dialogue.Conversation.Id.TestCactus.2")), false));
	LegacyB.CompletedChoiceRecords.Add(SharedRecord);

	FDialogueChoiceMemoryRecord UniqueRecord;
	UniqueRecord.ConversationTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Dialogue.Conversation.Id.TestCactus.2")), false);
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
	TestTrue(TEXT("Merged row keeps first completed conversation"), Save->CharacterStates[0].DialogueState.CompletedConversationTags.HasTagExact(FGameplayTag::RequestGameplayTag(FName(TEXT("Dialogue.Conversation.Id.TestCactus.1")), false)));
	TestTrue(TEXT("Merged row keeps second completed conversation"), Save->CharacterStates[0].DialogueState.CompletedConversationTags.HasTagExact(FGameplayTag::RequestGameplayTag(FName(TEXT("Dialogue.Conversation.Id.TestCactus.2")), false)));
	TestEqual(TEXT("Duplicate choice-memory rows are deduplicated during merge"), Save->CharacterStates[0].DialogueState.CompletedChoiceRecords.Num(), 2);
	TestTrue(TEXT("Migration emits merge warning summary"), Warnings.Num() > 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
