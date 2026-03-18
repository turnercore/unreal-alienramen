#include "EmoComponent.h"

#include "EmoComponentRegistrySubsystem.h"
#include "EmoResolverSubsystem.h"
#include "EmoSettings.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameplayTagsManager.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"
#if WITH_EDITOR
#include "Components/BillboardComponent.h"
#endif

namespace
{
	static FGameplayTag GetP1SlotTag()
	{
		return UGameplayTagsManager::Get().RequestGameplayTag(FName(TEXT("Player.Slot.P1")), false);
	}

	static FGameplayTag GetP2SlotTag()
	{
		return UGameplayTagsManager::Get().RequestGameplayTag(FName(TEXT("Player.Slot.P2")), false);
	}

	static bool IsP1SlotTag(const FGameplayTag& SlotTag)
	{
		const FGameplayTag P1Tag = GetP1SlotTag();
		return P1Tag.IsValid() && SlotTag.MatchesTagExact(P1Tag);
	}

	static bool IsP2SlotTag(const FGameplayTag& SlotTag)
	{
		const FGameplayTag P2Tag = GetP2SlotTag();
		return P2Tag.IsValid() && SlotTag.MatchesTagExact(P2Tag);
	}

	static bool TryGetPlayerSlotTagFromObject(const UObject* SourceObject, FGameplayTag& OutPlayerSlotTag)
	{
		OutPlayerSlotTag = FGameplayTag();
		if (!SourceObject)
		{
			return false;
		}

		if (UFunction* GetPlayerSlotTagFunction = SourceObject->FindFunction(TEXT("GetPlayerSlotTag")))
		{
			struct FGetPlayerSlotTagParams
			{
				FGameplayTag ReturnValue;
			};

			FGetPlayerSlotTagParams Params;
			const_cast<UObject*>(SourceObject)->ProcessEvent(GetPlayerSlotTagFunction, &Params);
			if (Params.ReturnValue.IsValid())
			{
				OutPlayerSlotTag = Params.ReturnValue;
				return true;
			}
		}

		const FStructProperty* SlotTagProperty = FindFProperty<FStructProperty>(SourceObject->GetClass(), TEXT("PlayerSlotTag"));
		if (!SlotTagProperty || SlotTagProperty->Struct != TBaseStructure<FGameplayTag>::Get())
		{
			return false;
		}

		if (const FGameplayTag* SlotTagValue = SlotTagProperty->ContainerPtrToValuePtr<FGameplayTag>(SourceObject))
		{
			if (SlotTagValue->IsValid())
			{
				OutPlayerSlotTag = *SlotTagValue;
				return true;
			}
		}

		return false;
	}

	static FGameplayTag ResolveViewerSlotTag(const APlayerController* ViewerController)
	{
		if (!ViewerController)
		{
			return FGameplayTag();
		}

		FGameplayTag SlotTag;
		if (TryGetPlayerSlotTagFromObject(ViewerController, SlotTag))
		{
			return SlotTag;
		}

		if (const APlayerState* ViewerState = ViewerController->GetPlayerState<APlayerState>())
		{
			if (TryGetPlayerSlotTagFromObject(ViewerState, SlotTag))
			{
				return SlotTag;
			}
		}

		return FGameplayTag();
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

	static FGameplayTag GetSlotEmotionTag(const FEmoDisplayState& State, const FGameplayTag& SlotTag)
	{
		if (IsP1SlotTag(SlotTag))
		{
			return State.P1EmotionTag;
		}

		if (IsP2SlotTag(SlotTag))
		{
			return State.P2EmotionTag;
		}

		return FGameplayTag();
	}

	static FGameplayTag ResolveDisplayedEmotionTagFromStates(
		const FEmoDisplayState& BaseState,
		const FEmoDisplayState& DialogueState,
		const FEmoDisplayState& SystemState,
		const FGameplayTag& SlotTag)
	{
		const FGameplayTag SystemSlotTag = GetSlotEmotionTag(SystemState, SlotTag);
		if (SystemSlotTag.IsValid())
		{
			return SystemSlotTag;
		}

		if (SystemState.SharedEmotionTag.IsValid())
		{
			return SystemState.SharedEmotionTag;
		}

		const FGameplayTag DialogueSlotTag = GetSlotEmotionTag(DialogueState, SlotTag);
		if (DialogueSlotTag.IsValid())
		{
			return DialogueSlotTag;
		}

		if (DialogueState.SharedEmotionTag.IsValid())
		{
			return DialogueState.SharedEmotionTag;
		}

		const FGameplayTag BaseSlotTag = GetSlotEmotionTag(BaseState, SlotTag);
		if (BaseSlotTag.IsValid())
		{
			return BaseSlotTag;
		}

		return BaseState.SharedEmotionTag;
	}

}

UEmoComponent::UEmoComponent()
{
	SetIsReplicatedByDefault(true);
}

void UEmoComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* World = GetWorld())
	{
		if (UEmoComponentRegistrySubsystem* Registry = World->GetSubsystem<UEmoComponentRegistrySubsystem>())
		{
			Registry->RegisterEmotionComponent(this);
		}
	}
}

void UEmoComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UEmoComponentRegistrySubsystem* Registry = World->GetSubsystem<UEmoComponentRegistrySubsystem>())
		{
			Registry->UnregisterEmotionComponent(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

#if WITH_EDITOR
void UEmoComponent::OnRegister()
{
	Super::OnRegister();
	RefreshEditorPreviewBillboard();
}

void UEmoComponent::OnUnregister()
{
	DestroyEditorPreviewBillboard();
	Super::OnUnregister();
}

void UEmoComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	RefreshEditorPreviewBillboard();
}
#endif

void UEmoComponent::SetEmotionTag(const FGameplayTag NewEmotionTag)
{
	if (!IsAuthorityOwner() || AreTagsEqual(BaseEmotionState.SharedEmotionTag, NewEmotionTag))
	{
		return;
	}

	const FEmoDisplayState OldBaseState = BaseEmotionState;
	BaseEmotionState.SharedEmotionTag = NewEmotionTag;
	OnRep_BaseEmotionState(OldBaseState);
	ForceOwnerNetUpdate();
}

void UEmoComponent::SetEmotionTagForPlayerSlotTag(const FGameplayTag PlayerSlotTag, const FGameplayTag NewEmotionTag)
{
	if (!IsAuthorityOwner())
	{
		return;
	}

	const FGameplayTag Existing = GetStateSlotTag(BaseEmotionState, PlayerSlotTag);
	if (AreTagsEqual(Existing, NewEmotionTag))
	{
		return;
	}

	const FEmoDisplayState OldBaseState = BaseEmotionState;
	SetStateSlotTag(BaseEmotionState, PlayerSlotTag, NewEmotionTag);
	OnRep_BaseEmotionState(OldBaseState);
	ForceOwnerNetUpdate();
}

void UEmoComponent::ClearEmotionTag()
{
	SetEmotionTag(FGameplayTag());
}

void UEmoComponent::ClearEmotionTagForPlayerSlotTag(const FGameplayTag PlayerSlotTag)
{
	SetEmotionTagForPlayerSlotTag(PlayerSlotTag, FGameplayTag());
}

void UEmoComponent::ClearAllEmotionTags()
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

	const FEmoDisplayState OldBaseState = BaseEmotionState;
	BaseEmotionState = FEmoDisplayState();
	OnRep_BaseEmotionState(OldBaseState);
	ForceOwnerNetUpdate();
}

void UEmoComponent::SetDialogueEmotionTag(const FGameplayTag NewEmotionTag)
{
	if (!IsAuthorityOwner() || AreTagsEqual(DialogueOverrideState.SharedEmotionTag, NewEmotionTag))
	{
		return;
	}

	const FEmoDisplayState OldDialogueState = DialogueOverrideState;
	DialogueOverrideState.SharedEmotionTag = NewEmotionTag;
	OnRep_DialogueOverrideState(OldDialogueState);
	ForceOwnerNetUpdate();
}

