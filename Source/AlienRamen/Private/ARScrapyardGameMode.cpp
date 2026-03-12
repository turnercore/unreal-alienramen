#include "ARScrapyardGameMode.h"

#include "ARGameStateBase.h"
#include "ARLog.h"
#include "ARPlayerStateBase.h"
#include "ARRunBuffSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "StructUtils/InstancedStruct.h"
#include "TagContentResolverSubsystem.h"
#include "UObject/UnrealType.h"

namespace
{
	static void ApplyActiveRunBuffsForController(AARScrapyardGameMode* GameMode, AController* Controller)
	{
		if (!GameMode || !Controller || !GameMode->HasAuthority())
		{
			return;
		}

		if (UGameInstance* GameInstance = GameMode->GetGameInstance())
		{
			if (UARRunBuffSubsystem* RunBuffSubsystem = GameInstance->GetSubsystem<UARRunBuffSubsystem>())
			{
				if (AARPlayerStateBase* PlayerState = Controller->GetPlayerState<AARPlayerStateBase>())
				{
					RunBuffSubsystem->ApplyActiveRunBuffsToPlayerState(PlayerState);
				}
			}
		}
	}
}

AARScrapyardGameMode::AARScrapyardGameMode()
{
	ModeTag = FGameplayTag::RequestGameplayTag(TEXT("Mode.Scrapyard"), false);
	ensureMsgf(ModeTag.IsValid(), TEXT("[ScrapyardGameMode] Required gameplay tag 'Mode.Scrapyard' is missing."));
}

void AARScrapyardGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UARRunBuffSubsystem* RunBuffSubsystem = GameInstance->GetSubsystem<UARRunBuffSubsystem>())
		{
			if (AARGameStateBase* GameState = World->GetGameState<AARGameStateBase>())
			{
				for (AARPlayerStateBase* PlayerState : GameState->GetPlayerStates())
				{
					RunBuffSubsystem->ApplyActiveRunBuffsToPlayerState(PlayerState);
				}
			}
		}
	}
}

void AARScrapyardGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
	ApplyActiveRunBuffsForController(this, NewPlayer);
}

void AARScrapyardGameMode::RestartPlayer(AController* NewPlayer)
{
	Super::RestartPlayer(NewPlayer);
	ApplyActiveRunBuffsForController(this, NewPlayer);
}

TSubclassOf<APawn> AARScrapyardGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	if (InController)
	{
		const AARPlayerStateBase* PlayerState = InController->GetPlayerState<AARPlayerStateBase>();
		const FGameplayTag ShipRootTag = FGameplayTag::RequestGameplayTag(TEXT("Unlock.Ship"), false);
		FGameplayTag ShipTag;
		if (PlayerState
			&& ShipRootTag.IsValid()
			&& FindFirstTagUnderRoot(PlayerState->LoadoutTags, ShipRootTag, ShipTag))
		{
			TSubclassOf<APawn> ResolvedPawnClass;
			if (ResolveScrapyardPawnClassFromShipTag(ShipTag, ResolvedPawnClass) && ResolvedPawnClass)
			{
				return ResolvedPawnClass;
			}

			UE_LOG(
				ARLog,
				Verbose,
				TEXT("[ScrapyardGameMode] Falling back from unresolved ScrapyardPawnClass for ship '%s'."),
				*ShipTag.ToString());
		}
	}

	if (FallbackScrapyardPawnClass)
	{
		return FallbackScrapyardPawnClass;
	}

	return Super::GetDefaultPawnClassForController_Implementation(InController);
}

FProperty* AARScrapyardGameMode::FindPropertyByNamePrefix(const UScriptStruct* StructType, const FString& Prefix)
{
	if (!StructType)
	{
		return nullptr;
	}

	for (TFieldIterator<FProperty> It(StructType); It; ++It)
	{
		FProperty* Property = *It;
		if (Property && Property->GetName().StartsWith(Prefix))
		{
			return Property;
		}
	}

	return nullptr;
}

bool AARScrapyardGameMode::ResolveScrapyardPawnClassFromShipTag(const FGameplayTag ShipTag, TSubclassOf<APawn>& OutPawnClass) const
{
	OutPawnClass = nullptr;
	if (!ShipTag.IsValid())
	{
		return false;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UTagContentResolverSubsystem* Resolver = GameInstance ? GameInstance->GetSubsystem<UTagContentResolverSubsystem>() : nullptr;
	if (!Resolver)
	{
		return false;
	}

	FInstancedStruct ShipRow;
	FString ResolveError;
	if (!Resolver->TryResolveRowForTag(ShipTag, ShipRow, ResolveError))
	{
		UE_LOG(
			ARLog,
			Warning,
			TEXT("[ScrapyardGameMode] Failed resolving ship row '%s': %s"),
			*ShipTag.ToString(),
			*ResolveError);
		return false;
	}

	const UScriptStruct* StructType = ShipRow.GetScriptStruct();
	const void* StructData = ShipRow.GetMemory();
	if (!StructType || !StructData)
	{
		return false;
	}

	FProperty* PawnClassProperty = FindPropertyByNamePrefix(StructType, TEXT("ScrapyardPawnClass"));
	if (!PawnClassProperty)
	{
		return false;
	}

	if (const FClassProperty* ClassProperty = CastField<FClassProperty>(PawnClassProperty))
	{
		if (UClass* PawnClass = Cast<UClass>(ClassProperty->GetPropertyValue_InContainer(StructData)))
		{
			OutPawnClass = PawnClass;
			return OutPawnClass != nullptr;
		}
	}
	else if (const FSoftClassProperty* SoftClassProperty = CastField<FSoftClassProperty>(PawnClassProperty))
	{
		const FSoftObjectPtr SoftClassPtr = SoftClassProperty->GetPropertyValue_InContainer(StructData);
		if (UClass* PawnClass = Cast<UClass>(SoftClassPtr.LoadSynchronous()))
		{
			OutPawnClass = PawnClass;
			return OutPawnClass != nullptr;
		}
	}

	return false;
}

bool AARScrapyardGameMode::FindFirstTagUnderRoot(const FGameplayTagContainer& InTags, const FGameplayTag& RootTag, FGameplayTag& OutTag)
{
	OutTag = FGameplayTag();
	if (!RootTag.IsValid())
	{
		return false;
	}

	for (const FGameplayTag Tag : InTags)
	{
		if (Tag.IsValid() && Tag.MatchesTag(RootTag))
		{
			OutTag = Tag;
			return true;
		}
	}

	return false;
}
