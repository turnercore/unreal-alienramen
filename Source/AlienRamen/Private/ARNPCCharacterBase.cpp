#include "ARNPCCharacterBase.h"

#include "ARCustomerComponent.h"
#include "AREmotionComponent.h"
#include "ARLog.h"
#include "ARPlayerController.h"
#include "Net/UnrealNetwork.h"

AARNPCCharacterBase::AARNPCCharacterBase()
{
	bReplicates = true;
	NpcTalkComponent = CreateDefaultSubobject<UARNPCTalkComponent>(TEXT("NpcTalkComponent"));
	EmotionComponent = CreateDefaultSubobject<UAREmotionComponent>(TEXT("EmotionComponent"));
	CustomerComponent = CreateDefaultSubobject<UARCustomerComponent>(TEXT("CustomerComponent"));
}

void AARNPCCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	if (NpcTalkComponent)
	{
		// Runtime migration path: if legacy actor fields were authored, hydrate the component once.
		if (!NpcTalkComponent->GetNpcTag().IsValid() && NpcTag.IsValid())
		{
			NpcTalkComponent->SetNpcTag(NpcTag);
		}

		NpcTalkComponent->OnNpcTalkableStateChanged.RemoveDynamic(this, &AARNPCCharacterBase::HandleTalkComponentTalkableStateChanged);
		NpcTalkComponent->OnNpcTalkableStateChanged.AddDynamic(this, &AARNPCCharacterBase::HandleTalkComponentTalkableStateChanged);

		if (HasAuthority() && EmotionComponent)
		{
			EmotionComponent->SetRegisteredSpeakerTag(NpcTalkComponent->GetNpcTag());
		}
	}
}

void AARNPCCharacterBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (NpcTalkComponent)
	{
		NpcTalkComponent->OnNpcTalkableStateChanged.RemoveDynamic(this, &AARNPCCharacterBase::HandleTalkComponentTalkableStateChanged);
	}

	Super::EndPlay(EndPlayReason);
}

void AARNPCCharacterBase::InteractByController(AARPlayerController* InteractingController)
{
	if (HasAuthority() && CustomerComponent && InteractingController)
	{
		FARRamenServeResult ServeResult;
		if (CustomerComponent->TryServeHeldBowlFromController(InteractingController, ServeResult))
		{
			return;
		}
	}

	if (!bNpcLocalStateAllowsDialogue)
	{
		UE_LOG(ARLog, Verbose, TEXT("[NPC] Interact ignored for '%s': local state blocks dialogue."), *GetNameSafe(this));
		return;
	}

	if (NpcTalkComponent)
	{
		NpcTalkComponent->InteractByController(InteractingController);
	}
}

FGameplayTag AARNPCCharacterBase::GetNpcTag() const
{
	return NpcTalkComponent ? NpcTalkComponent->GetNpcTag() : FGameplayTag();
}

bool AARNPCCharacterBase::IsTalkable() const
{
	const bool bHasActiveCustomerOrder = CustomerComponent && CustomerComponent->HasActiveOrder();
	return bHasActiveCustomerOrder || (bNpcLocalStateAllowsDialogue && NpcTalkComponent && NpcTalkComponent->IsTalkable());
}

bool AARNPCCharacterBase::IsTalkableForPlayerSlot(const EARPlayerSlot PlayerSlot) const
{
	const bool bHasActiveCustomerOrder = CustomerComponent && CustomerComponent->HasActiveOrder();
	return bHasActiveCustomerOrder || (bNpcLocalStateAllowsDialogue && NpcTalkComponent && NpcTalkComponent->IsTalkableForPlayerSlot(PlayerSlot));
}

bool AARNPCCharacterBase::IsTalkableForController(const AARPlayerController* QueryController) const
{
	const bool bHasActiveCustomerOrder = CustomerComponent && CustomerComponent->HasActiveOrder();
	return bHasActiveCustomerOrder || (bNpcLocalStateAllowsDialogue && NpcTalkComponent && NpcTalkComponent->IsTalkableForController(QueryController));
}

bool AARNPCCharacterBase::IsNpcLocalStateAllowingDialogue() const
{
	return bNpcLocalStateAllowsDialogue;
}

void AARNPCCharacterBase::SetNpcLocalStateAllowsDialogue(const bool bEnabled)
{
	if (!HasAuthority() || bNpcLocalStateAllowsDialogue == bEnabled)
	{
		return;
	}

	const bool bOldAllowsDialogue = bNpcLocalStateAllowsDialogue;
	bNpcLocalStateAllowsDialogue = bEnabled;
	OnRep_NpcLocalStateAllowsDialogue(bOldAllowsDialogue);
	ForceNetUpdate();
}

void AARNPCCharacterBase::HandleTalkComponentTalkableStateChanged(const bool bNewTalkable)
{
	const bool bHasActiveCustomerOrder = CustomerComponent && CustomerComponent->HasActiveOrder();
	const bool bEffectiveTalkable = bHasActiveCustomerOrder || (bNpcLocalStateAllowsDialogue && bNewTalkable);
	OnNpcTalkableStateChanged.Broadcast(bEffectiveTalkable);
}

void AARNPCCharacterBase::OnRep_NpcLocalStateAllowsDialogue(const bool bOldAllowsDialogue)
{
	if (bNpcLocalStateAllowsDialogue == bOldAllowsDialogue)
	{
		return;
	}

	OnNpcTalkableStateChanged.Broadcast(IsTalkable());
}

void AARNPCCharacterBase::RefreshTalkableFromSubsystem()
{
	if (NpcTalkComponent)
	{
		NpcTalkComponent->RefreshTalkableFromSubsystem();
	}
}

void AARNPCCharacterBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AARNPCCharacterBase, bNpcLocalStateAllowsDialogue);
}
