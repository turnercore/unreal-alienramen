#include "AREmotionComponent.h"

#include "AREmotionResolverSubsystem.h"
#include "AREmotionSettings.h"
#include "ARPlayerController.h"
#include "ARPlayerStateBase.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameplayTagsManager.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#if WITH_EDITOR
#include "Components/BillboardComponent.h"
#include "UObject/UnrealType.h"
#endif

namespace
{
	static const FName DialogueEmotionSourceId(TEXT("Dialogue"));

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
#if WITH_EDITOR
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	bTickInEditor = true;
#endif
}

void UAREmotionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

#if WITH_EDITOR
	const AActor* OwnerActor = GetOwner();
	const UWorld* World = OwnerActor ? OwnerActor->GetWorld() : nullptr;
	if (World && !World->IsGameWorld())
	{
		RefreshEditorPreviewBillboard();
	}
#endif
}

#if WITH_EDITOR
void UAREmotionComponent::OnRegister()
{
	Super::OnRegister();
	RefreshEditorPreviewBillboard();
}

void UAREmotionComponent::OnUnregister()
{
	DestroyEditorPreviewBillboard();
	Super::OnUnregister();
}

void UAREmotionComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	RefreshEditorPreviewBillboard();
}
#endif

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
	SetSystemEmotionTag(DialogueEmotionSourceId, NewEmotionTag, 0);
}

void UAREmotionComponent::SetDialogueEmotionTagForPlayerSlot(const EARPlayerSlot PlayerSlot, const FGameplayTag NewEmotionTag)
{
	SetSystemEmotionTagForPlayerSlot(DialogueEmotionSourceId, PlayerSlot, NewEmotionTag, 0);
}

void UAREmotionComponent::ClearDialogueEmotionTag()
{
	ClearSystemEmotionTag(DialogueEmotionSourceId);
}

void UAREmotionComponent::ClearDialogueEmotionTagForPlayerSlot(const EARPlayerSlot PlayerSlot)
{
	ClearSystemEmotionTagForPlayerSlot(DialogueEmotionSourceId, PlayerSlot);
}

void UAREmotionComponent::ClearAllDialogueEmotionTags()
{
	ClearAllSystemEmotionTagsForSource(DialogueEmotionSourceId);
}

void UAREmotionComponent::SetSystemEmotionTag(const FName SourceId, const FGameplayTag NewEmotionTag, const int32 Priority)
{
	if (!IsAuthorityOwner() || SourceId.IsNone())
	{
		return;
	}

	FSystemEmotionSourceState& SourceState = SystemEmotionSources.FindOrAdd(SourceId);
	SourceState.Priority = Priority;
	SourceState.LastWriteSerial = NextSystemEmotionWriteSerial++;
	SourceState.State.SharedEmotionTag = NewEmotionTag;
	if (!HasAnyStateTag(SourceState.State))
	{
		ClearAllTimedSystemOverrideTimersForSource(SourceId);
		SystemEmotionSources.Remove(SourceId);
	}

	if (RebuildSystemOverrideStateFromSources())
	{
		ForceOwnerNetUpdate();
	}
}

void UAREmotionComponent::SetSystemEmotionTagForDuration(const FName SourceId, const FGameplayTag NewEmotionTag, const float DurationSeconds, const int32 Priority)
{
	SetSystemEmotionTag(SourceId, NewEmotionTag, Priority);
	if (!IsAuthorityOwner() || SourceId.IsNone())
	{
		return;
	}

	if (!NewEmotionTag.IsValid())
	{
		ClearTimedSystemOverrideTimer(SourceId);
		return;
	}

	const float EffectiveDuration = ResolveTimedSystemOverrideDurationSeconds(DurationSeconds);
	SetTimedSystemOverrideClearTimer(SourceId, EffectiveDuration);
}