void UEmoComponent::SetDialogueEmotionTagForPlayerSlotTag(const FGameplayTag PlayerSlotTag, const FGameplayTag NewEmotionTag)
{
	if (!IsAuthorityOwner())
	{
		return;
	}

	const FGameplayTag Existing = GetStateSlotTag(DialogueOverrideState, PlayerSlotTag);
	if (AreTagsEqual(Existing, NewEmotionTag))
	{
		return;
	}

	const FEmoDisplayState OldDialogueState = DialogueOverrideState;
	SetStateSlotTag(DialogueOverrideState, PlayerSlotTag, NewEmotionTag);
	OnRep_DialogueOverrideState(OldDialogueState);
	ForceOwnerNetUpdate();
}

void UEmoComponent::ClearDialogueEmotionTag()
{
	SetDialogueEmotionTag(FGameplayTag());
}

void UEmoComponent::ClearDialogueEmotionTagForPlayerSlotTag(const FGameplayTag PlayerSlotTag)
{
	SetDialogueEmotionTagForPlayerSlotTag(PlayerSlotTag, FGameplayTag());
}

void UEmoComponent::ClearAllDialogueEmotionTags()
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

	const FEmoDisplayState OldDialogueState = DialogueOverrideState;
	DialogueOverrideState = FEmoDisplayState();
	OnRep_DialogueOverrideState(OldDialogueState);
	ForceOwnerNetUpdate();
}

void UEmoComponent::SetSystemEmotionTag(const FName SourceId, const FGameplayTag NewEmotionTag, const int32 Priority)
{
	if (!IsAuthorityOwner() || SourceId.IsNone())
	{
		return;
	}

	const int32 OldSourceCount = SystemEmotionSources.Num();
	FSystemEmotionSourceState& SourceState = SystemEmotionSources.FindOrAdd(SourceId);
	SourceState.Priority = Priority;
	SourceState.SharedWriteSerial = NextSystemEmotionWriteSerial++;
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

	if (OldSourceCount != SystemEmotionSources.Num())
	{
		OnEmotionQueueChanged.Broadcast(SystemEmotionSources.Num());
	}
}

void UEmoComponent::SetSystemEmotionTagForDuration(const FName SourceId, const FGameplayTag NewEmotionTag, const float DurationSeconds, const int32 Priority)
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

void UEmoComponent::SetSystemEmotionTagForPlayerSlotTag(
	const FName SourceId,
	const FGameplayTag PlayerSlotTag,
	const FGameplayTag NewEmotionTag,
	const int32 Priority)
{
	if (!IsAuthorityOwner() || SourceId.IsNone())
	{
		return;
	}

	const int32 OldSourceCount = SystemEmotionSources.Num();
	FSystemEmotionSourceState& SourceState = SystemEmotionSources.FindOrAdd(SourceId);
	SourceState.Priority = Priority;
	if (IsP1SlotTag(PlayerSlotTag))
	{
		SourceState.P1WriteSerial = NextSystemEmotionWriteSerial++;
	}
	else if (IsP2SlotTag(PlayerSlotTag))
	{
		SourceState.P2WriteSerial = NextSystemEmotionWriteSerial++;
	}
	else
	{
		SourceState.SharedWriteSerial = NextSystemEmotionWriteSerial++;
	}
	SetStateSlotTag(SourceState.State, PlayerSlotTag, NewEmotionTag);
	if (!HasAnyStateTag(SourceState.State))
	{
		ClearAllTimedSystemOverrideTimersForSource(SourceId);
		SystemEmotionSources.Remove(SourceId);
	}

	if (RebuildSystemOverrideStateFromSources())
	{
		ForceOwnerNetUpdate();
	}

	if (OldSourceCount != SystemEmotionSources.Num())
	{
		OnEmotionQueueChanged.Broadcast(SystemEmotionSources.Num());
	}
}

