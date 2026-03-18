#include "ARNPCCharacterBase.h"

#include "ARCustomerComponent.h"
#include "ParleyDialogueSubsystem.h"
#include "ParleySpeakerComponent.h"
#include "EmoComponent.h"
#include "EmoSettings.h"
#include "ARLog.h"
#include "ARPlayerController.h"
#include "Components/ActorComponent.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

namespace
{
	static const FName TalkableStateEmotionSourceId(TEXT("TalkableState"));
	static const FName SpeakerEmotionSourceId(TEXT("ParleySpeakerEmotion"));

	static FString DescribeActorComponentsForDiagnostics(AActor* Actor)
	{
		if (!Actor)
		{
			return TEXT("<no-actor>");
		}

		TArray<UActorComponent*> Components;
		Actor->GetComponents(Components);
		if (Components.IsEmpty())
		{
			return TEXT("<none>");
		}

		FString Summary;
		for (UActorComponent* Component : Components)
		{
			if (!Summary.IsEmpty())
			{
				Summary += TEXT(", ");
			}

			const FString EditorOnlySuffix = (Component && Component->IsEditorOnly()) ? TEXT(",EditorOnly") : TEXT("");
			Summary += FString::Printf(
				TEXT("%s<%s%s>"),
				*GetNameSafe(Component),
				*GetNameSafe(Component ? Component->GetClass() : nullptr),
				*EditorOnlySuffix);
		}

		return Summary;
	}

	static void LogMissingSpeakerComponentDiagnostics(AARNPCCharacterBase* Actor)
	{
		if (!Actor)
		{
			return;
		}

		TArray<UParleySpeakerComponent*> SpeakerComponents;
		Actor->GetComponents(SpeakerComponents);

		UE_LOG(
			ARLog,
			Warning,
			TEXT("[Interact] '%s' missing UParleySpeakerComponent after refresh. FoundTypedSpeakerComponents=%d Components=[%s]"),
			*GetNameSafe(Actor),
			SpeakerComponents.Num(),
			*DescribeActorComponentsForDiagnostics(Actor));
	}
}

AARNPCCharacterBase::AARNPCCharacterBase()
{
	bReplicates = true;
}

void AARNPCCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	ResolveOptionalComponents();
	if (!SpeakerComponent && !CustomerComponent && !EmotionComponent)
	{
		UE_LOG(ARLog, Verbose, TEXT("[Interact] '%s' has no optional speaker/customer/emotion components."), *GetNameSafe(this));
	}

	if (SpeakerComponent && !SpeakerComponent->GetSpeakerTag().IsValid())
	{
		UE_LOG(ARLog, Warning, TEXT("[Speaker] '%s' has a SpeakerComponent but no SpeakerTag configured."), *GetNameSafe(this));
	}

	if (!EmotionComponent && (SpeakerComponent || CustomerComponent))
	{
		UE_LOG(
			ARLog,
			Warning,
			TEXT("[Emotion] '%s' has speaker/customer behavior but no EmotionComponent; overhead emotions will not render."),
			*GetNameSafe(this));
	}

	if (SpeakerComponent)
	{
		SpeakerComponent->OnSpeakerTalkableStateChanged.RemoveDynamic(this, &AARNPCCharacterBase::HandleSpeakerComponentTalkableStateChanged);
		SpeakerComponent->OnSpeakerTalkableStateChanged.AddDynamic(this, &AARNPCCharacterBase::HandleSpeakerComponentTalkableStateChanged);
		SpeakerComponent->OnSpeakerEmotionRequested.RemoveDynamic(this, &AARNPCCharacterBase::HandleSpeakerEmotionRequested);
		SpeakerComponent->OnSpeakerEmotionRequested.AddDynamic(this, &AARNPCCharacterBase::HandleSpeakerEmotionRequested);
		SpeakerComponent->OnSpeakerEmotionCleared.RemoveDynamic(this, &AARNPCCharacterBase::HandleSpeakerEmotionCleared);
		SpeakerComponent->OnSpeakerEmotionCleared.AddDynamic(this, &AARNPCCharacterBase::HandleSpeakerEmotionCleared);

		if (HasAuthority() && EmotionComponent)
		{
			EmotionComponent->SetRegisteredSpeakerTag(SpeakerComponent->GetSpeakerTag());
		}
	}

	if (CustomerComponent)
	{
		CustomerComponent->OnCustomerOrderChanged.RemoveDynamic(this, &AARNPCCharacterBase::HandleCustomerOrderChanged);
		CustomerComponent->OnCustomerOrderChanged.AddDynamic(this, &AARNPCCharacterBase::HandleCustomerOrderChanged);
		CustomerComponent->OnCustomerOrderResolved.RemoveDynamic(this, &AARNPCCharacterBase::HandleCustomerOrderResolved);
		CustomerComponent->OnCustomerOrderResolved.AddDynamic(this, &AARNPCCharacterBase::HandleCustomerOrderResolved);
		CustomerComponent->OnCustomerDoneOrdering.RemoveDynamic(this, &AARNPCCharacterBase::HandleCustomerDoneOrdering);
		CustomerComponent->OnCustomerDoneOrdering.AddDynamic(this, &AARNPCCharacterBase::HandleCustomerDoneOrdering);
	}

	RefreshStateTreeInteractionFlags();
	RefreshAutoWantsToTalkEmotion(IsTalkable());
}