void UAREmotionComponent::SetSystemEmotionTagForPlayerSlot(const FName SourceId, const EARPlayerSlot PlayerSlot, const FGameplayTag NewEmotionTag, const int32 Priority)
{
	if (!IsAuthorityOwner() || SourceId.IsNone())
	{
		return;
	}

	FSystemEmotionSourceState& SourceState = SystemEmotionSources.FindOrAdd(SourceId);
	SourceState.Priority = Priority;
	SourceState.LastWriteSerial = NextSystemEmotionWriteSerial++;
	SetStateSlotTag(SourceState.State, PlayerSlot, NewEmotionTag);
	if (!HasAnyStateTag(SourceState.State))
	{
		ClearAllTimedSystemOverrideTimersForSource(SourceId);
		SystemEmotionSources.Remove(SourceId);
	}

	if (RebuildSystemOverrideStateFromSources())
	{
		ForceOwnerNetUpdate();
	}
}

void UAREmotionComponent::SetSystemEmotionTagForPlayerSlotForDuration(
	const FName SourceId,
	const EARPlayerSlot PlayerSlot,
	const FGameplayTag NewEmotionTag,
	const float DurationSeconds,
	const int32 Priority)
{
	SetSystemEmotionTagForPlayerSlot(SourceId, PlayerSlot, NewEmotionTag, Priority);
	if (!IsAuthorityOwner() || SourceId.IsNone())
	{
		return;
	}

	if (!NewEmotionTag.IsValid())
	{
		ClearTimedSystemOverrideSlotTimer(SourceId, PlayerSlot);
		return;
	}

	const float EffectiveDuration = ResolveTimedSystemOverrideDurationSeconds(DurationSeconds);
	SetTimedSystemOverrideSlotClearTimer(SourceId, PlayerSlot, EffectiveDuration);
}

void UAREmotionComponent::ClearSystemEmotionTag(const FName SourceId)
{
	if (!IsAuthorityOwner() || SourceId.IsNone())
	{
		return;
	}

	ClearTimedSystemOverrideTimer(SourceId);
	if (FSystemEmotionSourceState* SourceState = SystemEmotionSources.Find(SourceId))
	{
		SourceState->LastWriteSerial = NextSystemEmotionWriteSerial++;
		SourceState->State.SharedEmotionTag = FGameplayTag();
		if (!HasAnyStateTag(SourceState->State))
		{
			SystemEmotionSources.Remove(SourceId);
		}
	}

	if (RebuildSystemOverrideStateFromSources())
	{
		ForceOwnerNetUpdate();
	}
}

void UAREmotionComponent::ClearSystemEmotionTagForPlayerSlot(const FName SourceId, const EARPlayerSlot PlayerSlot)
{
	if (!IsAuthorityOwner() || SourceId.IsNone())
	{
		return;
	}

	ClearTimedSystemOverrideSlotTimer(SourceId, PlayerSlot);
	if (FSystemEmotionSourceState* SourceState = SystemEmotionSources.Find(SourceId))
	{
		SourceState->LastWriteSerial = NextSystemEmotionWriteSerial++;
		SetStateSlotTag(SourceState->State, PlayerSlot, FGameplayTag());
		if (!HasAnyStateTag(SourceState->State))
		{
			SystemEmotionSources.Remove(SourceId);
		}
	}

	if (RebuildSystemOverrideStateFromSources())
	{
		ForceOwnerNetUpdate();
	}
}

void UAREmotionComponent::ClearAllSystemEmotionTagsForSource(const FName SourceId)
{
	if (!IsAuthorityOwner() || SourceId.IsNone())
	{
		return;
	}

	ClearAllTimedSystemOverrideTimersForSource(SourceId);
	if (SystemEmotionSources.Remove(SourceId) <= 0)
	{
		return;
	}

	if (RebuildSystemOverrideStateFromSources())
	{
		ForceOwnerNetUpdate();
	}
}