void UEmoComponent::SetSystemEmotionTagForPlayerSlotTagForDuration(
	const FName SourceId,
	const FGameplayTag PlayerSlotTag,
	const FGameplayTag NewEmotionTag,
	const float DurationSeconds,
	const int32 Priority)
{
	SetSystemEmotionTagForPlayerSlotTag(SourceId, PlayerSlotTag, NewEmotionTag, Priority);
	if (!IsAuthorityOwner() || SourceId.IsNone())
	{
		return;
	}

	if (!NewEmotionTag.IsValid())
	{
		ClearTimedSystemOverrideSlotTimer(SourceId, PlayerSlotTag);
		return;
	}

	const float EffectiveDuration = ResolveTimedSystemOverrideDurationSeconds(DurationSeconds);
	SetTimedSystemOverrideSlotClearTimer(SourceId, PlayerSlotTag, EffectiveDuration);
}

void UEmoComponent::ClearSystemEmotionTag(const FName SourceId)
{
	if (!IsAuthorityOwner() || SourceId.IsNone())
	{
		return;
	}

	const int32 OldSourceCount = SystemEmotionSources.Num();
	ClearTimedSystemOverrideTimer(SourceId);
	if (FSystemEmotionSourceState* SourceState = SystemEmotionSources.Find(SourceId))
	{
		SourceState->SharedWriteSerial = NextSystemEmotionWriteSerial++;
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

	if (OldSourceCount != SystemEmotionSources.Num())
	{
		OnEmotionQueueChanged.Broadcast(SystemEmotionSources.Num());
	}
}

void UEmoComponent::ClearSystemEmotionTagForPlayerSlotTag(const FName SourceId, const FGameplayTag PlayerSlotTag)
{
	if (!IsAuthorityOwner() || SourceId.IsNone())
	{
		return;
	}

	const int32 OldSourceCount = SystemEmotionSources.Num();
	ClearTimedSystemOverrideSlotTimer(SourceId, PlayerSlotTag);
	if (FSystemEmotionSourceState* SourceState = SystemEmotionSources.Find(SourceId))
	{
		if (IsP1SlotTag(PlayerSlotTag))
		{
			SourceState->P1WriteSerial = NextSystemEmotionWriteSerial++;
		}
		else if (IsP2SlotTag(PlayerSlotTag))
		{
			SourceState->P2WriteSerial = NextSystemEmotionWriteSerial++;
		}
		else
		{
			SourceState->SharedWriteSerial = NextSystemEmotionWriteSerial++;
		}
		SetStateSlotTag(SourceState->State, PlayerSlotTag, FGameplayTag());
		if (!HasAnyStateTag(SourceState->State))
		{
			SystemEmotionSources.Remove(SourceId);
		}
	}

	if (RebuildSystemOverrideStateFromSources())
	{
		ForceOwnerNetUpdate();
	}

	if (OldSourceCount != SystemEmotionSources.Num())
	{
		OnEmotionQueueChanged.Broadcast(SystemEmotionSources.Num());
	}
}

void UEmoComponent::ClearAllSystemEmotionTagsForSource(const FName SourceId)
{
	if (!IsAuthorityOwner() || SourceId.IsNone())
	{
		return;
	}

	const int32 OldSourceCount = SystemEmotionSources.Num();
	ClearAllTimedSystemOverrideTimersForSource(SourceId);
	if (SystemEmotionSources.Remove(SourceId) <= 0)
	{
		return;
	}

	if (RebuildSystemOverrideStateFromSources())
	{
		ForceOwnerNetUpdate();
	}

	if (OldSourceCount != SystemEmotionSources.Num())
	{
		OnEmotionQueueChanged.Broadcast(SystemEmotionSources.Num());
	}
}

void UEmoComponent::ClearAllSystemEmotionTags()
{
	if (!IsAuthorityOwner() || SystemEmotionSources.IsEmpty())
	{
		return;
	}

	const int32 OldSourceCount = SystemEmotionSources.Num();
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

	if (OldSourceCount != SystemEmotionSources.Num())
	{
		OnEmotionQueueChanged.Broadcast(SystemEmotionSources.Num());
	}
}

FGameplayTag UEmoComponent::GetDisplayedEmotionTagForPlayerSlotTag(const FGameplayTag PlayerSlotTag) const
{
	const FGameplayTag SystemSlotTag = GetStateSlotTag(SystemOverrideState, PlayerSlotTag);
	if (SystemSlotTag.IsValid())
	{
		return SystemSlotTag;
	}

	if (SystemOverrideState.SharedEmotionTag.IsValid())
	{
		return SystemOverrideState.SharedEmotionTag;
	}

	const FGameplayTag DialogueSlotTag = GetStateSlotTag(DialogueOverrideState, PlayerSlotTag);
	if (DialogueSlotTag.IsValid())
	{
		return DialogueSlotTag;
	}

	if (DialogueOverrideState.SharedEmotionTag.IsValid())
	{
		return DialogueOverrideState.SharedEmotionTag;
	}

	const FGameplayTag BaseSlotTag = GetStateSlotTag(BaseEmotionState, PlayerSlotTag);
	if (BaseSlotTag.IsValid())
	{
		return BaseSlotTag;
	}

	return BaseEmotionState.SharedEmotionTag;
}

FGameplayTag UEmoComponent::GetDisplayedEmotionTagForController(const APlayerController* ViewerController) const
{
	return GetDisplayedEmotionTagForPlayerSlotTag(ResolveViewerSlotTag(ViewerController));
}

bool UEmoComponent::TryResolveDisplayedEmotionIconForPlayerSlot(
	const FGameplayTag PlayerSlotTag,
	TSoftObjectPtr<UTexture2D>& OutIconTexture,
	FGameplayTag& OutResolvedEmotionTag) const
{
	const FGameplayTag DisplayTag = GetDisplayedEmotionTagForPlayerSlotTag(PlayerSlotTag);
	return TryResolveEmotionIconForTag(DisplayTag, OutIconTexture, OutResolvedEmotionTag);
}

bool UEmoComponent::TryResolveDisplayedEmotionIconForController(
	const APlayerController* ViewerController,
	TSoftObjectPtr<UTexture2D>& OutIconTexture,
	FGameplayTag& OutResolvedEmotionTag) const
{
	return TryResolveDisplayedEmotionIconForPlayerSlot(ResolveViewerSlotTag(ViewerController), OutIconTexture, OutResolvedEmotionTag);
}

bool UEmoComponent::TryResolveEmotionIconForTag(
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
		if (UEmoResolverSubsystem* Resolver = GameInstance->GetSubsystem<UEmoResolverSubsystem>())
		{
			return Resolver->TryResolveEmotionIcon(EmotionTag, OutIconTexture, OutResolvedEmotionTag);
		}
	}

	return UEmoResolverSubsystem::TryResolveEmotionIconFromConfiguredData(EmotionTag, OutIconTexture, OutResolvedEmotionTag);
}