void AARNPCCharacterBase::ResolveOptionalComponents()
{
	TArray<UParleySpeakerComponent*> TalkComponents;
	GetComponents(TalkComponents);
	if (!TalkComponents.IsEmpty())
	{
		UParleySpeakerComponent* PreferredTalkComponent = SpeakerComponent;
		if (!PreferredTalkComponent || !TalkComponents.Contains(PreferredTalkComponent))
		{
			PreferredTalkComponent = TalkComponents[0];
		}

		if (PreferredTalkComponent && !PreferredTalkComponent->GetSpeakerTag().IsValid())
		{
			for (UParleySpeakerComponent* Candidate : TalkComponents)
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
				TEXT("[Speaker] '%s' has %d UParleySpeakerComponent instances. Using '%s' as canonical."),
				*GetNameSafe(this),
				TalkComponents.Num(),
				*GetNameSafe(SpeakerComponent));
		}
	}
	else
	{
		SpeakerComponent = nullptr;
	}

	TArray<UEmoComponent*> EmotionComponents;
	GetComponents(EmotionComponents);
	if (!EmotionComponents.IsEmpty())
	{
		UEmoComponent* PreferredEmotionComponent = EmotionComponent;
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
				TEXT("[Speaker] '%s' has %d UEmoComponent instances. Using '%s' as canonical."),
				*GetNameSafe(this),
				EmotionComponents.Num(),
				*GetNameSafe(EmotionComponent));
		}
	}
	else
	{
		EmotionComponent = nullptr;
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
	else
	{
		CustomerComponent = nullptr;
	}
}

void AARNPCCharacterBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (SpeakerComponent)
	{
		SpeakerComponent->OnSpeakerTalkableStateChanged.RemoveDynamic(this, &AARNPCCharacterBase::HandleSpeakerComponentTalkableStateChanged);
		SpeakerComponent->OnSpeakerEmotionRequested.RemoveDynamic(this, &AARNPCCharacterBase::HandleSpeakerEmotionRequested);
		SpeakerComponent->OnSpeakerEmotionCleared.RemoveDynamic(this, &AARNPCCharacterBase::HandleSpeakerEmotionCleared);
	}
	if (CustomerComponent)
	{
		CustomerComponent->OnCustomerOrderChanged.RemoveDynamic(this, &AARNPCCharacterBase::HandleCustomerOrderChanged);
		CustomerComponent->OnCustomerOrderResolved.RemoveDynamic(this, &AARNPCCharacterBase::HandleCustomerOrderResolved);
		CustomerComponent->OnCustomerDoneOrdering.RemoveDynamic(this, &AARNPCCharacterBase::HandleCustomerDoneOrdering);
	}

	Super::EndPlay(EndPlayReason);
}

void AARNPCCharacterBase::ForwardUseToController(AActor* UsingActor)
{
	AARPlayerController* UsingController = ResolveUsingController(UsingActor);
	if (!UsingController)
	{
		UE_LOG(
			ARLog,
			Warning,
			TEXT("[Use] '%s' could not resolve AARPlayerController from source '%s' (class '%s')."),
			*GetNameSafe(this),
			*GetNameSafe(UsingActor),
			UsingActor ? *UsingActor->GetClass()->GetName() : TEXT("None"));
		return;
	}

	UsingController->RequestInteractWithCharacter(this);
}

AARPlayerController* AARNPCCharacterBase::ResolveUsingController(AActor* UsingActor) const
{
	if (!UsingActor)
	{
		return nullptr;
	}

	if (AARPlayerController* UsingController = Cast<AARPlayerController>(UsingActor))
	{
		return UsingController;
	}

	const APawn* UsingPawn = Cast<APawn>(UsingActor);
	if (!UsingPawn)
	{
		return nullptr;
	}

	return Cast<AARPlayerController>(UsingPawn->GetController());
}

