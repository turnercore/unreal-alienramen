#include "AREmotionComponent.h"

#include "AREmotionResolverSubsystem.h"
#include "AREmotionSettings.h"
#include "ARPlayerController.h"
#include "ARPlayerStateBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameplayTagsManager.h"
#include "Net/UnrealNetwork.h"

namespace
{
	static EARPlayerSlot ResolveViewerSlot(const AARPlayerController* ViewerController)
	{
		if (!ViewerController)
		{
			return EARPlayerSlot::Unknown;
		}

		const AARPlayerStateBase* ViewerState = ViewerController->GetPlayerState<AARPlayerStateBase>();
		return ViewerState ? ViewerState->GetPlayerSlot() : EARPlayerSlot::Unknown;
	}

	static bool AreTagsEqual(const FGameplayTag& Left, const FGameplayTag& Right)
	{
		if (!Left.IsValid() && !Right.IsValid())
		{
			return true;
		}
		if (!Left.IsValid() || !Right.IsValid())
		{
			return false;
		}
		return Left.MatchesTagExact(Right);
	}

}

UAREmotionComponent::UAREmotionComponent()
{
	SetIsReplicatedByDefault(true);
}

void UAREmotionComponent::SetEmotionTag(const FGameplayTag NewEmotionTag)
{
	if (!IsAuthorityOwner() || AreTagsEqual(BaseEmotionState.SharedEmotionTag, NewEmotionTag))
	{
		return;
	}

	const FAREmotionDisplayState OldBaseState = BaseEmotionState;
	BaseEmotionState.SharedEmotionTag = NewEmotionTag;
	OnRep_BaseEmotionState(OldBaseState);
	ForceOwnerNetUpdate();
}

void UAREmotionComponent::SetEmotionTagForPlayerSlot(const EARPlayerSlot PlayerSlot, const FGameplayTag NewEmotionTag)
{
	if (!IsAuthorityOwner())
	{
		return;
	}

	const FGameplayTag Existing = GetStateSlotTag(BaseEmotionState, PlayerSlot);
	if (AreTagsEqual(Existing, NewEmotionTag))
	{
		return;
	}

	const FAREmotionDisplayState OldBaseState = BaseEmotionState;
	SetStateSlotTag(BaseEmotionState, PlayerSlot, NewEmotionTag);
	OnRep_BaseEmotionState(OldBaseState);
	ForceOwnerNetUpdate();
}

void UAREmotionComponent::ClearEmotionTag()
{
	SetEmotionTag(FGameplayTag());
}

void UAREmotionComponent::ClearEmotionTagForPlayerSlot(const EARPlayerSlot PlayerSlot)
{
	SetEmotionTagForPlayerSlot(PlayerSlot, FGameplayTag());
}

void UAREmotionComponent::ClearAllEmotionTags()
{
	if (!IsAuthorityOwner())
	{
		return;
	}

	if (!BaseEmotionState.SharedEmotionTag.IsValid()
		&& !BaseEmotionState.P1EmotionTag.IsValid()
		&& !BaseEmotionState.P2EmotionTag.IsValid())
	{
		return;
	}

	const FAREmotionDisplayState OldBaseState = BaseEmotionState;
	BaseEmotionState = FAREmotionDisplayState();
	OnRep_BaseEmotionState(OldBaseState);
	ForceOwnerNetUpdate();
}

void UAREmotionComponent::SetDialogueEmotionTag(const FGameplayTag NewEmotionTag)
{
	if (!IsAuthorityOwner() || AreTagsEqual(DialogueOverrideState.SharedEmotionTag, NewEmotionTag))
	{
		return;
	}

	const FAREmotionDisplayState OldDialogueState = DialogueOverrideState;
	DialogueOverrideState.SharedEmotionTag = NewEmotionTag;
	OnRep_DialogueOverrideState(OldDialogueState);
	ForceOwnerNetUpdate();
}

void UAREmotionComponent::SetDialogueEmotionTagForPlayerSlot(const EARPlayerSlot PlayerSlot, const FGameplayTag NewEmotionTag)
{
	if (!IsAuthorityOwner())
	{
		return;
	}

	const FGameplayTag Existing = GetStateSlotTag(DialogueOverrideState, PlayerSlot);
	if (AreTagsEqual(Existing, NewEmotionTag))
	{
		return;
	}

	const FAREmotionDisplayState OldDialogueState = DialogueOverrideState;
	SetStateSlotTag(DialogueOverrideState, PlayerSlot, NewEmotionTag);
	OnRep_DialogueOverrideState(OldDialogueState);
	ForceOwnerNetUpdate();
}

void UAREmotionComponent::ClearDialogueEmotionTag()
{
	SetDialogueEmotionTag(FGameplayTag());
}