bool UEmoComponent::TryResolvePreviewEmotionIcon(
	TSoftObjectPtr<UTexture2D>& OutIconTexture,
	FGameplayTag& OutResolvedEmotionTag) const
{
	const FGameplayTag PreviewTag = GetPreviewEmotionTag();
	return TryResolveEmotionIconForTag(PreviewTag, OutIconTexture, OutResolvedEmotionTag);
}

FGameplayTag UEmoComponent::GetPreviewEmotionTag() const
{
	if (PreviewEmotionTag.IsValid())
	{
		return PreviewEmotionTag;
	}

	return UGameplayTagsManager::Get().RequestGameplayTag(FName(TEXT("Parley.Emotion.Preview")), false);
}

FVector UEmoComponent::GetEmotionAnchorWorldLocation() const
{
	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return FVector::ZeroVector;
	}

	FVector EffectiveOffset = AnchorWorldOffset;
	if (bUseSettingsDefaultAnchorWorldOffset)
	{
		if (const UEmoSettings* Settings = GetDefault<UEmoSettings>())
		{
			if (EffectiveOffset.IsNearlyZero() && !Settings->DefaultAnchorWorldOffset.IsNearlyZero())
			{
				EffectiveOffset = Settings->DefaultAnchorWorldOffset;
			}
		}
	}

	FVector Origin = FVector::ZeroVector;
	FVector Extent = FVector::ZeroVector;
	OwnerActor->GetActorBounds(true, Origin, Extent);
	return Origin + FVector(0.0f, 0.0f, Extent.Z) + EffectiveOffset;
}

