#include "ARNPCCharacterBase.h"

#include "ARCustomerComponent.h"
#include "ARDialogueSubsystem.h"
#include "AREmotionComponent.h"
#include "AREmotionSettings.h"
#include "ARLog.h"
#include "ARPlayerController.h"
#include "Components/ActorComponent.h"
#include "Engine/GameInstance.h"
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

	TArray<UARNPCTalkComponent*> TalkComponents;
	GetComponents(TalkComponents);
	if (!TalkComponents.IsEmpty())
	{
		UARNPCTalkComponent* PreferredTalkComponent = NpcTalkComponent;
		if (!PreferredTalkComponent || !TalkComponents.Contains(PreferredTalkComponent))
		{
			PreferredTalkComponent = TalkComponents[0];
		}

		if (PreferredTalkComponent && !PreferredTalkComponent->GetNpcTag().IsValid())
		{
			for (UARNPCTalkComponent* Candidate : TalkComponents)
			{
				if (Candidate && Candidate->GetNpcTag().IsValid())
				{
					PreferredTalkComponent = Candidate;
					break;
				}
			}
		}

		NpcTalkComponent = PreferredTalkComponent;
		if (TalkComponents.Num() > 1)
		{
			UE_LOG(
				ARLog,
				Warning,
				TEXT("[Speaker] '%s' has %d UARNPCTalkComponent instances. Using '%s' as canonical."),
				*GetNameSafe(this),
				TalkComponents.Num(),
				*GetNameSafe(NpcTalkComponent));
		}
	}

	TArray<UAREmotionComponent*> EmotionComponents;
	GetComponents(EmotionComponents);
	if (!EmotionComponents.IsEmpty())
	{
		UAREmotionComponent* PreferredEmotionComponent = EmotionComponent;
		if (!PreferredEmotionComponent || !EmotionComponents.Contains(PreferredEmotionComponent))
		{
			PreferredEmotionComponent = EmotionComponents[0];
		}

		EmotionComponent = PreferredEmotionComponent;
		if (EmotionComponents.Num() > 1)
		{
			UE_LOG(
				ARLog,
				Warning,
				TEXT("[Speaker] '%s' has %d UAREmotionComponent instances. Using '%s' as canonical."),
				*GetNameSafe(this),
				EmotionComponents.Num(),
				*GetNameSafe(EmotionComponent));
		}
	}

	TArray<UARCustomerComponent*> CustomerComponents;
	GetComponents(CustomerComponents);
	if (!CustomerComponents.IsEmpty())
	{
		UARCustomerComponent* PreferredCustomerComponent = CustomerComponent;
		if (!PreferredCustomerComponent || !CustomerComponents.Contains(PreferredCustomerComponent))
		{
			PreferredCustomerComponent = CustomerComponents[0];
		}

		CustomerComponent = PreferredCustomerComponent;
		if (CustomerComponents.Num() > 1)
		{
			UE_LOG(
				ARLog,
				Warning,
				TEXT("[Speaker] '%s' has %d UARCustomerComponent instances. Using '%s' as canonical."),
				*GetNameSafe(this),
				CustomerComponents.Num(),
				*GetNameSafe(CustomerComponent));
		}
	}

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

	RefreshAutoWantsToTalkEmotion(IsTalkable());
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

bool AARNPCCharacterBase::IsSpeakerBusyForController(const AARPlayerController* QueryController) const
{
	if (!QueryController || !NpcTalkComponent)
	{
		return false;
	}

	const FGameplayTag SpeakerTag = NpcTalkComponent->GetNpcTag();
	if (!SpeakerTag.IsValid())
	{
		return false;
	}

	const UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		return false;
	}

	const UARDialogueSubsystem* DialogueSubsystem = GameInstance->GetSubsystem<UARDialogueSubsystem>();
	return DialogueSubsystem && DialogueSubsystem->IsSpeakerBusyForController(QueryController, SpeakerTag);
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
	RefreshAutoWantsToTalkEmotion(bEffectiveTalkable);
	OnNpcTalkableStateChanged.Broadcast(bEffectiveTalkable);
}

void AARNPCCharacterBase::OnRep_NpcLocalStateAllowsDialogue(const bool bOldAllowsDialogue)
{
	if (bNpcLocalStateAllowsDialogue == bOldAllowsDialogue)
	{
		return;
	}

	RefreshAutoWantsToTalkEmotion(IsTalkable());
	OnNpcTalkableStateChanged.Broadcast(IsTalkable());
}

void AARNPCCharacterBase::RefreshTalkableFromSubsystem()
{
	if (NpcTalkComponent)
	{
		NpcTalkComponent->RefreshTalkableFromSubsystem();
	}
}

void AARNPCCharacterBase::RefreshAutoWantsToTalkEmotion(const bool bEffectiveTalkable)
{
	if (!HasAuthority() || !EmotionComponent)
	{
		return;
	}

	const UAREmotionSettings* EmotionSettings = GetDefault<UAREmotionSettings>();
	const FGameplayTag WantsToTalkTag = EmotionSettings ? EmotionSettings->WantsToTalkEmotionTag : FGameplayTag();
	if (!WantsToTalkTag.IsValid())
	{
		bAutoWantsToTalkEmotionApplied = false;
		return;
	}

	if (bEffectiveTalkable)
	{
		EmotionComponent->SetEmotionTag(WantsToTalkTag);
		bAutoWantsToTalkEmotionApplied = true;
		return;
	}

	if (bAutoWantsToTalkEmotionApplied && EmotionComponent->GetBaseEmotionTag().MatchesTagExact(WantsToTalkTag))
	{
		EmotionComponent->ClearEmotionTag();
	}

	bAutoWantsToTalkEmotionApplied = false;
}

void AARNPCCharacterBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AARNPCCharacterBase, bNpcLocalStateAllowsDialogue);
}
