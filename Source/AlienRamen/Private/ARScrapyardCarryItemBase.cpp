#include "ARScrapyardCarryItemBase.h"

#include "ARLog.h"
#include "ARScrapyardPlayerController.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

AARScrapyardCarryItemBase::AARScrapyardCarryItemBase()
{
}

void AARScrapyardCarryItemBase::BeginPlay()
{
	Super::BeginPlay();
	RefreshVisualModelActor();
}

void AARScrapyardCarryItemBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DestroyVisualModelActor();
	Super::EndPlay(EndPlayReason);
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
	DOREPLIFETIME(AARScrapyardCarryItemBase, VisualModelClass);
}

void AARScrapyardCarryItemBase::SetVisualModelClass(TSoftClassPtr<AActor> NewVisualModelClass)
{
	if (!HasAuthority())
	{
		return;
	}

	if (VisualModelClass == NewVisualModelClass)
	{
		return;
	}

	VisualModelClass = NewVisualModelClass;
	RefreshVisualModelActor();
	ForceNetUpdate();
}

void AARScrapyardCarryItemBase::OnRep_VisualModelClass()
{
	RefreshVisualModelActor();
}

void AARScrapyardCarryItemBase::RefreshVisualModelActor()
{
	DestroyVisualModelActor();

	if (VisualModelClass.IsNull())
	{
		return;
	}

	UClass* LoadedClass = VisualModelClass.LoadSynchronous();
	if (!LoadedClass)
	{
		UE_LOG(
			ARLog,
			Warning,
			TEXT("[Scrapyard|Carry] '%s' RefreshVisualModelActor: failed to load VisualModelClass '%s'."),
			*GetNameSafe(this),
			*VisualModelClass.ToString());
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnedVisualModelActor = World->SpawnActor<AActor>(LoadedClass, GetActorTransform(), SpawnParams);
	if (SpawnedVisualModelActor)
	{
		SpawnedVisualModelActor->AttachToActor(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}
}

void AARScrapyardCarryItemBase::DestroyVisualModelActor()
{
	if (SpawnedVisualModelActor)
	{
		SpawnedVisualModelActor->Destroy();
		SpawnedVisualModelActor = nullptr;
	}
}