void UAREmotionComponent::ClearAllSystemEmotionTags()
{
	if (!IsAuthorityOwner() || SystemEmotionSources.IsEmpty())
	{
		return;
	}

	TArray<FName> SourceIds;
	SystemEmotionSources.GetKeys(SourceIds);
	for (const FName SourceId : SourceIds)
	{
		ClearAllTimedSystemOverrideTimersForSource(SourceId);
	}
	SystemEmotionSources.Reset();
	if (RebuildSystemOverrideStateFromSources())
	{
		ForceOwnerNetUpdate();
	}
}

FGameplayTag UAREmotionComponent::GetDisplayedEmotionTagForPlayerSlot(const EARPlayerSlot PlayerSlot) const
{
	const FGameplayTag SystemSlotTag = GetStateSlotTag(SystemOverrideState, PlayerSlot);
	if (SystemSlotTag.IsValid())
	{
		return SystemSlotTag;
	}

	if (SystemOverrideState.SharedEmotionTag.IsValid())
	{
		return SystemOverrideState.SharedEmotionTag;
	}

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

	FVector EffectiveOffset = AnchorWorldOffset;
	if (const UAREmotionSettings* Settings = GetDefault<UAREmotionSettings>())
	{
		if (EffectiveOffset.IsNearlyZero() && !Settings->DefaultAnchorWorldOffset.IsNearlyZero())
		{
			EffectiveOffset = Settings->DefaultAnchorWorldOffset;
		}
	}

	FTransform AnchorTransform = FTransform::Identity;
	if (TryResolveAnchorTransformFromReference(OwnerActor, AnchorTransform))
	{
		return AnchorTransform.GetLocation() + EffectiveOffset;
	}

	FVector Origin = FVector::ZeroVector;
	FVector Extent = FVector::ZeroVector;
	OwnerActor->GetActorBounds(true, Origin, Extent);
	return Origin + FVector(0.0f, 0.0f, Extent.Z) + EffectiveOffset;
}

