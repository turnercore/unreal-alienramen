#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "ARDialogueTypes.h"
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
	TestEqual(TEXT("Without migrations, current schema equals min supported schema"), CurrentSchemaVersion, MinSupportedSchemaVersion);
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
		FDialoguePlayerPersistentState PlayerState;
		FDialogueChoiceMemoryRecord InvalidRecord;
		InvalidRecord.ConversationTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Dialogue.Conversation")), false);
		PlayerState.CompletedChoiceRecords.Add(InvalidRecord);
		Save->DialoguePlayerPersistentStates.Add(PlayerState);
	}

	TArray<FString> Warnings;
	const int32 ClampedCount = Save->ValidateAndSanitize(&Warnings);
	TestTrue(TEXT("Sanitization performs at least one correction"), ClampedCount > 0);
	TestEqual(TEXT("Invalid relationship rows removed"), Save->DialogueRelationshipStates.Num(), 1);
	TestEqual(TEXT("Invalid choice memory rows removed"), Save->DialoguePlayerPersistentStates[0].CompletedChoiceRecords.Num(), 0);
	TestTrue(TEXT("Warnings produced for invalid dialogue data"), Warnings.Num() > 0);
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
