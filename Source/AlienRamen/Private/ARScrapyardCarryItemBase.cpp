#include "ARScrapyardCarryItemBase.h"

#include "ARLog.h"
#include "ARScrapyardPlayerController.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

AARScrapyardCarryItemBase::AARScrapyardCarryItemBase()
{
}

void AARScrapyardCarryItemBase::ForwardUseToController(AActor* UsingActor)
{
	AARScrapyardPlayerController* UsingController = Cast<AARScrapyardPlayerController>(UsingActor);
	if (!UsingController)
	{
		const APawn* UsingPawn = Cast<APawn>(UsingActor);
		UsingController = UsingPawn ? Cast<AARScrapyardPlayerController>(UsingPawn->GetController()) : nullptr;
	}

	if (!UsingController)
	{
		UE_LOG(
			ARLog,
			Warning,
			TEXT("[Scrapyard|Carry] '%s' use-forward ignored: could not resolve AARScrapyardPlayerController from '%s'."),
			*GetNameSafe(this),
			*GetNameSafe(UsingActor));
		return;
	}

	UsingController->RequestScrapyardPickupCarryItem(this);
}

void AARScrapyardCarryItemBase::SetScrapyardItemTag(const FGameplayTag NewItemTag)
{
	if (!HasAuthority())
	{
		return;
	}

	if (ScrapyardItemTag == NewItemTag)
	{
		return;
	}

	ScrapyardItemTag = NewItemTag;
	ForceNetUpdate();
}

void AARScrapyardCarryItemBase::SetFallbackScrapCost(const int32 NewFallbackCost)
{
	if (!HasAuthority())
	{
		return;
	}

	const int32 SanitizedCost = FMath::Max(0, NewFallbackCost);
	if (FallbackScrapCost == SanitizedCost)
	{
		return;
	}

	FallbackScrapCost = SanitizedCost;
	ForceNetUpdate();
}

void AARScrapyardCarryItemBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AARScrapyardCarryItemBase, ScrapyardItemTag);
	DOREPLIFETIME(AARScrapyardCarryItemBase, FallbackScrapCost);
}

