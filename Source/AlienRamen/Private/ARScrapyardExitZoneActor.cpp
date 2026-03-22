#include "ARScrapyardExitZoneActor.h"

#include "ARPlayerStateBase.h"
#include "ARCarryItemBase.h"
#include "ARScrapyardGameState.h"
#include "ARScrapyardPlayerController.h"
#include "ARShopCarryComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

AARScrapyardExitZoneActor::AARScrapyardExitZoneActor()
{
	bReplicates = true;

	ExitVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("ExitVolume"));
	SetRootComponent(ExitVolume);
	ExitVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ExitVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	ExitVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	ExitVolume->SetGenerateOverlapEvents(true);
}

TArray<AARCarryItemBase*> AARScrapyardExitZoneActor::GetDepositedItems() const
{
	TArray<AARCarryItemBase*> Result;
	Result.Reserve(DepositedItems.Num());
	for (const TObjectPtr<AARCarryItemBase>& Item : DepositedItems)
	{
		if (Item)
		{
			Result.Add(Item.Get());
		}
	}

	return Result;
}

const TArray<TObjectPtr<AARCarryItemBase>>& AARScrapyardExitZoneActor::GetDepositedItemsRef() const
{
	return DepositedItems;
}

void AARScrapyardExitZoneActor::BeginPlay()
{
	Super::BeginPlay();

	if (ExitVolume)
	{
		ExitVolume->OnComponentBeginOverlap.AddUniqueDynamic(this, &AARScrapyardExitZoneActor::HandleExitVolumeBeginOverlap);
		ExitVolume->OnComponentEndOverlap.AddUniqueDynamic(this, &AARScrapyardExitZoneActor::HandleExitVolumeEndOverlap);
	}

	if (HasAuthority())
	{
		if (AARScrapyardGameState* ScrapyardGameState = ResolveScrapyardGameState())
		{
			ScrapyardGameState->RegisterExitZone(this);
		}

		RefreshDepositedReservedScrapValue(false);
	}
}

void AARScrapyardExitZoneActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		if (AARScrapyardGameState* ScrapyardGameState = ResolveScrapyardGameState())
		{
			ScrapyardGameState->UnregisterExitZone(this);
		}
	}

	if (ExitVolume)
	{
		ExitVolume->OnComponentBeginOverlap.RemoveDynamic(this, &AARScrapyardExitZoneActor::HandleExitVolumeBeginOverlap);
		ExitVolume->OnComponentEndOverlap.RemoveDynamic(this, &AARScrapyardExitZoneActor::HandleExitVolumeEndOverlap);
	}

	Super::EndPlay(EndPlayReason);
}

bool AARScrapyardExitZoneActor::TryDepositHeldItem(AARScrapyardPlayerController* Controller)
{
	if (!HasAuthority() || !Controller || !IsControllerEligibleForExit(Controller))
	{
		return false;
	}

	APawn* Pawn = Controller->GetPawn();
	UARShopCarryComponent* CarryComponent = Pawn ? Pawn->FindComponentByClass<UARShopCarryComponent>() : nullptr;
	AARCarryItemBase* HeldItem = CarryComponent ? Cast<AARCarryItemBase>(CarryComponent->GetHeldActor()) : nullptr;
	if (!CarryComponent || !HeldItem)
	{
		return false;
	}

	PruneInvalidDeposits();
	if (DepositedItems.Contains(HeldItem))
	{
		return false;
	}

	AARScrapyardGameState* ScrapyardGameState = ResolveScrapyardGameState();
	if (!ScrapyardGameState)
	{
		return false;
	}

	int32 ReservedCost = 0;
	if (!ScrapyardGameState->ReserveScrapForItem(HeldItem, ReservedCost))
	{
		return false;
	}

	CarryComponent->ReleaseHeldActorForTransfer();
	if (USceneComponent* ItemRoot = HeldItem->GetRootComponent())
	{
		ItemRoot->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	}

	const FVector DepositLocation = GetActorTransform().TransformPosition(DepositOffset);
	HeldItem->SetActorLocation(DepositLocation);

	if (UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(HeldItem->GetRootComponent()))
	{
		if (RootPrimitive->IsSimulatingPhysics())
		{
			RootPrimitive->SetSimulatePhysics(false);
		}
		RootPrimitive->SetEnableGravity(false);
		RootPrimitive->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}

	HeldItem->SetActorEnableCollision(true);
	DepositedItems.Add(HeldItem);
	RefreshDepositedReservedScrapValue(true);
	return true;
}

bool AARScrapyardExitZoneActor::TryWithdrawDepositedItem(AARScrapyardPlayerController* Controller, AARCarryItemBase* ItemActor)
{
	if (!HasAuthority() || !Controller || !ItemActor || !IsControllerEligibleForExit(Controller))
	{
		return false;
	}

	PruneInvalidDeposits();
	if (!DepositedItems.Contains(ItemActor))
	{
		return false;
	}

	APawn* Pawn = Controller->GetPawn();
	UARShopCarryComponent* CarryComponent = Pawn ? Pawn->FindComponentByClass<UARShopCarryComponent>() : nullptr;
	if (!CarryComponent || CarryComponent->HasHeldActor())
	{
		return false;
	}

	AARScrapyardGameState* ScrapyardGameState = ResolveScrapyardGameState();
	if (!ScrapyardGameState)
	{
		return false;
	}

	int32 RefundCost = 0;
	if (!ScrapyardGameState->RefundScrapForItem(ItemActor, RefundCost))
	{
		return false;
	}

	DepositedItems.RemoveSwap(ItemActor);
	if (!CarryComponent->TrySetHeldActor(ItemActor))
	{
		int32 RereservedCost = 0;
		ScrapyardGameState->ReserveScrapForItem(ItemActor, RereservedCost);
		DepositedItems.Add(ItemActor);
		RefreshDepositedReservedScrapValue(true);
		return false;
	}

	RefreshDepositedReservedScrapValue(true);
	return true;
}

