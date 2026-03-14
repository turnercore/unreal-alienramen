#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "ARPlayerTypes.h"
#include "ParleyDialogueTypes.h"
#include "ARSaveGame.h"
#include "Kismet/GameplayStatics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FARDialogueSchemaVersionTest,
	"AlienRamen.Dialogue.Save.SchemaVersion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FARDialogueSchemaVersionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const int32 CurrentSchemaVersion = UARSaveGame::GetCurrentSchemaVersion();
	const int32 MinSupportedSchemaVersion = UARSaveGame::GetMinSupportedSchemaVersion();

	TestTrue(TEXT("Current schema version is positive"), CurrentSchemaVersion > 0);
	TestTrue(TEXT("Min supported schema version is positive"), MinSupportedSchemaVersion > 0);
	TestTrue(TEXT("Current schema version is >= min supported schema"), CurrentSchemaVersion >= MinSupportedSchemaVersion);
	TestTrue(TEXT("Current schema is supported"), UARSaveGame::IsSchemaVersionSupported(CurrentSchemaVersion));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FARDialogueSaveSanitizeTest,
	"AlienRamen.Dialogue.Save.ValidateAndSanitize",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FARDialogueSaveSanitizeTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UARSaveGame* Save = Cast<UARSaveGame>(UGameplayStatics::CreateSaveGameObject(UARSaveGame::StaticClass()));
	if (!TestNotNull(TEXT("Created save object"), Save))
	{
		return false;
	}

	{
		FDialogueRelationshipState InvalidRelationship;
		InvalidRelationship.RelationshipPoints = 10.0f;
		Save->DialogueRelationshipStates.Add(InvalidRelationship);

		FDialogueRelationshipState ValidRelationship;
		ValidRelationship.SpeakerTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Dialogue.Speaker")), false);
		ValidRelationship.RelationshipPoints = 25.0f;
		Save->DialogueRelationshipStates.Add(ValidRelationship);
	}

	{
		FARCharacterSaveData CharacterState;
		CharacterState.CharacterTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Dialogue.Speaker.Brother")), false);

		FDialoguePlayerPersistentState& PlayerState = CharacterState.DialogueState;
		FDialogueChoiceMemoryRecord InvalidRecord;
		InvalidRecord.ConversationTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Dialogue.Conversation")), false);
		PlayerState.CompletedChoiceRecords.Add(InvalidRecord);
		Save->CharacterStates.Add(CharacterState);
	}

	TArray<FString> Warnings;
	const int32 ClampedCount = Save->ValidateAndSanitize(&Warnings);
	TestTrue(TEXT("Sanitization performs at least one correction"), ClampedCount > 0);
	TestEqual(TEXT("Invalid relationship rows removed"), Save->DialogueRelationshipStates.Num(), 1);
	TestEqual(TEXT("Invalid choice memory rows removed"), Save->CharacterStates[0].DialogueState.CompletedChoiceRecords.Num(), 0);
	TestTrue(TEXT("Warnings produced for invalid dialogue data"), Warnings.Num() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FARDialogueSaveMigrationTest,
	"AlienRamen.Dialogue.Save.MigrateOwnershipModel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FARDialogueSaveMigrationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UARSaveGame* Save = Cast<UARSaveGame>(UGameplayStatics::CreateSaveGameObject(UARSaveGame::StaticClass()));
	if (!TestNotNull(TEXT("Created save object"), Save))
	{
		return false;
	}

	Save->SaveGameVersion = 10;

	FARPlayerStateSaveData& PlayerState = Save->PlayerStates.AddDefaulted_GetRef();
	PlayerState.Identity.PlayerSlot = EARPlayerSlot::P1;
	PlayerState.CharacterPicked = EARCharacterChoice::Brother;

	FDialoguePlayerPersistentState& LegacyDialogueState = Save->DialoguePlayerPersistentStates.AddDefaulted_GetRef();
	LegacyDialogueState.OwnerPlayerSlotTag = ARPlayer::GetPlayerSlotTag(EARPlayerSlot::P1);
	LegacyDialogueState.ProgressionTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Dialogue.Speaker.Brother.Default")), false));
	LegacyDialogueState.CompletedConversationTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Dialogue.Conversation.Id.TestCactus.1")), false));

	FDialogueChoiceMemoryRecord ChoiceRecord;
	ChoiceRecord.ConversationTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Dialogue.Conversation.Id.TestCactus.1")), false);
	ChoiceRecord.ChoiceNodeId = FGuid::NewGuid();
	ChoiceRecord.SelectedBranchId = FGuid::NewGuid();
	LegacyDialogueState.CompletedChoiceRecords.Add(ChoiceRecord);

	TArray<FString> Warnings;
	const int32 ClampedCount = Save->ValidateAndSanitize(&Warnings);

	TestTrue(TEXT("Migration performed at least one correction"), ClampedCount > 0);
	TestEqual(TEXT("Schema migrated to current version"), Save->SaveGameVersion, UARSaveGame::GetCurrentSchemaVersion());
	TestEqual(TEXT("Legacy dialogue array cleared after migration"), Save->DialoguePlayerPersistentStates.Num(), 0);

	TestEqual(TEXT("One character row created"), Save->CharacterStates.Num(), 1);
	TestEqual(TEXT("Character row keyed by Brother tag"), Save->CharacterStates[0].CharacterTag, FGameplayTag::RequestGameplayTag(FName(TEXT("Dialogue.Speaker.Brother")), false));
	TestTrue(TEXT("Dialogue progression migrated to character row"), Save->CharacterStates[0].DialogueState.ProgressionTags.HasTagExact(FGameplayTag::RequestGameplayTag(FName(TEXT("Dialogue.Speaker.Brother.Default")), false)));
	TestTrue(TEXT("Dialogue completion migrated to character row"), Save->CharacterStates[0].DialogueState.CompletedConversationTags.HasTagExact(FGameplayTag::RequestGameplayTag(FName(TEXT("Dialogue.Conversation.Id.TestCactus.1")), false)));
	TestEqual(TEXT("Dialogue choice memory migrated to character row"), Save->CharacterStates[0].DialogueState.CompletedChoiceRecords.Num(), 1);

	TestEqual(TEXT("Current character tag resolved"), Save->PlayerStates[0].CurrentCharacterTag, FGameplayTag::RequestGameplayTag(FName(TEXT("Dialogue.Speaker.Brother")), false));
	TestEqual(TEXT("Character selection enum mirrors canonical tag"), Save->PlayerStates[0].CharacterPicked, EARCharacterChoice::Brother);
	TestTrue(TEXT("Migration warnings emitted"), Warnings.Num() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDialogueTypesDefaultsTest,
	"AlienRamen.Dialogue.Types.Defaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDialogueTypesDefaultsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FDialogueConversationHeader Header;
	TestEqual(TEXT("Header default priority is zero"), Header.Priority, 0);
	TestFalse(TEXT("Header default repeatable is false"), Header.bRepeatable);
	TestFalse(TEXT("Header default important is false"), Header.bImportant);
	TestEqual(TEXT("Header default character restriction is Any"), Header.CharacterRestriction, EDialogueActiveCharacterRestriction::Any);

	const FDialogueLineNodeData LineNodeData;
	TestEqual(TEXT("Line node default character restriction is Any"), LineNodeData.CharacterRestriction, EDialogueActiveCharacterRestriction::Any);

	const FDialogueClientView View;
	TestFalse(TEXT("Default view waiting-for-choice is false"), View.bWaitingForChoice);
	TestFalse(TEXT("Default view eavesdropping is false"), View.bIsEavesdropping);
	TestEqual(TEXT("Default view choices array is empty"), View.Choices.Num(), 0);

	const FDialogueChoiceNodeData ChoiceNode;
	TestEqual(TEXT("Choice default policy is locked"), ChoiceNode.CompletedChoicePolicy, EDialogueCompletedChoicePolicy::LockedToRecordedChoice);
	TestEqual(TEXT("Choice default fallback text"), ChoiceNode.FallbackChoiceText.ToString(), FString(TEXT("...")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
