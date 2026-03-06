#include "ARNPCCharacterBase.h"

#include "ARDialogueSubsystem.h"
#include "ARLog.h"
#include "ARNPCSubsystem.h"
#include "ARPlayerController.h"
#include "Net/UnrealNetwork.h"

AARNPCCharacterBase::AARNPCCharacterBase()
{
	bReplicates = true;
}

void AARNPCCharacterBase::BeginPlay()
{
	Super::BeginPlay();
	RefreshTalkableFromSubsystem();

	if (UARNPCSubsystem* NpcSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UARNPCSubsystem>() : nullptr)
	{
		NpcSubsystem->OnNpcTalkableChanged.AddDynamic(this, &AARNPCCharacterBase::HandleNpcTalkableChanged);
	}
}

void AARNPCCharacterBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UARNPCSubsystem* NpcSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UARNPCSubsystem>() : nullptr)
	{
		NpcSubsystem->OnNpcTalkableChanged.RemoveDynamic(this, &AARNPCCharacterBase::HandleNpcTalkableChanged);
	}

	Super::EndPlay(EndPlayReason);
}

void AARNPCCharacterBase::InteractByController(AARPlayerController* InteractingController)
{
	if (!HasAuthority() || !InteractingController)
	{
		return;
	}

	if (!NpcTag.IsValid())
	{
		UE_LOG(ARLog, Warning, TEXT("[NPC] Interact ignored: '%s' has no NpcTag."), *GetNameSafe(this));
		return;
	}

	if (UARDialogueSubsystem* DialogueSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UARDialogueSubsystem>() : nullptr)
	{
		if (!DialogueSubsystem->TryStartDialogueWithNpc(InteractingController, NpcTag))
		{
			UE_LOG(ARLog, Verbose, TEXT("[NPC] TryStartDialogueWithNpc returned false for '%s' with NPC '%s'."), *GetNameSafe(InteractingController), *NpcTag.ToString());
		}
	}
}

void AARNPCCharacterBase::RefreshTalkableFromSubsystem()
{
	if (!HasAuthority() || !NpcTag.IsValid())
	{
		return;
	}

	if (UARNPCSubsystem* NpcSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UARNPCSubsystem>() : nullptr)
	{
		NpcSubsystem->RefreshNpcTalkableState(NpcTag);
		const bool bNewTalkable = NpcSubsystem->IsNpcTalkable(NpcTag);
		if (bIsTalkable != bNewTalkable)
		{
			const bool bOld = bIsTalkable;
			bIsTalkable = bNewTalkable;
			OnRep_IsTalkable(bOld);
			ForceNetUpdate();
		}
	}
}

void AARNPCCharacterBase::HandleNpcTalkableChanged(FGameplayTag ChangedNpcTag, bool bNewTalkable)
{
	if (!HasAuthority() || !ChangedNpcTag.MatchesTagExact(NpcTag))
	{
		return;
	}

	if (bIsTalkable == bNewTalkable)
	{
		return;
	}

	const bool bOld = bIsTalkable;
	bIsTalkable = bNewTalkable;
	OnRep_IsTalkable(bOld);
	ForceNetUpdate();
}

void AARNPCCharacterBase::OnRep_IsTalkable(bool bOldTalkable)
{
	if (bIsTalkable != bOldTalkable)
	{
		OnNpcTalkableStateChanged.Broadcast(bIsTalkable);
	}
}

void AARNPCCharacterBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AARNPCCharacterBase, bIsTalkable);
}