bool AARScrapyardExitZoneActor::IsPlayerStateInsideExit(const AARPlayerStateBase* PlayerState) const
{
	return PlayerState && PlayerStatesInZone.Contains(PlayerState);
}

void AARScrapyardExitZoneActor::GatherHeldItemsInZone(TArray<AARCarryItemBase*>& OutHeldItems) const
{
	OutHeldItems.Reset();

	for (const TWeakObjectPtr<AARPlayerStateBase>& PlayerStateWeak : PlayerStatesInZone)
	{
		const AARPlayerStateBase* PlayerState = PlayerStateWeak.Get();
		APawn* Pawn = PlayerState ? PlayerState->GetPawn() : nullptr;
		const UARShopCarryComponent* CarryComponent = Pawn ? Pawn->FindComponentByClass<UARShopCarryComponent>() : nullptr;
		AARCarryItemBase* HeldItem = CarryComponent ? Cast<AARCarryItemBase>(CarryComponent->GetHeldActor()) : nullptr;
		if (HeldItem)
		{
			OutHeldItems.AddUnique(HeldItem);
		}
	}
}

void AARScrapyardExitZoneActor::HandleExitVolumeBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	(void)OverlappedComponent;
	(void)OtherComp;
	(void)OtherBodyIndex;
	(void)bFromSweep;
	(void)SweepResult;

	if (!HasAuthority())
	{
		return;
	}

	const APawn* Pawn = Cast<APawn>(OtherActor);
	AARPlayerStateBase* PlayerState = Pawn ? Pawn->GetPlayerState<AARPlayerStateBase>() : nullptr;
	if (!PlayerState)
	{
		return;
	}

	PlayerStatesInZone.Add(PlayerState);
	OnExitZoneChanged.Broadcast();
}

void AARScrapyardExitZoneActor::HandleExitVolumeEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	(void)OverlappedComponent;
	(void)OtherComp;
	(void)OtherBodyIndex;

	if (!HasAuthority())
	{
		return;
	}

	const APawn* Pawn = Cast<APawn>(OtherActor);
	AARPlayerStateBase* PlayerState = Pawn ? Pawn->GetPlayerState<AARPlayerStateBase>() : nullptr;
	if (!PlayerState)
	{
		return;
	}

	PlayerStatesInZone.Remove(PlayerState);
	OnExitZoneChanged.Broadcast();
}

AARScrapyardGameState* AARScrapyardExitZoneActor::ResolveScrapyardGameState() const
{
	UWorld* World = GetWorld();
	return World ? World->GetGameState<AARScrapyardGameState>() : nullptr;
}

bool AARScrapyardExitZoneActor::IsControllerEligibleForExit(const AARScrapyardPlayerController* Controller) const
{
	if (!Controller)
	{
		return false;
	}

	const AARPlayerStateBase* PlayerState = Controller->GetPlayerState<AARPlayerStateBase>();
	return PlayerState && PlayerStatesInZone.Contains(PlayerState);
}

void AARScrapyardExitZoneActor::PruneInvalidDeposits()
{
	bool bRemovedAny = false;
	for (int32 Index = DepositedItems.Num() - 1; Index >= 0; --Index)
	{
		if (!IsValid(DepositedItems[Index]))
		{
			DepositedItems.RemoveAtSwap(Index);
			bRemovedAny = true;
		}
	}

	if (bRemovedAny)
	{
		RefreshDepositedReservedScrapValue(true);
	}
}

void AARScrapyardExitZoneActor::RefreshDepositedReservedScrapValue(const bool bBroadcast)
{
	if (!HasAuthority())
	{
		return;
	}

	int32 NewReservedValue = 0;
	AARScrapyardGameState* ScrapyardGameState = ResolveScrapyardGameState();
	for (AARCarryItemBase* DepositedItem : DepositedItems)
	{
		if (!DepositedItem)
		{
			continue;
		}

		int32 ReservedCost = 0;
		if (ScrapyardGameState && ScrapyardGameState->TryGetReservedScrapForItem(DepositedItem, ReservedCost))
		{
			NewReservedValue += ReservedCost;
		}
		else if (ScrapyardGameState)
		{
			NewReservedValue += ScrapyardGameState->ResolveItemCostForActor(DepositedItem);
		}
	}

	if (DepositedReservedScrapValue == NewReservedValue)
	{
		if (bBroadcast)
		{
			OnExitZoneChanged.Broadcast();
		}
		return;
	}

	const int32 OldValue = DepositedReservedScrapValue;
	DepositedReservedScrapValue = NewReservedValue;
	if (bBroadcast)
	{
		OnRep_DepositedReservedScrapValue(OldValue);
	}

	ForceNetUpdate();
}

void AARScrapyardExitZoneActor::OnRep_DepositedReservedScrapValue(int32 OldValue)
{
	(void)OldValue;
	OnExitZoneChanged.Broadcast();
}

void AARScrapyardExitZoneActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AARScrapyardExitZoneActor, DepositedReservedScrapValue);
}
