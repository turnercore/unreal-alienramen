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

	const FGameplayTag ValidNpcTag = FGameplayTag::RequestGameplayTag(FName(TEXT("NPC.Identity")), false);
	const FGameplayTag ValidNodeTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Dialogue.Node")), false);
	const FGameplayTag ValidChoiceTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Progression.Dialogue.Choice")), false);

	if (!TestTrue(TEXT("Valid NPC tag exists"), ValidNpcTag.IsValid()) ||
		!TestTrue(TEXT("Valid dialogue node tag exists"), ValidNodeTag.IsValid()) ||
		!TestTrue(TEXT("Valid dialogue choice tag exists"), ValidChoiceTag.IsValid()))
	{
		return false;
	}

	// Invalid NPC entry should be removed, negative love should clamp.
	{
		FARNpcRelationshipState InvalidNpc;
		InvalidNpc.LoveRating = 10;
		Save->NpcRelationshipStates.Add(InvalidNpc);

		FARNpcRelationshipState ValidNpc;
		ValidNpc.NpcTag = ValidNpcTag;
		ValidNpc.LoveRating = -3;
		Save->NpcRelationshipStates.Add(ValidNpc);
	}

	// Invalid canonical choice entry should be removed.
	{
		FARDialogueCanonicalChoiceState Invalid;
		Invalid.NodeTag = ValidNodeTag;
		Save->DialogueCanonicalChoiceStates.Add(Invalid);
	}

	// Duplicate canonical node should dedupe down to one entry.
	{
		FARDialogueCanonicalChoiceState A;
		A.NodeTag = ValidNodeTag;
		A.ChoiceTag = ValidChoiceTag;
		Save->DialogueCanonicalChoiceStates.Add(A);

		FARDialogueCanonicalChoiceState B;
		B.NodeTag = ValidNodeTag;
		B.ChoiceTag = ValidChoiceTag;
		Save->DialogueCanonicalChoiceStates.Add(B);
	}

	TArray<FString> Warnings;
	const int32 ClampedCount = Save->ValidateAndSanitize(&Warnings);
	TestTrue(TEXT("Sanitization performs at least one correction"), ClampedCount > 0);

	TestEqual(TEXT("NPC relationship entries sanitized to one valid row"), Save->NpcRelationshipStates.Num(), 1);
	if (Save->NpcRelationshipStates.Num() == 1)
	{
		TestTrue(TEXT("Remaining NPC tag is valid"), Save->NpcRelationshipStates[0].NpcTag.IsValid());
		TestEqual(TEXT("Remaining NPC love clamped to non-negative"), Save->NpcRelationshipStates[0].LoveRating, 0);
	}

	TestEqual(TEXT("Canonical dialogue choice entries deduped to one row"), Save->DialogueCanonicalChoiceStates.Num(), 1);
	if (Save->DialogueCanonicalChoiceStates.Num() == 1)
	{
		TestTrue(TEXT("Canonical node tag remains valid"), Save->DialogueCanonicalChoiceStates[0].NodeTag.IsValid());
		TestTrue(TEXT("Canonical choice tag remains valid"), Save->DialogueCanonicalChoiceStates[0].ChoiceTag.IsValid());
	}

	TestTrue(TEXT("Warnings produced for invalid/duplicate dialogue data"), Warnings.Num() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FARDialogueNodeDefaultsTest,
	"AlienRamen.Dialogue.Types.NodeDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FARDialogueNodeDefaultsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FARDialogueNodeRow Row;
	TestEqual(TEXT("Default choice participation is InitiatorOnly"), Row.ChoiceParticipation, EARDialogueChoiceParticipation::InitiatorOnly);
	TestFalse(TEXT("Important decision force-eavesdrop defaults false"), Row.bForceEavesdropForImportantDecision);
	TestEqual(TEXT("Default priority is zero"), Row.Priority, 0);
	TestEqual(TEXT("Default choices array is empty"), Row.Choices.Num(), 0);
	TestFalse(TEXT("Allow repeat after seen defaults false"), Row.bAllowRepeatAfterSeen);
	TestEqual(TEXT("Default min love rating is zero"), Row.MinLoveRating, 0);
	TestFalse(TEXT("Requires want satisfied defaults false"), Row.bRequiresWantSatisfied);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FARDialogueClientViewDefaultsTest,
	"AlienRamen.Dialogue.Types.ClientViewDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FARDialogueClientViewDefaultsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FARDialogueClientView View;
	TestFalse(TEXT("Default bWaitingForChoice is false"), View.bWaitingForChoice);
	TestFalse(TEXT("Default bIsSharedSession is false"), View.bIsSharedSession);
	TestFalse(TEXT("Default bIsEavesdropping is false"), View.bIsEavesdropping);
	TestEqual(TEXT("Default InitiatorSlot is Unknown"), View.InitiatorSlot, EARPlayerSlot::Unknown);
	TestEqual(TEXT("Default OwnerSlot is Unknown"), View.OwnerSlot, EARPlayerSlot::Unknown);
	TestTrue(TEXT("Default SessionId is empty"), View.SessionId.IsEmpty());
	TestFalse(TEXT("Default NpcTag is invalid"), View.NpcTag.IsValid());
	TestEqual(TEXT("Default Choices array is empty"), View.Choices.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FARNpcDefinitionRowDefaultsTest,
	"AlienRamen.Dialogue.Types.NpcDefinitionRowDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FARNpcDefinitionRowDefaultsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FARNpcDefinitionRow Row;
	TestEqual(TEXT("Default starting love rating is zero"), Row.StartingLoveRating, 0);
	TestEqual(TEXT("Default love increase on want delivery is 1"), Row.LoveIncreaseOnWantDelivery, 1);
	TestFalse(TEXT("Default NpcTag is invalid"), Row.NpcTag.IsValid());
	TestFalse(TEXT("Default InitialWantTag is invalid"), Row.InitialWantTag.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FARNpcRelationshipStateDefaultsTest,
	"AlienRamen.Dialogue.Types.NpcRelationshipStateDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FARNpcRelationshipStateDefaultsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FARNpcRelationshipState State;
	TestEqual(TEXT("Default love rating is zero"), State.LoveRating, 0);
	TestFalse(TEXT("Default want satisfied is false"), State.bCurrentWantSatisfied);
	TestFalse(TEXT("Default NpcTag is invalid"), State.NpcTag.IsValid());
	TestFalse(TEXT("Default CurrentWantTag is invalid"), State.CurrentWantTag.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FARPlayerDialogueHistoryStateDefaultsTest,
	"AlienRamen.Dialogue.Types.PlayerHistoryStateDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FARPlayerDialogueHistoryStateDefaultsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FARPlayerDialogueHistoryState State;
	TestTrue(TEXT("Default seen node tags is empty"), State.SeenNodeTags.IsEmpty());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