void UAREmotionComponent::ClearDialogueEmotionTagForPlayerSlot(const EARPlayerSlot PlayerSlot)
{
	SetDialogueEmotionTagForPlayerSlot(PlayerSlot, FGameplayTag());
}

void UAREmotionComponent::ClearAllDialogueEmotionTags()
{
	if (!IsAuthorityOwner())
	{
		return;
	}

	if (!DialogueOverrideState.SharedEmotionTag.IsValid()
		&& !DialogueOverrideState.P1EmotionTag.IsValid()
		&& !DialogueOverrideState.P2EmotionTag.IsValid())
	{
		return;
	}

	const FAREmotionDisplayState OldDialogueState = DialogueOverrideState;
	DialogueOverrideState = FAREmotionDisplayState();
	OnRep_DialogueOverrideState(OldDialogueState);
	ForceOwnerNetUpdate();
}

FGameplayTag UAREmotionComponent::GetDisplayedEmotionTagForPlayerSlot(const EARPlayerSlot PlayerSlot) const
{
	const FGameplayTag DialogueSlotTag = GetStateSlotTag(DialogueOverrideState, PlayerSlot);
	if (DialogueSlotTag.IsValid())
	{
		return DialogueSlotTag;
	}

	if (DialogueOverrideState.SharedEmotionTag.IsValid())
	{
		return DialogueOverrideState.SharedEmotionTag;
	}

	const FGameplayTag BaseSlotTag = GetStateSlotTag(BaseEmotionState, PlayerSlot);
	if (BaseSlotTag.IsValid())
	{
		return BaseSlotTag;
	}

	return BaseEmotionState.SharedEmotionTag;
}

FGameplayTag UAREmotionComponent::GetDisplayedEmotionTagForController(const AARPlayerController* ViewerController) const
{
	return GetDisplayedEmotionTagForPlayerSlot(ResolveViewerSlot(ViewerController));
}

bool UAREmotionComponent::TryResolveDisplayedEmotionIconForPlayerSlot(
	const EARPlayerSlot PlayerSlot,
	TSoftObjectPtr<UTexture2D>& OutIconTexture,
	FGameplayTag& OutResolvedEmotionTag) const
{
	const FGameplayTag DisplayTag = GetDisplayedEmotionTagForPlayerSlot(PlayerSlot);
	return TryResolveEmotionIconForTag(DisplayTag, OutIconTexture, OutResolvedEmotionTag);
}

bool UAREmotionComponent::TryResolveDisplayedEmotionIconForController(
	const AARPlayerController* ViewerController,
	TSoftObjectPtr<UTexture2D>& OutIconTexture,
	FGameplayTag& OutResolvedEmotionTag) const
{
	return TryResolveDisplayedEmotionIconForPlayerSlot(ResolveViewerSlot(ViewerController), OutIconTexture, OutResolvedEmotionTag);
}

bool UAREmotionComponent::TryResolveEmotionIconForTag(
	const FGameplayTag EmotionTag,
	TSoftObjectPtr<UTexture2D>& OutIconTexture,
	FGameplayTag& OutResolvedEmotionTag) const
{
	OutIconTexture.Reset();
	OutResolvedEmotionTag = FGameplayTag();

	if (!EmotionTag.IsValid())
	{
		return false;
	}

	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (GameInstance)
	{
		if (UAREmotionResolverSubsystem* Resolver = GameInstance->GetSubsystem<UAREmotionResolverSubsystem>())
		{
			return Resolver->TryResolveEmotionIcon(EmotionTag, OutIconTexture, OutResolvedEmotionTag);
		}
	}

	return UAREmotionResolverSubsystem::TryResolveEmotionIconFromConfiguredData(EmotionTag, OutIconTexture, OutResolvedEmotionTag);
}

bool UAREmotionComponent::TryResolvePreviewEmotionIcon(
	TSoftObjectPtr<UTexture2D>& OutIconTexture,
	FGameplayTag& OutResolvedEmotionTag) const
{
	const FGameplayTag PreviewTag = GetPreviewEmotionTag();
	return TryResolveEmotionIconForTag(PreviewTag, OutIconTexture, OutResolvedEmotionTag);
}

FGameplayTag UAREmotionComponent::GetPreviewEmotionTag() const
{
	if (PreviewEmotionTag.IsValid())
	{
		return PreviewEmotionTag;
	}

	return UGameplayTagsManager::Get().RequestGameplayTag(FName(TEXT("Dialogue.Emotion.Preview")), false);
}