bool UAREmotionComponent::TryResolveAnchorTransformFromReference(const AActor* OwnerActor, FTransform& OutAnchorTransform) const
{
	OutAnchorTransform = FTransform::Identity;

	UObject* AnchorObject = AnchorTransformObject.Get();
	if (!AnchorObject)
	{
		return false;
	}

	if (USceneComponent* SourceSceneComponent = Cast<USceneComponent>(AnchorObject))
	{
		USceneComponent* ResolvedSceneComponent = SourceSceneComponent;
		if (OwnerActor && SourceSceneComponent->GetOwner() != OwnerActor)
		{
			const FName DesiredName = SourceSceneComponent->GetFName();
			if (!DesiredName.IsNone())
			{
				TArray<USceneComponent*> OwnerSceneComponents;
				const_cast<AActor*>(OwnerActor)->GetComponents(OwnerSceneComponents);
				for (USceneComponent* Candidate : OwnerSceneComponents)
				{
					if (Candidate
						&& Candidate->GetFName() == DesiredName
						&& Candidate->GetClass() == SourceSceneComponent->GetClass())
					{
						ResolvedSceneComponent = Candidate;
						break;
					}
				}
			}
		}

		if (ResolvedSceneComponent)
		{
			OutAnchorTransform = ResolvedSceneComponent->GetComponentTransform();
			return true;
		}
	}

	if (const AActor* SourceActor = Cast<AActor>(AnchorObject))
	{
		OutAnchorTransform = SourceActor->GetActorTransform();
		return true;
	}

	if (const UActorComponent* SourceActorComponent = Cast<UActorComponent>(AnchorObject))
	{
		if (const AActor* ComponentOwner = SourceActorComponent->GetOwner())
		{
			OutAnchorTransform = ComponentOwner->GetActorTransform();
			return true;
		}
	}

	return false;
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

bool UAREmotionComponent::HasAnyStateTag(const FAREmotionDisplayState& State)
{
	return State.SharedEmotionTag.IsValid() || State.P1EmotionTag.IsValid() || State.P2EmotionTag.IsValid();
}

FName UAREmotionComponent::MakeTimedSlotKey(const FName SourceId, const EARPlayerSlot Slot)
{
	return FName(*FString::Printf(TEXT("%s|%d"), *SourceId.ToString(), static_cast<int32>(Slot)));
}

bool UAREmotionComponent::RebuildSystemOverrideStateFromSources()
{
	FAREmotionDisplayState ResolvedState;
	int32 SharedPriority = TNumericLimits<int32>::Lowest();
	uint64 SharedSerial = 0;
	int32 P1Priority = TNumericLimits<int32>::Lowest();
	uint64 P1Serial = 0;
	int32 P2Priority = TNumericLimits<int32>::Lowest();
	uint64 P2Serial = 0;

	for (const TPair<FName, FSystemEmotionSourceState>& Pair : SystemEmotionSources)
	{
		const FSystemEmotionSourceState& SourceState = Pair.Value;
		auto ShouldReplace = [](const int32 CandidatePriority, const uint64 CandidateSerial, const int32 CurrentPriority, const uint64 CurrentSerial)
		{
			return CandidatePriority > CurrentPriority || (CandidatePriority == CurrentPriority && CandidateSerial > CurrentSerial);
		};

		if (SourceState.State.SharedEmotionTag.IsValid()
			&& ShouldReplace(SourceState.Priority, SourceState.LastWriteSerial, SharedPriority, SharedSerial))
		{
			ResolvedState.SharedEmotionTag = SourceState.State.SharedEmotionTag;
			SharedPriority = SourceState.Priority;
			SharedSerial = SourceState.LastWriteSerial;
		}

		if (SourceState.State.P1EmotionTag.IsValid()
			&& ShouldReplace(SourceState.Priority, SourceState.LastWriteSerial, P1Priority, P1Serial))
		{
			ResolvedState.P1EmotionTag = SourceState.State.P1EmotionTag;
			P1Priority = SourceState.Priority;
			P1Serial = SourceState.LastWriteSerial;
		}

		if (SourceState.State.P2EmotionTag.IsValid()
			&& ShouldReplace(SourceState.Priority, SourceState.LastWriteSerial, P2Priority, P2Serial))
		{
			ResolvedState.P2EmotionTag = SourceState.State.P2EmotionTag;
			P2Priority = SourceState.Priority;
			P2Serial = SourceState.LastWriteSerial;
		}
	}

	if (AreDisplayStatesEqual(SystemOverrideState, ResolvedState))
	{
		return false;
	}

	const FAREmotionDisplayState OldState = SystemOverrideState;
	SystemOverrideState = ResolvedState;
	OnRep_SystemOverrideState(OldState);
	return true;
}

float UAREmotionComponent::ResolveTimedSystemOverrideDurationSeconds(const float RequestedDurationSeconds) const
{
	if (RequestedDurationSeconds > 0.0f)
	{
		return RequestedDurationSeconds;
	}

	const UAREmotionSettings* Settings = GetDefault<UAREmotionSettings>();
	const float DefaultDuration = Settings ? Settings->DefaultTimedSystemOverrideDurationSeconds : 1.5f;
	return FMath::Max(0.01f, DefaultDuration);
}

void UAREmotionComponent::SetTimedSystemOverrideClearTimer(const FName SourceId, const float DurationSeconds)
{
	UWorld* World = GetWorld();
	if (!World || SourceId.IsNone())
	{
		return;
	}

	FTimerHandle& Handle = TimedSystemOverrideClearHandles.FindOrAdd(SourceId);
	FTimerDelegate Delegate;
	Delegate.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(UAREmotionComponent, HandleTimedSystemOverrideClear), SourceId);
	World->GetTimerManager().SetTimer(Handle, Delegate, FMath::Max(0.01f, DurationSeconds), false);
}