bool UEmoComponent::GetEmotionFacingRotationForController(const APlayerController* ViewerController, FRotator& OutFacingRotation) const
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

void UEmoComponent::SetRegisteredSpeakerTag(const FGameplayTag NewSpeakerTag)
{
	if (!IsAuthorityOwner() || AreTagsEqual(RegisteredSpeakerTag, NewSpeakerTag))
	{
		return;
	}

	RegisteredSpeakerTag = NewSpeakerTag;
	ForceOwnerNetUpdate();
}

FGameplayTag UEmoComponent::GetStateSlotTag(const FEmoDisplayState& State, const FGameplayTag& PlayerSlotTag)
{
	if (IsP1SlotTag(PlayerSlotTag))
	{
		return State.P1EmotionTag;
	}

	if (IsP2SlotTag(PlayerSlotTag))
	{
		return State.P2EmotionTag;
	}

	return FGameplayTag();
}

void UEmoComponent::SetStateSlotTag(FEmoDisplayState& State, const FGameplayTag& PlayerSlotTag, const FGameplayTag& EmotionTag)
{
	if (IsP1SlotTag(PlayerSlotTag))
	{
		State.P1EmotionTag = EmotionTag;
		return;
	}

	if (IsP2SlotTag(PlayerSlotTag))
	{
		State.P2EmotionTag = EmotionTag;
	}
}

bool UEmoComponent::AreDisplayStatesEqual(const FEmoDisplayState& Left, const FEmoDisplayState& Right)
{
	return AreTagsEqual(Left.SharedEmotionTag, Right.SharedEmotionTag)
		&& AreTagsEqual(Left.P1EmotionTag, Right.P1EmotionTag)
		&& AreTagsEqual(Left.P2EmotionTag, Right.P2EmotionTag);
}

bool UEmoComponent::HasAnyStateTag(const FEmoDisplayState& State)
{
	return State.SharedEmotionTag.IsValid() || State.P1EmotionTag.IsValid() || State.P2EmotionTag.IsValid();
}

FName UEmoComponent::MakeTimedSlotKey(const FName SourceId, const FGameplayTag& SlotTag)
{
	return FName(*FString::Printf(TEXT("%s|%s"), *SourceId.ToString(), *SlotTag.ToString()));
}

bool UEmoComponent::RebuildSystemOverrideStateFromSources()
{
	FEmoDisplayState ResolvedState;
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
			&& ShouldReplace(SourceState.Priority, SourceState.SharedWriteSerial, SharedPriority, SharedSerial))
		{
			ResolvedState.SharedEmotionTag = SourceState.State.SharedEmotionTag;
			SharedPriority = SourceState.Priority;
			SharedSerial = SourceState.SharedWriteSerial;
		}

		if (SourceState.State.P1EmotionTag.IsValid()
			&& ShouldReplace(SourceState.Priority, SourceState.P1WriteSerial, P1Priority, P1Serial))
		{
			ResolvedState.P1EmotionTag = SourceState.State.P1EmotionTag;
			P1Priority = SourceState.Priority;
			P1Serial = SourceState.P1WriteSerial;
		}

		if (SourceState.State.P2EmotionTag.IsValid()
			&& ShouldReplace(SourceState.Priority, SourceState.P2WriteSerial, P2Priority, P2Serial))
		{
			ResolvedState.P2EmotionTag = SourceState.State.P2EmotionTag;
			P2Priority = SourceState.Priority;
			P2Serial = SourceState.P2WriteSerial;
		}
	}

	if (AreDisplayStatesEqual(SystemOverrideState, ResolvedState))
	{
		return false;
	}

	const FEmoDisplayState OldState = SystemOverrideState;
	SystemOverrideState = ResolvedState;
	OnRep_SystemOverrideState(OldState);
	return true;
}