FVector UAREmotionComponent::GetEmotionAnchorWorldLocation() const
{
	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return FVector::ZeroVector;
	}

	FName EffectiveSocketName = AnchorSocketName;
	FVector EffectiveOffset = AnchorWorldOffset;
	if (const UAREmotionSettings* Settings = GetDefault<UAREmotionSettings>())
	{
		if (EffectiveSocketName.IsNone() && !Settings->DefaultAnchorSocketName.IsNone())
		{
			EffectiveSocketName = Settings->DefaultAnchorSocketName;
		}
		if (EffectiveOffset.IsNearlyZero() && !Settings->DefaultAnchorWorldOffset.IsNearlyZero())
		{
			EffectiveOffset = Settings->DefaultAnchorWorldOffset;
		}
	}

	const ACharacter* CharacterOwner = Cast<ACharacter>(OwnerActor);
	if (CharacterOwner && !EffectiveSocketName.IsNone())
	{
		const USkeletalMeshComponent* Mesh = CharacterOwner->GetMesh();
		if (Mesh && Mesh->DoesSocketExist(EffectiveSocketName))
		{
			return Mesh->GetSocketLocation(EffectiveSocketName) + EffectiveOffset;
		}
	}

	FVector Origin = FVector::ZeroVector;
	FVector Extent = FVector::ZeroVector;
	OwnerActor->GetActorBounds(true, Origin, Extent);
	return Origin + FVector(0.0f, 0.0f, Extent.Z) + EffectiveOffset;
}

bool UAREmotionComponent::GetEmotionFacingRotationForController(const APlayerController* ViewerController, FRotator& OutFacingRotation) const
{
	OutFacingRotation = FRotator::ZeroRotator;
	if (!ViewerController || !ViewerController->PlayerCameraManager)
	{
		return false;
	}

	const FVector AnchorLocation = GetEmotionAnchorWorldLocation();
	const FVector CameraLocation = ViewerController->PlayerCameraManager->GetCameraLocation();
	OutFacingRotation = (CameraLocation - AnchorLocation).Rotation();
	return true;
}

void UAREmotionComponent::SetRegisteredSpeakerTag(const FGameplayTag NewSpeakerTag)
{
	if (!IsAuthorityOwner() || AreTagsEqual(RegisteredSpeakerTag, NewSpeakerTag))
	{
		return;
	}

	RegisteredSpeakerTag = NewSpeakerTag;
	ForceOwnerNetUpdate();
}

FGameplayTag UAREmotionComponent::GetStateSlotTag(const FAREmotionDisplayState& State, const EARPlayerSlot PlayerSlot)
{
	switch (PlayerSlot)
	{
	case EARPlayerSlot::P1:
		return State.P1EmotionTag;
	case EARPlayerSlot::P2:
		return State.P2EmotionTag;
	default:
		return FGameplayTag();
	}
}

void UAREmotionComponent::SetStateSlotTag(FAREmotionDisplayState& State, const EARPlayerSlot PlayerSlot, const FGameplayTag& EmotionTag)
{
	switch (PlayerSlot)
	{
	case EARPlayerSlot::P1:
		State.P1EmotionTag = EmotionTag;
		break;
	case EARPlayerSlot::P2:
		State.P2EmotionTag = EmotionTag;
		break;
	default:
		break;
	}
}

bool UAREmotionComponent::AreDisplayStatesEqual(const FAREmotionDisplayState& Left, const FAREmotionDisplayState& Right)
{
	return AreTagsEqual(Left.SharedEmotionTag, Right.SharedEmotionTag)
		&& AreTagsEqual(Left.P1EmotionTag, Right.P1EmotionTag)
		&& AreTagsEqual(Left.P2EmotionTag, Right.P2EmotionTag);
}

bool UAREmotionComponent::IsAuthorityOwner() const
{
	const AActor* OwnerActor = GetOwner();
	return OwnerActor && OwnerActor->HasAuthority();
}

void UAREmotionComponent::ForceOwnerNetUpdate() const
{
	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->ForceNetUpdate();
	}
}

void UAREmotionComponent::OnRep_BaseEmotionState(const FAREmotionDisplayState OldState)
{
	if (!AreDisplayStatesEqual(OldState, BaseEmotionState))
	{
		OnEmotionDisplayStateChanged.Broadcast();
	}
}

void UAREmotionComponent::OnRep_DialogueOverrideState(const FAREmotionDisplayState OldState)
{
	if (!AreDisplayStatesEqual(OldState, DialogueOverrideState))
	{
		OnEmotionDisplayStateChanged.Broadcast();
	}
}

void UAREmotionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UAREmotionComponent, RegisteredSpeakerTag);
	DOREPLIFETIME(UAREmotionComponent, BaseEmotionState);
	DOREPLIFETIME(UAREmotionComponent, DialogueOverrideState);
}