void UAREmotionComponent::SetTimedSystemOverrideSlotClearTimer(const FName SourceId, const EARPlayerSlot Slot, const float DurationSeconds)
{
	UWorld* World = GetWorld();
	if (!World || SourceId.IsNone() || Slot == EARPlayerSlot::Unknown)
	{
		return;
	}

	const FName TimerKey = MakeTimedSlotKey(SourceId, Slot);
	FTimerHandle& Handle = TimedSystemOverrideSlotClearHandles.FindOrAdd(TimerKey);
	FTimerDelegate Delegate;
	Delegate.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(UAREmotionComponent, HandleTimedSystemOverrideSlotClear), SourceId, Slot);
	World->GetTimerManager().SetTimer(Handle, Delegate, FMath::Max(0.01f, DurationSeconds), false);
}

void UAREmotionComponent::ClearTimedSystemOverrideTimer(const FName SourceId)
{
	UWorld* World = GetWorld();
	FTimerHandle* Handle = TimedSystemOverrideClearHandles.Find(SourceId);
	if (World && Handle)
	{
		World->GetTimerManager().ClearTimer(*Handle);
	}

	TimedSystemOverrideClearHandles.Remove(SourceId);
}

void UAREmotionComponent::ClearTimedSystemOverrideSlotTimer(const FName SourceId, const EARPlayerSlot Slot)
{
	const FName TimerKey = MakeTimedSlotKey(SourceId, Slot);
	UWorld* World = GetWorld();
	FTimerHandle* Handle = TimedSystemOverrideSlotClearHandles.Find(TimerKey);
	if (World && Handle)
	{
		World->GetTimerManager().ClearTimer(*Handle);
	}

	TimedSystemOverrideSlotClearHandles.Remove(TimerKey);
}

void UAREmotionComponent::ClearAllTimedSystemOverrideTimersForSource(const FName SourceId)
{
	ClearTimedSystemOverrideTimer(SourceId);
	ClearTimedSystemOverrideSlotTimer(SourceId, EARPlayerSlot::P1);
	ClearTimedSystemOverrideSlotTimer(SourceId, EARPlayerSlot::P2);
}

void UAREmotionComponent::HandleTimedSystemOverrideClear(const FName SourceId)
{
	ClearTimedSystemOverrideTimer(SourceId);
	ClearSystemEmotionTag(SourceId);
}

void UAREmotionComponent::HandleTimedSystemOverrideSlotClear(const FName SourceId, const EARPlayerSlot Slot)
{
	ClearTimedSystemOverrideSlotTimer(SourceId, Slot);
	ClearSystemEmotionTagForPlayerSlot(SourceId, Slot);
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
#if WITH_EDITOR
		RefreshEditorPreviewBillboard();
#endif
	}
}

void UAREmotionComponent::OnRep_DialogueOverrideState(const FAREmotionDisplayState OldState)
{
	if (!AreDisplayStatesEqual(OldState, DialogueOverrideState))
	{
		OnEmotionDisplayStateChanged.Broadcast();
#if WITH_EDITOR
		RefreshEditorPreviewBillboard();
#endif
	}
}

void UAREmotionComponent::OnRep_SystemOverrideState(const FAREmotionDisplayState OldState)
{
	if (!AreDisplayStatesEqual(OldState, SystemOverrideState))
	{
		OnEmotionDisplayStateChanged.Broadcast();
#if WITH_EDITOR
		RefreshEditorPreviewBillboard();
#endif
	}
}

void UAREmotionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UAREmotionComponent, RegisteredSpeakerTag);
	DOREPLIFETIME(UAREmotionComponent, BaseEmotionState);
	DOREPLIFETIME(UAREmotionComponent, DialogueOverrideState);
	DOREPLIFETIME(UAREmotionComponent, SystemOverrideState);
}

