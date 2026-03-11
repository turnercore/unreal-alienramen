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

namespace
{
	static const FName TalkableStateEmotionSourceId(TEXT("TalkableState"));
}

AARNPCCharacterBase::AARNPCCharacterBase()
{
	bReplicates = true;
	SpeakerComponent = CreateDefaultSubobject<UARSpeakerComponent>(TEXT("SpeakerComponent"));
	EmotionComponent = CreateDefaultSubobject<UAREmotionComponent>(TEXT("EmotionComponent"));
	CustomerComponent = CreateDefaultSubobject<UARCustomerComponent>(TEXT("CustomerComponent"));
}

void AARNPCCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	TArray<UARSpeakerComponent*> TalkComponents;
	GetComponents(TalkComponents);
	if (!TalkComponents.IsEmpty())
	{
		UARSpeakerComponent* PreferredTalkComponent = SpeakerComponent;
		if (!PreferredTalkComponent || !TalkComponents.Contains(PreferredTalkComponent))
		{
			PreferredTalkComponent = TalkComponents[0];
		}

		if (PreferredTalkComponent && !PreferredTalkComponent->GetSpeakerTag().IsValid())
		{
			for (UARSpeakerComponent* Candidate : TalkComponents)
			{
				if (Candidate && Candidate->GetSpeakerTag().IsValid())
				{
					PreferredTalkComponent = Candidate;
					break;
				}
			}
		}

		SpeakerComponent = PreferredTalkComponent;
		if (TalkComponents.Num() > 1)
		{
			UE_LOG(
				ARLog,
				Warning,
				TEXT("[Speaker] '%s' has %d UARSpeakerComponent instances. Using '%s' as canonical."),
				*GetNameSafe(this),
				TalkComponents.Num(),
				*GetNameSafe(SpeakerComponent));
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

	if (SpeakerComponent)
	{
		SpeakerComponent->OnSpeakerTalkableStateChanged.RemoveDynamic(this, &AARNPCCharacterBase::HandleSpeakerComponentTalkableStateChanged);
		SpeakerComponent->OnSpeakerTalkableStateChanged.AddDynamic(this, &AARNPCCharacterBase::HandleSpeakerComponentTalkableStateChanged);

		if (HasAuthority() && EmotionComponent)
		{
			EmotionComponent->SetRegisteredSpeakerTag(SpeakerComponent->GetSpeakerTag());
		}
	}

	RefreshAutoWantsToTalkEmotion(IsTalkable());
}

void AARNPCCharacterBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (SpeakerComponent)
	{
		SpeakerComponent->OnSpeakerTalkableStateChanged.RemoveDynamic(this, &AARNPCCharacterBase::HandleSpeakerComponentTalkableStateChanged);
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

	if (!bSpeakerLocalStateAllowsDialogue)
	{
		UE_LOG(ARLog, Verbose, TEXT("[Speaker] Interact ignored for '%s': local state blocks dialogue."), *GetNameSafe(this));
		return;
	}

	if (SpeakerComponent)
	{
		SpeakerComponent->InteractByController(InteractingController);
	}
}

FGameplayTag AARNPCCharacterBase::GetSpeakerTag() const
{
	return SpeakerComponent ? SpeakerComponent->GetSpeakerTag() : FGameplayTag();
}

bool AARNPCCharacterBase::IsTalkable() const
{
	const bool bHasActiveCustomerOrder = CustomerComponent && CustomerComponent->HasActiveOrder();
	return bHasActiveCustomerOrder || (bSpeakerLocalStateAllowsDialogue && SpeakerComponent && SpeakerComponent->IsTalkable());
}

bool AARNPCCharacterBase::IsTalkableForPlayerSlot(const EARPlayerSlot PlayerSlot) const
{
	const bool bHasActiveCustomerOrder = CustomerComponent && CustomerComponent->HasActiveOrder();
	return bHasActiveCustomerOrder || (bSpeakerLocalStateAllowsDialogue && SpeakerComponent && SpeakerComponent->IsTalkableForPlayerSlot(PlayerSlot));
}

bool AARNPCCharacterBase::IsTalkableForController(const AARPlayerController* QueryController) const
{
	const bool bHasActiveCustomerOrder = CustomerComponent && CustomerComponent->HasActiveOrder();
	return bHasActiveCustomerOrder || (bSpeakerLocalStateAllowsDialogue && SpeakerComponent && SpeakerComponent->IsTalkableForController(QueryController));
}

bool AARNPCCharacterBase::IsSpeakerBusyForController(const AARPlayerController* QueryController) const
{
	if (!QueryController || !SpeakerComponent)
	{
		return false;
	}

	const FGameplayTag SpeakerTag = SpeakerComponent->GetSpeakerTag();
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

bool AARNPCCharacterBase::IsSpeakerLocalStateAllowingDialogue() const
{
	return bSpeakerLocalStateAllowsDialogue;
}

void AARNPCCharacterBase::SetSpeakerLocalStateAllowsDialogue(const bool bEnabled)
{
	if (!HasAuthority() || bSpeakerLocalStateAllowsDialogue == bEnabled)
	{
		return;
	}

	const bool bOldAllowsDialogue = bSpeakerLocalStateAllowsDialogue;
	bSpeakerLocalStateAllowsDialogue = bEnabled;
	OnRep_SpeakerLocalStateAllowsDialogue(bOldAllowsDialogue);
	ForceNetUpdate();
}

void AARNPCCharacterBase::HandleSpeakerComponentTalkableStateChanged(const bool bNewTalkable)
{
	const bool bHasActiveCustomerOrder = CustomerComponent && CustomerComponent->HasActiveOrder();
	const bool bEffectiveTalkable = bHasActiveCustomerOrder || (bSpeakerLocalStateAllowsDialogue && bNewTalkable);
	RefreshAutoWantsToTalkEmotion(bEffectiveTalkable);
	OnSpeakerTalkableStateChanged.Broadcast(bEffectiveTalkable);
}

void AARNPCCharacterBase::OnRep_SpeakerLocalStateAllowsDialogue(const bool bOldAllowsDialogue)
{
	if (bSpeakerLocalStateAllowsDialogue == bOldAllowsDialogue)
	{
		return;
	}

	RefreshAutoWantsToTalkEmotion(IsTalkable());
	OnSpeakerTalkableStateChanged.Broadcast(IsTalkable());
}

void AARNPCCharacterBase::RefreshTalkableFromSubsystem()
{
	if (SpeakerComponent)
	{
		SpeakerComponent->RefreshTalkableFromSubsystem();
	}
}

void AARNPCCharacterBase::RefreshAutoWantsToTalkEmotion(const bool bEffectiveTalkable)
{
	if (!HasAuthority() || !EmotionComponent)
	{
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[Emotion][Talkable] Skip auto wants-to-talk for '%s': Authority=%s EmotionComponent=%s"),
			*GetNameSafe(this),
			HasAuthority() ? TEXT("true") : TEXT("false"),
			EmotionComponent ? TEXT("valid") : TEXT("null"));
		return;
	}

	const UAREmotionSettings* EmotionSettings = GetDefault<UAREmotionSettings>();
	const FGameplayTag WantsToTalkTag = EmotionSettings ? EmotionSettings->WantsToTalkEmotionTag : FGameplayTag();
	if (!WantsToTalkTag.IsValid())
	{
		EmotionComponent->ClearSystemEmotionTag(TalkableStateEmotionSourceId);
		bAutoWantsToTalkEmotionApplied = false;
		UE_LOG(ARLog, Verbose, TEXT("[Emotion][Talkable] '%s' no WantsToTalkEmotionTag configured; cleared TalkableState source."), *GetNameSafe(this));
		return;
	}

	if (bEffectiveTalkable)
	{
		EmotionComponent->SetSystemEmotionTag(TalkableStateEmotionSourceId, WantsToTalkTag, 0);
		bAutoWantsToTalkEmotionApplied = true;
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[Emotion][Talkable] '%s' apply TalkableState=%s (effective talkable=true)."),
			*GetNameSafe(this),
			*WantsToTalkTag.ToString());
		return;
	}

	EmotionComponent->ClearSystemEmotionTag(TalkableStateEmotionSourceId);
	UE_LOG(ARLog, Verbose, TEXT("[Emotion][Talkable] '%s' cleared TalkableState (effective talkable=false)."), *GetNameSafe(this));
	bAutoWantsToTalkEmotionApplied = false;
}

void AARNPCCharacterBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AARNPCCharacterBase, bSpeakerLocalStateAllowsDialogue);
}