float UEmoComponent::ResolveTimedSystemOverrideDurationSeconds(const float RequestedDurationSeconds) const
{
	if (RequestedDurationSeconds > 0.0f)
	{
		return RequestedDurationSeconds;
	}

	const UEmoSettings* Settings = GetDefault<UEmoSettings>();
	const float DefaultDuration = Settings ? Settings->DefaultTimedSystemOverrideDurationSeconds : 1.5f;
	return FMath::Max(0.01f, DefaultDuration);
}

void UEmoComponent::SetTimedSystemOverrideClearTimer(const FName SourceId, const float DurationSeconds)
{
	UWorld* World = GetWorld();
	if (!World || SourceId.IsNone())
	{
		return;
	}

	FTimerHandle& Handle = TimedSystemOverrideClearHandles.FindOrAdd(SourceId);
	FTimerDelegate Delegate;
	Delegate.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(UEmoComponent, HandleTimedSystemOverrideClear), SourceId);
	World->GetTimerManager().SetTimer(Handle, Delegate, FMath::Max(0.01f, DurationSeconds), false);
}

void UEmoComponent::SetTimedSystemOverrideSlotClearTimer(const FName SourceId, const FGameplayTag& SlotTag, const float DurationSeconds)
{
	UWorld* World = GetWorld();
	if (!World || SourceId.IsNone() || !SlotTag.IsValid())
	{
		return;
	}

	const FName TimerKey = MakeTimedSlotKey(SourceId, SlotTag);
	FTimerHandle& Handle = TimedSystemOverrideSlotClearHandles.FindOrAdd(TimerKey);
	FTimerDelegate Delegate;
	Delegate.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(UEmoComponent, HandleTimedSystemOverrideSlotClear), SourceId, SlotTag);
	World->GetTimerManager().SetTimer(Handle, Delegate, FMath::Max(0.01f, DurationSeconds), false);
}

void UEmoComponent::ClearTimedSystemOverrideTimer(const FName SourceId)
{
	UWorld* World = GetWorld();
	FTimerHandle* Handle = TimedSystemOverrideClearHandles.Find(SourceId);
	if (World && Handle)
	{
		World->GetTimerManager().ClearTimer(*Handle);
	}

	TimedSystemOverrideClearHandles.Remove(SourceId);
}

void UEmoComponent::ClearTimedSystemOverrideSlotTimer(const FName SourceId, const FGameplayTag& SlotTag)
{
	if (!SlotTag.IsValid())
	{
		return;
	}

	const FName TimerKey = MakeTimedSlotKey(SourceId, SlotTag);
	UWorld* World = GetWorld();
	FTimerHandle* Handle = TimedSystemOverrideSlotClearHandles.Find(TimerKey);
	if (World && Handle)
	{
		World->GetTimerManager().ClearTimer(*Handle);
	}

	TimedSystemOverrideSlotClearHandles.Remove(TimerKey);
}

void UEmoComponent::ClearAllTimedSystemOverrideTimersForSource(const FName SourceId)
{
	ClearTimedSystemOverrideTimer(SourceId);
	ClearTimedSystemOverrideSlotTimer(SourceId, GetP1SlotTag());
	ClearTimedSystemOverrideSlotTimer(SourceId, GetP2SlotTag());
}

void UEmoComponent::HandleTimedSystemOverrideClear(const FName SourceId)
{
	ClearTimedSystemOverrideTimer(SourceId);
	ClearSystemEmotionTag(SourceId);
}

void UEmoComponent::HandleTimedSystemOverrideSlotClear(const FName SourceId, const FGameplayTag SlotTag)
{
	ClearTimedSystemOverrideSlotTimer(SourceId, SlotTag);
	ClearSystemEmotionTagForPlayerSlotTag(SourceId, SlotTag);
}

bool UEmoComponent::IsAuthorityOwner() const
{
	const AActor* OwnerActor = GetOwner();
	return OwnerActor && OwnerActor->HasAuthority();
}

void UEmoComponent::ForceOwnerNetUpdate() const
{
	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->ForceNetUpdate();
	}
}