#if WITH_EDITOR
void UAREmotionComponent::RefreshEditorPreviewBillboard()
{
#if WITH_EDITORONLY_DATA
	AActor* OwnerActor = GetOwner();
	UWorld* World = OwnerActor ? OwnerActor->GetWorld() : nullptr;
	if (!OwnerActor
		|| !World
		|| World->IsGameWorld()
		|| OwnerActor->HasAnyFlags(RF_ClassDefaultObject))
	{
		DestroyEditorPreviewBillboard();
		return;
	}

	TSoftObjectPtr<UTexture2D> PreviewIconTexture;
	FGameplayTag ResolvedPreviewTag;
	if (!TryResolvePreviewEmotionIcon(PreviewIconTexture, ResolvedPreviewTag) || PreviewIconTexture.IsNull())
	{
		DestroyEditorPreviewBillboard();
		return;
	}

	UTexture2D* LoadedTexture = PreviewIconTexture.Get();
	if (!LoadedTexture)
	{
		LoadedTexture = PreviewIconTexture.LoadSynchronous();
	}
	if (!LoadedTexture)
	{
		DestroyEditorPreviewBillboard();
		return;
	}

	if (!EditorPreviewBillboardComponent)
	{
		EditorPreviewBillboardComponent = NewObject<UBillboardComponent>(OwnerActor, NAME_None, RF_Transient | RF_TextExportTransient);
		if (!EditorPreviewBillboardComponent)
		{
			return;
		}

		EditorPreviewBillboardComponent->CreationMethod = EComponentCreationMethod::Instance;
		EditorPreviewBillboardComponent->bIsEditorOnly = true;
		EditorPreviewBillboardComponent->SetHiddenInGame(true);
		EditorPreviewBillboardComponent->SetMobility(EComponentMobility::Movable);
		OwnerActor->AddInstanceComponent(EditorPreviewBillboardComponent);
		EditorPreviewBillboardComponent->RegisterComponentWithWorld(World);
	}

	EditorPreviewBillboardComponent->SetSprite(LoadedTexture);
	EditorPreviewBillboardComponent->SetVisibility(true, true);

	const float PreviewScale = FMath::Max(0.05f, IconScreenSize / 64.0f);
	EditorPreviewBillboardComponent->SetRelativeScale3D(FVector(PreviewScale));

	USceneComponent* AttachAnchor = Cast<USceneComponent>(AnchorTransformObject.Get());
	if (AttachAnchor && OwnerActor && AttachAnchor->GetOwner() != OwnerActor)
	{
		const FName DesiredName = AttachAnchor->GetFName();
		if (!DesiredName.IsNone())
		{
			TArray<USceneComponent*> OwnerSceneComponents;
			OwnerActor->GetComponents(OwnerSceneComponents);
			for (USceneComponent* Candidate : OwnerSceneComponents)
			{
				if (Candidate && Candidate->GetFName() == DesiredName && Candidate->GetClass() == AttachAnchor->GetClass())
				{
					AttachAnchor = Candidate;
					break;
				}
			}
		}
	}

	if (AttachAnchor)
	{
		if (EditorPreviewBillboardComponent->GetAttachParent() != AttachAnchor)
		{
			EditorPreviewBillboardComponent->AttachToComponent(AttachAnchor, FAttachmentTransformRules::KeepWorldTransform);
		}
		EditorPreviewBillboardComponent->SetRelativeLocation(AnchorWorldOffset);
	}
	else
	{
		EditorPreviewBillboardComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		EditorPreviewBillboardComponent->SetWorldLocation(GetEmotionAnchorWorldLocation());
	}
#endif
}

void UAREmotionComponent::DestroyEditorPreviewBillboard()
{
#if WITH_EDITORONLY_DATA
	if (EditorPreviewBillboardComponent)
	{
		EditorPreviewBillboardComponent->DestroyComponent();
		EditorPreviewBillboardComponent = nullptr;
	}
#endif
}
#endif
