#include "ARInvaderGameMode.h"

#include "ARLog.h"
#include "ARPlayerStateBase.h"
#include "ARRunBuffSubsystem.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "StructUtils/InstancedStruct.h"
#include "TagKeySubsystem.h"
#include "UObject/UnrealType.h"

namespace
{
	static void ApplyActiveRunBuffsForController(AARInvaderGameMode* GameMode, AController* Controller)
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

AARInvaderGameMode::AARInvaderGameMode()
{
	ModeTag = FGameplayTag::RequestGameplayTag(TEXT("Mode.Invader"), false);
	ensureMsgf(ModeTag.IsValid(), TEXT("[InvaderGameMode] Required gameplay tag 'Mode.Invader' is missing."));
	bAutosaveOnQuit = false;
	bAllowManualSaveInMode = false;
	bShareLocalPauseAcrossControllersInMode = true;
	bRouteModeTravelThroughTransitionMap = true;
	TransitionTravelMapURL = TEXT("/Game/Maps/Lvl_Loading");
	TransitionSourceMode = EARTransitionSourceMode::Invader;
	TransitionReason = EARTransitionReason::InvaderToScrapyard;
}

void AARInvaderGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UARRunBuffSubsystem* RunBuffSubsystem = GameInstance->GetSubsystem<UARRunBuffSubsystem>())
		{
			RunBuffSubsystem->RotateRunBuffsAtInvaderInit();
		}
		else
		{
			UE_LOG(ARLog, Warning, TEXT("[InvaderGameMode] Missing RunBuffSubsystem during invader init rotation."));
		}
	}
}

void AARInvaderGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);

	ApplyActiveRunBuffsForController(this, NewPlayer);
}

void AARInvaderGameMode::RestartPlayer(AController* NewPlayer)
{
	Super::RestartPlayer(NewPlayer);
	ApplyActiveRunBuffsForController(this, NewPlayer);
}

UClass* AARInvaderGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
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
			if (ResolveInvaderPawnClassFromShipTag(ShipTag, ResolvedPawnClass) && ResolvedPawnClass)
			{
				return ResolvedPawnClass.Get();
			}

			UE_LOG(
				ARLog,
				Error,
				TEXT("[InvaderGameMode] Missing/invalid invader pawn class for ship '%s' (expected ship row field 'InvaderPawnClass' or 'DummyPawnClass')."),
				*ShipTag.ToString());
			return nullptr;
		}
	}

	UE_LOG(
		ARLog,
		Error,
		TEXT("[InvaderGameMode] Could not resolve ship loadout tag for controller '%s'; invader pawn spawn aborted."),
		*GetNameSafe(InController));

	return nullptr;
}

FProperty* AARInvaderGameMode::FindPropertyByNamePrefix(const UScriptStruct* StructType, const FString& Prefix)
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

bool AARInvaderGameMode::ResolveInvaderPawnClassFromShipTag(const FGameplayTag ShipTag, TSubclassOf<APawn>& OutPawnClass) const
{
	OutPawnClass = nullptr;
	if (!ShipTag.IsValid())
	{
		return false;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UTagKeySubsystem* Resolver = GameInstance ? GameInstance->GetSubsystem<UTagKeySubsystem>() : nullptr;
	if (!Resolver)
	{
		return false;
	}

	FInstancedStruct ShipRow;
	FString ResolveError;
	if (!Resolver->TryResolveRowStructForTag(ShipTag, ShipRow, ResolveError))
	{
		UE_LOG(
			ARLog,
			Warning,
			TEXT("[InvaderGameMode] Failed resolving ship row '%s': %s"),
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

	static const TCHAR* PawnClassPrefixes[] = {
		TEXT("InvaderPawnClass"),
		TEXT("DummyPawnClass")
	};

	FProperty* PawnClassProperty = nullptr;
	for (const TCHAR* Prefix : PawnClassPrefixes)
	{
		PawnClassProperty = FindPropertyByNamePrefix(StructType, Prefix);
		if (PawnClassProperty)
		{
			break;
		}
	}

	if (!PawnClassProperty)
	{
		UE_LOG(
			ARLog,
			Error,
			TEXT("[InvaderGameMode] Ship row '%s' missing pawn-class field. Expected prefixes: InvaderPawnClass / DummyPawnClass (Struct=%s)."),
			*ShipTag.ToString(),
			*GetNameSafe(StructType));
		return false;
	}

	if (const FClassProperty* ClassProperty = CastField<FClassProperty>(PawnClassProperty))
	{
		if (UClass* PawnClass = Cast<UClass>(ClassProperty->GetPropertyValue_InContainer(StructData)))
		{
			OutPawnClass = PawnClass;
			return OutPawnClass != nullptr;
		}

		UE_LOG(
			ARLog,
			Error,
			TEXT("[InvaderGameMode] Ship row '%s' field '%s' resolved as null hard class."),
			*ShipTag.ToString(),
			*PawnClassProperty->GetName());
	}
	else if (const FSoftClassProperty* SoftClassProperty = CastField<FSoftClassProperty>(PawnClassProperty))
	{
		const FSoftObjectPtr SoftClassPtr = SoftClassProperty->GetPropertyValue_InContainer(StructData);
		if (UClass* PawnClass = Cast<UClass>(SoftClassPtr.LoadSynchronous()))
		{
			OutPawnClass = PawnClass;
			return OutPawnClass != nullptr;
		}

		UE_LOG(
			ARLog,
			Error,
			TEXT("[InvaderGameMode] Ship row '%s' field '%s' soft class failed to load (Path=%s)."),
			*ShipTag.ToString(),
			*PawnClassProperty->GetName(),
			*SoftClassPtr.ToString());
	}
	else
	{
		UE_LOG(
			ARLog,
			Error,
			TEXT("[InvaderGameMode] Ship row '%s' pawn field '%s' has unsupported property type '%s'."),
			*ShipTag.ToString(),
			*PawnClassProperty->GetName(),
			*PawnClassProperty->GetClass()->GetName());
	}

	return false;
}

bool AARInvaderGameMode::FindFirstTagUnderRoot(const FGameplayTagContainer& InTags, const FGameplayTag& RootTag, FGameplayTag& OutTag)
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