void UEmoComponent::BroadcastDisplayStateDelta(
	const FEmoDisplayState& OldBaseState,
	const FEmoDisplayState& OldDialogueState,
	const FEmoDisplayState& OldSystemState)
{
	const FGameplayTag OldDisplayedTag = ResolveDisplayedEmotionTagFromStates(
		OldBaseState,
		OldDialogueState,
		OldSystemState,
		FGameplayTag());
	const FGameplayTag OldP1DisplayedTag = ResolveDisplayedEmotionTagFromStates(
		OldBaseState,
		OldDialogueState,
		OldSystemState,
		GetP1SlotTag());
	const FGameplayTag OldP2DisplayedTag = ResolveDisplayedEmotionTagFromStates(
		OldBaseState,
		OldDialogueState,
		OldSystemState,
		GetP2SlotTag());

	const FGameplayTag NewDisplayedTag = GetDisplayedEmotionTagForPlayerSlotTag(FGameplayTag());
	const FGameplayTag NewP1DisplayedTag = GetDisplayedEmotionTagForPlayerSlotTag(GetP1SlotTag());
	const FGameplayTag NewP2DisplayedTag = GetDisplayedEmotionTagForPlayerSlotTag(GetP2SlotTag());
	const bool bAnyDisplayChanged = !AreTagsEqual(OldDisplayedTag, NewDisplayedTag)
		|| !AreTagsEqual(OldP1DisplayedTag, NewP1DisplayedTag)
		|| !AreTagsEqual(OldP2DisplayedTag, NewP2DisplayedTag);
	OnEmotionDisplayStateChanged.Broadcast();

	if (bAnyDisplayChanged)
	{
		OnEmotionDisplayChanged.Broadcast(NewDisplayedTag, OldDisplayedTag);
		if (!NewDisplayedTag.IsValid())
		{
			OnEmotionDisplayCleared.Broadcast();
		}

		if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
		{
			if (UEmoResolverSubsystem* Resolver = GameInstance->GetSubsystem<UEmoResolverSubsystem>())
			{
				Resolver->NotifyComponentEmotionChanged(this, NewDisplayedTag);
			}
		}
	}

#if WITH_EDITOR
	RefreshEditorPreviewBillboard();
#endif
}

void UEmoComponent::OnRep_BaseEmotionState(const FEmoDisplayState OldState)
{
	if (!AreDisplayStatesEqual(OldState, BaseEmotionState))
	{
		BroadcastDisplayStateDelta(OldState, DialogueOverrideState, SystemOverrideState);
	}
}

void UEmoComponent::OnRep_DialogueOverrideState(const FEmoDisplayState OldState)
{
	if (!AreDisplayStatesEqual(OldState, DialogueOverrideState))
	{
		BroadcastDisplayStateDelta(BaseEmotionState, OldState, SystemOverrideState);
	}
}

void UEmoComponent::OnRep_SystemOverrideState(const FEmoDisplayState OldState)
{
	if (!AreDisplayStatesEqual(OldState, SystemOverrideState))
	{
		BroadcastDisplayStateDelta(BaseEmotionState, DialogueOverrideState, OldState);
	}
}

void UEmoComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UEmoComponent, RegisteredSpeakerTag);
	DOREPLIFETIME(UEmoComponent, BaseEmotionState);
	DOREPLIFETIME(UEmoComponent, DialogueOverrideState);
	DOREPLIFETIME(UEmoComponent, SystemOverrideState);
}

#if WITH_EDITOR
void UEmoComponent::RefreshEditorPreviewBillboard()
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

	if (USceneComponent* RootComponent = OwnerActor->GetRootComponent())
	{
		if (EditorPreviewBillboardComponent->GetAttachParent() != RootComponent)
		{
			EditorPreviewBillboardComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
		}
	}

	EditorPreviewBillboardComponent->SetSprite(LoadedTexture);
	EditorPreviewBillboardComponent->SetVisibility(true, true);

	const float PreviewScale = FMath::Max(0.05f, IconScreenSize / 64.0f);
	EditorPreviewBillboardComponent->SetRelativeScale3D(FVector(PreviewScale));

	EditorPreviewBillboardComponent->SetWorldLocation(GetEmotionAnchorWorldLocation());
#endif
}

void UEmoComponent::DestroyEditorPreviewBillboard()
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
