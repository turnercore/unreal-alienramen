#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AREnemyAIController.h"
#include "AREnemyBase.h"
#include "ARStateTreeAIComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameplayTagContainer.h"
#include "UObject/UObjectGlobals.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAREnemyAIStateTagBridgeTest,
	"AlienRamen.AI.StateTree.ASCStateTagBridge",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FAREnemyAIStateTagBridgeTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* TestWorld = nullptr;
	if (GEngine)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				TestWorld = Context.World();
				if (TestWorld)
				{
					break;
				}
			}
		}
	}

	if (!TestNotNull(TEXT("Test world (PIE/Game context)"), TestWorld))
	{
		return false;
	}

	if (!TestNotNull(TEXT("GameInstance"), TestWorld->GetGameInstance()))
	{
		return false;
	}

	const FGameplayTag StateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Enemy.Berserk")), false);
	if (!StateTag.IsValid())
	{
		AddError(TEXT("Missing gameplay tag 'State.Enemy.Berserk'."));
		return false;
	}

	const FGameplayTag EnemyIdentifierTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Enemy.Identifier.Grunt")), false);
	if (!EnemyIdentifierTag.IsValid())
	{
		AddError(TEXT("Missing gameplay tag 'Enemy.Identifier.Grunt'."));
		return false;
	}

	UClass* EnemyClass = StaticLoadClass(
		AAREnemyBase::StaticClass(),
		nullptr,
		TEXT("/Game/CodeAlong/Blueprints/Enemies/BP_EnemyBase_Grunt.BP_EnemyBase_Grunt_C"));
	UClass* ControllerClass = StaticLoadClass(
		AAREnemyAIController::StaticClass(),
		nullptr,
		TEXT("/Game/CodeAlong/Blueprints/Enemies/AI/Controllers/BP_AIController_InvaderEnemy.BP_AIController_InvaderEnemy_C"));

	if (!TestNotNull(TEXT("Loaded BP enemy class"), EnemyClass) ||
		!TestNotNull(TEXT("Loaded BP AI controller class"), ControllerClass))
	{
		return false;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AAREnemyBase* Enemy = TestWorld->SpawnActor<AAREnemyBase>(
		EnemyClass,
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParams);
	if (!TestNotNull(TEXT("Spawned enemy"), Enemy))
	{
		return false;
	}

	AAREnemyAIController* Controller = TestWorld->SpawnActor<AAREnemyAIController>(
		ControllerClass,
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParams);
	if (!TestNotNull(TEXT("Spawned enemy AI controller"), Controller))
	{
		Enemy->Destroy();
		return false;
	}

	Enemy->SetEnemyIdentifierTag(EnemyIdentifierTag);
	Controller->Possess(Enemy);

	UARStateTreeAIComponent* StateTreeComp = Controller->GetEnemyStateTreeComponent();
	if (!TestNotNull(TEXT("Enemy StateTree component"), StateTreeComp))
	{
		Controller->Destroy();
		Enemy->Destroy();
		return false;
	}

	TestFalse(TEXT("Initial mirrored ASC state tag is absent"), Enemy->HasASCGameplayTag(StateTag));
	TestEqual(TEXT("Initial ASC state tag ref count is zero"), Enemy->GetASCStateTagRefCount(StateTag), 0);

	{
		FGameplayTagContainer AddedTags;
		AddedTags.AddTag(StateTag);
		FGameplayTagContainer RemovedTags;
		StateTreeComp->OnActiveStateTagsChanged.Broadcast(AddedTags, RemovedTags);
	}

	TestTrue(TEXT("ASC state tag is added after state enter"), Enemy->HasASCGameplayTag(StateTag));
	TestEqual(TEXT("ASC state tag ref count after add"), Enemy->GetASCStateTagRefCount(StateTag), 1);

	{
		// Duplicate add should be deduped by controller bridge bookkeeping.
		FGameplayTagContainer AddedTags;
		AddedTags.AddTag(StateTag);
		FGameplayTagContainer RemovedTags;
		StateTreeComp->OnActiveStateTagsChanged.Broadcast(AddedTags, RemovedTags);
	}

	TestEqual(TEXT("ASC state tag ref count remains deduped on duplicate add"), Enemy->GetASCStateTagRefCount(StateTag), 1);

	{
		FGameplayTagContainer AddedTags;
		FGameplayTagContainer RemovedTags;
		RemovedTags.AddTag(StateTag);
		StateTreeComp->OnActiveStateTagsChanged.Broadcast(AddedTags, RemovedTags);
	}

	TestFalse(TEXT("ASC state tag is removed after state exit"), Enemy->HasASCGameplayTag(StateTag));
	TestEqual(TEXT("ASC state tag ref count after remove"), Enemy->GetASCStateTagRefCount(StateTag), 0);

	Controller->UnPossess();
	Controller->Destroy();
	Enemy->Destroy();

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