void AARNPCCharacterBase::InteractByController(AARPlayerController* InteractingController)
{
	if (!InteractingController)
	{
		UE_LOG(ARLog, Warning, TEXT("[Interact] '%s' interaction ignored: InteractingController is null."), *GetNameSafe(this));
		return;
	}

	// Runtime resilience: resolve optional components at interaction time in case this actor was reinstanced
	// or component pointers became stale after hot reload/editor world transitions.
	if (!SpeakerComponent || !CustomerComponent || !EmotionComponent)
	{
		ResolveOptionalComponents();
		RefreshStateTreeInteractionFlags();
	}

	const bool bHasActiveCustomerOrder = CustomerComponent && CustomerComponent->HasActiveOrder();
	if (HasAuthority() && CustomerComponent)
	{
		FARRamenServeResult ServeResult;
		if (CustomerComponent->TryServeHeldBowlFromController(InteractingController, ServeResult))
		{
			return;
		}

		if (bHasActiveCustomerOrder)
		{
			UE_LOG(
				ARLog,
				Verbose,
				TEXT("[Interact] '%s' has active customer order but no valid bowl was served by '%s'; trying speaker fallback."),
				*GetNameSafe(this),
				*GetNameSafe(InteractingController));
		}
	}

	if (!SpeakerComponent)
	{
		if (HasAuthority() && CustomerComponent)
		{
			const FGameplayTag CustomerSpeakerTag = CustomerComponent->GetSpeakerTag();
			if (CustomerSpeakerTag.IsValid())
			{
				if (UGameInstance* GameInstance = GetGameInstance())
				{
					if (UParleyDialogueSubsystem* DialogueSubsystem = GameInstance->GetSubsystem<UParleyDialogueSubsystem>())
					{
						FGameplayTag SourceSpeakerTag;
						if (const APawn* InteractingPawn = InteractingController->GetPawn())
						{
							if (const UParleySpeakerComponent* SourceSpeakerComponent = InteractingPawn->FindComponentByClass<UParleySpeakerComponent>())
							{
								SourceSpeakerTag = SourceSpeakerComponent->GetSpeakerTag();
							}
						}
						if (!SourceSpeakerTag.IsValid())
						{
							UE_LOG(
								ARLog,
								Verbose,
								TEXT("[Interact] '%s' cannot start dialogue via customer speaker '%s': controller '%s' has no possessed pawn speaker component."),
								*GetNameSafe(this),
								*CustomerSpeakerTag.ToString(),
								*GetNameSafe(InteractingController));
							return;
						}

						if (DialogueSubsystem->TryStartDialogueBetweenSpeakers(InteractingController, SourceSpeakerTag, CustomerSpeakerTag))
						{
							UE_LOG(
								ARLog,
								Verbose,
								TEXT("[Interact] '%s' started dialogue via customer speaker '%s'."),
								*GetNameSafe(this),
								*CustomerSpeakerTag.ToString());
							return;
						}
					}
				}
			}
		}

		LogMissingSpeakerComponentDiagnostics(this);
		return;
	}

	if (!bSpeakerLocalStateAllowsDialogue && !bHasActiveCustomerOrder)
	{
		UE_LOG(ARLog, Verbose, TEXT("[Speaker] Interact ignored for '%s': local state blocks dialogue."), *GetNameSafe(this));
		return;
	}

	SpeakerComponent->InteractByController(InteractingController);
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

bool AARNPCCharacterBase::IsTalkableForCharacter(FGameplayTag CharacterTag) const
{
	const FGameplayTag NormalizedCharacterTag = ARPlayer::NormalizeCharacterTag(CharacterTag);
	const bool bHasActiveCustomerOrder = CustomerComponent && CustomerComponent->HasActiveOrder();
	return bHasActiveCustomerOrder
		|| (bSpeakerLocalStateAllowsDialogue
			&& SpeakerComponent
			&& NormalizedCharacterTag.IsValid()
			&& SpeakerComponent->IsTalkableForCharacterTag(NormalizedCharacterTag));
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

	const UParleyDialogueSubsystem* DialogueSubsystem = GameInstance->GetSubsystem<UParleyDialogueSubsystem>();
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
	RefreshStateTreeInteractionFlags();
	RefreshAutoWantsToTalkEmotion(bEffectiveTalkable);
	OnSpeakerTalkableStateChanged.Broadcast(bEffectiveTalkable);
}

void AARNPCCharacterBase::HandleSpeakerEmotionRequested(FGameplayTag EmotionTag, FGameplayTag ViewerCharacterTag, bool bIsDialogueLine)
{
	(void)bIsDialogueLine;
	if (!HasAuthority())
	{
		return;
	}

	if (!EmotionComponent)
	{
		UE_LOG(ARLog, Verbose, TEXT("[Emotion] '%s' speaker emotion request ignored: no emotion component."), *GetNameSafe(this));
		return;
	}

	const FGameplayTag NormalizedCharacterTag = ARPlayer::NormalizeCharacterTag(ViewerCharacterTag);
	if (NormalizedCharacterTag.IsValid())
	{
		FGameplayTagContainer ViewerTags;
		ViewerTags.AddTag(ViewerCharacterTag);

		if (EmotionTag.IsValid())
		{
			EmotionComponent->SetEmotionRegistration(SpeakerEmotionSourceId, EmotionTag, 0, ViewerTags);
		}
		else
		{
			EmotionComponent->ClearEmotionRegistration(SpeakerEmotionSourceId, ViewerTags);
		}
		return;
	}

	if (EmotionTag.IsValid())
	{
		EmotionComponent->SetEmotionRegistration(SpeakerEmotionSourceId, EmotionTag, 0);
	}
	else
	{
		EmotionComponent->ClearEmotionRegistration(SpeakerEmotionSourceId);
	}
}

void AARNPCCharacterBase::HandleSpeakerEmotionCleared(FGameplayTag ViewerCharacterTag)
{
	if (!HasAuthority())
	{
		return;
	}

	if (!EmotionComponent)
	{
		return;
	}

	const FGameplayTag NormalizedCharacterTag = ARPlayer::NormalizeCharacterTag(ViewerCharacterTag);
	if (NormalizedCharacterTag.IsValid())
	{
		FGameplayTagContainer ViewerTags;
		ViewerTags.AddTag(ViewerCharacterTag);
		EmotionComponent->ClearEmotionRegistration(SpeakerEmotionSourceId, ViewerTags);
		return;
	}

	EmotionComponent->ClearEmotionRegistration(SpeakerEmotionSourceId);
}

void AARNPCCharacterBase::OnRep_SpeakerLocalStateAllowsDialogue(const bool bOldAllowsDialogue)
{
	if (bSpeakerLocalStateAllowsDialogue == bOldAllowsDialogue)
	{
		return;
	}

	RefreshStateTreeInteractionFlags();
	RefreshAutoWantsToTalkEmotion(IsTalkable());
	OnSpeakerTalkableStateChanged.Broadcast(IsTalkable());
}

void AARNPCCharacterBase::HandleCustomerOrderChanged(const FARRamenOrderRequest& NewOrder)
{
	(void)NewOrder;
	RefreshStateTreeInteractionFlags();
}

void AARNPCCharacterBase::HandleCustomerOrderResolved(const FARRamenServeResult& ServeResult)
{
	(void)ServeResult;
	RefreshStateTreeInteractionFlags();
}

void AARNPCCharacterBase::HandleCustomerDoneOrdering(const int32 OrdersGeneratedCount, const int32 OrdersServedCount, const int32 RemainingOrdersToGenerate)
{
	(void)OrdersGeneratedCount;
	(void)OrdersServedCount;
	(void)RemainingOrdersToGenerate;
	RefreshStateTreeInteractionFlags();
}

void AARNPCCharacterBase::RefreshTalkableFromSubsystem()
{
	if (SpeakerComponent)
	{
		SpeakerComponent->RefreshTalkableFromSubsystem();
	}
}

void AARNPCCharacterBase::RefreshStateTreeInteractionFlags()
{
	bST_HasActiveOrder = CustomerComponent && CustomerComponent->HasOrderForInteraction();
	bST_HasDialogueToSay = bSpeakerLocalStateAllowsDialogue && SpeakerComponent && SpeakerComponent->HasDialogueToSay();
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

	const UEmoSettings* EmotionSettings = GetDefault<UEmoSettings>();
	const FGameplayTag WantsToTalkTag = EmotionSettings ? EmotionSettings->WantsToTalkEmotionTag : FGameplayTag();
	if (!WantsToTalkTag.IsValid())
	{
		EmotionComponent->ClearEmotionRegistration(TalkableStateEmotionSourceId);
		bAutoWantsToTalkEmotionApplied = false;
		UE_LOG(ARLog, Verbose, TEXT("[Emotion][Talkable] '%s' no WantsToTalkEmotionTag configured; cleared TalkableState source."), *GetNameSafe(this));
		return;
	}

	if (bEffectiveTalkable)
	{
		EmotionComponent->SetEmotionRegistration(TalkableStateEmotionSourceId, WantsToTalkTag, 0);
		bAutoWantsToTalkEmotionApplied = true;
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[Emotion][Talkable] '%s' apply TalkableState=%s (effective talkable=true)."),
			*GetNameSafe(this),
			*WantsToTalkTag.ToString());
		return;
	}

	EmotionComponent->ClearEmotionRegistration(TalkableStateEmotionSourceId);
	UE_LOG(ARLog, Verbose, TEXT("[Emotion][Talkable] '%s' cleared TalkableState (effective talkable=false)."), *GetNameSafe(this));
	bAutoWantsToTalkEmotionApplied = false;
}

void AARNPCCharacterBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AARNPCCharacterBase, bSpeakerLocalStateAllowsDialogue);
}
