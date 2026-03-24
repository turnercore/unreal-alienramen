#include "EmoComponent.h"

#include "EmoComponentRegistrySubsystem.h"
#include "EmoLog.h"
#include "EmoResolverSubsystem.h"
#include "EmoSettings.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagsManager.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#if WITH_EDITOR
#include "Components/BillboardComponent.h"
#endif

namespace
{
	struct FResolvedEmotionRegistrationResult
	{
		FGameplayTag EmotionTag;
		int32 WinningPriority = TNumericLimits<int32>::Lowest();
		bool bUsedTargetedRegistration = false;
		TArray<int32> TiedTargetedIndices;
	};

	static bool HasAnyExactSharedTag(const FGameplayTagContainer& Left, const FGameplayTagContainer& Right)
	{
		if (Left.IsEmpty() || Right.IsEmpty())
		{
			return false;
		}

		TArray<FGameplayTag> LeftTags;
		Left.GetGameplayTagArray(LeftTags);
		for (const FGameplayTag& Tag : LeftTags)
		{
			if (Right.HasTagExact(Tag))
			{
				return true;
			}
		}

		return false;
	}

	static FString BuildSortedViewerTagKey(const FGameplayTagContainer& ViewerTags)
	{
		TArray<FGameplayTag> Tags;
		ViewerTags.GetGameplayTagArray(Tags);

		TArray<FString> TagStrings;
		TagStrings.Reserve(Tags.Num());
		for (const FGameplayTag& Tag : Tags)
		{
			if (Tag.IsValid())
			{
				TagStrings.Add(Tag.ToString());
			}
		}

		TagStrings.Sort();
		return FString::Join(TagStrings, TEXT("|"));
	}

	static void MaybeLogTargetedConflict(
		const UEmoComponent* Component,
		const FGameplayTagContainer& ViewerTags,
		const TArray<FEmoEmotionRegistration>& Registrations,
		const FResolvedEmotionRegistrationResult& Result)
	{
		if (!Component || Result.TiedTargetedIndices.Num() <= 1)
		{
			return;
		}

		TArray<FString> EntryDescriptions;
		EntryDescriptions.Reserve(Result.TiedTargetedIndices.Num());
		TArray<FString> SerialStrings;
		SerialStrings.Reserve(Result.TiedTargetedIndices.Num());
		for (const int32 Index : Result.TiedTargetedIndices)
		{
			if (!Registrations.IsValidIndex(Index))
			{
				continue;
			}

			const FEmoEmotionRegistration& Registration = Registrations[Index];
			EntryDescriptions.Add(FString::Printf(
				TEXT("%s:%s:[%s]"),
				*Registration.SourceId.ToString(),
				*Registration.EmotionTag.ToString(),
				*BuildSortedViewerTagKey(Registration.TargetViewerTags)));
			SerialStrings.Add(LexToString(Registration.WriteSerial));
		}

		static TSet<FString> LoggedConflictSignatures;
		const FString ConflictSignature = FString::Printf(
			TEXT("%s|%s|%d|%s"),
			*GetPathNameSafe(Component),
			*BuildSortedViewerTagKey(ViewerTags),
			Result.WinningPriority,
			*FString::Join(SerialStrings, TEXT(",")));
		if (LoggedConflictSignatures.Contains(ConflictSignature))
		{
			return;
		}

		LoggedConflictSignatures.Add(ConflictSignature);
		UE_LOG(
			EmoLog,
			Warning,
			TEXT("[Emotion] Same-priority targeted emotion registrations matched viewer tags on '%s'. Priority=%d Viewer=[%s] Entries=[%s]. Latest write wins deterministically."),
			*GetNameSafe(Component->GetOwner()),
			Result.WinningPriority,
			*BuildSortedViewerTagKey(ViewerTags),
			*FString::Join(EntryDescriptions, TEXT(", ")));
	}

	static FResolvedEmotionRegistrationResult ResolveDisplayedRegistration(
		const UEmoComponent* Component,
		const TArray<FEmoEmotionRegistration>& Registrations,
		const FGameplayTagContainer& ViewerTags)
	{
		FResolvedEmotionRegistrationResult Result;
		TArray<int32> TargetedIndices;
		TArray<int32> GlobalIndices;

		for (int32 Index = 0; Index < Registrations.Num(); ++Index)
		{
			const FEmoEmotionRegistration& Registration = Registrations[Index];
			if (!Registration.EmotionTag.IsValid())
			{
				continue;
			}

			const bool bIsGlobalRegistration = Registration.TargetViewerTags.IsEmpty();
			const bool bMatchesTargetedViewer = !bIsGlobalRegistration && HasAnyExactSharedTag(Registration.TargetViewerTags, ViewerTags);
			if (!bIsGlobalRegistration && !bMatchesTargetedViewer)
			{
				continue;
			}

			if (Registration.Priority > Result.WinningPriority)
			{
				Result.WinningPriority = Registration.Priority;
				TargetedIndices.Reset();
				GlobalIndices.Reset();
			}

			if (Registration.Priority != Result.WinningPriority)
			{
				continue;
			}

			if (bIsGlobalRegistration)
			{
				GlobalIndices.Add(Index);
			}
			else
			{
				TargetedIndices.Add(Index);
			}
		}

		auto ResolveLatestWrite = [&Registrations](const TArray<int32>& CandidateIndices) -> FGameplayTag
		{
			const FEmoEmotionRegistration* BestRegistration = nullptr;
			for (const int32 CandidateIndex : CandidateIndices)
			{
				if (!Registrations.IsValidIndex(CandidateIndex))
				{
					continue;
				}

				const FEmoEmotionRegistration& Candidate = Registrations[CandidateIndex];
				if (!BestRegistration || Candidate.WriteSerial > BestRegistration->WriteSerial)
				{
					BestRegistration = &Candidate;
				}
			}

			return BestRegistration ? BestRegistration->EmotionTag : FGameplayTag();
		};

		if (!TargetedIndices.IsEmpty())
		{
			Result.EmotionTag = ResolveLatestWrite(TargetedIndices);
			Result.bUsedTargetedRegistration = true;
			Result.TiedTargetedIndices = TargetedIndices;
			MaybeLogTargetedConflict(Component, ViewerTags, Registrations, Result);
			return Result;
		}

		if (!GlobalIndices.IsEmpty())
		{
			Result.EmotionTag = ResolveLatestWrite(GlobalIndices);
		}

		return Result;
	}

#if WITH_EDITOR
#if WITH_EDITORONLY_DATA
	static FName GetEditorPreviewBillboardComponentName(const UEmoComponent* Component)
	{
		const FString BillboardName = FString::Printf(TEXT("EmoEditorPreviewBillboard_%s"), *GetNameSafe(Component));
		return FName(*BillboardName);
	}

	static bool GetEditorPreviewAnchorLocationExcludingBillboards(
		const AActor* OwnerActor,
		const UBillboardComponent* PreviewBillboardComponent,
		FVector& OutAnchorLocation)
	{
		OutAnchorLocation = FVector::ZeroVector;
		if (!OwnerActor)
		{
			return false;
		}

		bool bFoundBounds = false;
		FBox CombinedBounds(ForceInit);
		TArray<UPrimitiveComponent*> PrimitiveComponents;
		OwnerActor->GetComponents(PrimitiveComponents);
		for (const UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
		{
			if (!PrimitiveComponent || PrimitiveComponent == PreviewBillboardComponent || !PrimitiveComponent->IsRegistered())
			{
				continue;
			}

			const FBox ComponentBounds = PrimitiveComponent->Bounds.GetBox();
			if (!ComponentBounds.IsValid)
			{
				continue;
			}

			CombinedBounds += ComponentBounds;
			bFoundBounds = true;
		}

		if (!bFoundBounds)
		{
			return false;
		}

		FVector BoundsOrigin = FVector::ZeroVector;
		FVector BoundsExtent = FVector::ZeroVector;
		CombinedBounds.GetCenterAndExtents(BoundsOrigin, BoundsExtent);
		OutAnchorLocation = BoundsOrigin + FVector(0.0f, 0.0f, BoundsExtent.Z);
		return true;
	}
#endif
#endif
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

	TimedEmotionRegistrationClearHandles.Reset();
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

void UEmoComponent::SetEmotionRegistration(
	const FName SourceId,
	const FGameplayTag NewEmotionTag,
	const int32 Priority,
	FGameplayTagContainer TargetViewerTags)
{
	if (!IsAuthorityOwner() || SourceId.IsNone())
	{
		return;
	}

	TargetViewerTags = SanitizeViewerTags(TargetViewerTags);
	if (!NewEmotionTag.IsValid())
	{
		ClearEmotionRegistration(SourceId, TargetViewerTags);
		return;
	}

	const int32 ExistingIndex = FindEmotionRegistrationIndex(SourceId, TargetViewerTags);
	if (EmotionRegistrations.IsValidIndex(ExistingIndex))
	{
		const FEmoEmotionRegistration& Existing = EmotionRegistrations[ExistingIndex];
		if (AreTagsEqual(Existing.EmotionTag, NewEmotionTag)
			&& Existing.Priority == Priority)
		{
			return;
		}
	}

	const TArray<FEmoEmotionRegistration> OldRegistrations = EmotionRegistrations;
	FEmoEmotionRegistration* Registration = nullptr;
	if (EmotionRegistrations.IsValidIndex(ExistingIndex))
	{
		Registration = &EmotionRegistrations[ExistingIndex];
	}
	else
	{
		Registration = &EmotionRegistrations.AddDefaulted_GetRef();
		Registration->SourceId = SourceId;
		Registration->TargetViewerTags = TargetViewerTags;
	}

	Registration->SourceId = SourceId;
	Registration->EmotionTag = NewEmotionTag;
	Registration->Priority = Priority;
	Registration->TargetViewerTags = TargetViewerTags;
	Registration->WriteSerial = NextEmotionWriteSerial++;

	OnRep_EmotionRegistrations(OldRegistrations);
	ForceOwnerNetUpdate();
}

void UEmoComponent::SetEmotionRegistrationForDuration(
	const FName SourceId,
	const FGameplayTag NewEmotionTag,
	const float DurationSeconds,
	const int32 Priority,
	FGameplayTagContainer TargetViewerTags)
{
	TargetViewerTags = SanitizeViewerTags(TargetViewerTags);
	SetEmotionRegistration(SourceId, NewEmotionTag, Priority, TargetViewerTags);
	if (!IsAuthorityOwner() || SourceId.IsNone())
	{
		return;
	}

	if (!NewEmotionTag.IsValid())
	{
		ClearTimedEmotionRegistrationTimer(SourceId, TargetViewerTags);
		return;
	}

	const float EffectiveDuration = ResolveTimedEmotionRegistrationDurationSeconds(DurationSeconds);
	SetTimedEmotionRegistrationClearTimer(SourceId, TargetViewerTags, EffectiveDuration);
}

void UEmoComponent::ClearEmotionRegistration(FName SourceId, FGameplayTagContainer TargetViewerTags)
{
	if (!IsAuthorityOwner() || SourceId.IsNone())
	{
		return;
	}

	TargetViewerTags = SanitizeViewerTags(TargetViewerTags);
	const int32 ExistingIndex = FindEmotionRegistrationIndex(SourceId, TargetViewerTags);
	if (!EmotionRegistrations.IsValidIndex(ExistingIndex))
	{
		return;
	}

	const TArray<FEmoEmotionRegistration> OldRegistrations = EmotionRegistrations;
	ClearTimedEmotionRegistrationTimer(SourceId, TargetViewerTags);
	EmotionRegistrations.RemoveAt(ExistingIndex);
	OnRep_EmotionRegistrations(OldRegistrations);
	ForceOwnerNetUpdate();
}

void UEmoComponent::ClearAllEmotionRegistrationsForSource(const FName SourceId)
{
	if (!IsAuthorityOwner() || SourceId.IsNone())
	{
		return;
	}

	bool bFoundAny = false;
	for (const FEmoEmotionRegistration& Registration : EmotionRegistrations)
	{
		if (Registration.SourceId == SourceId)
		{
			bFoundAny = true;
			break;
		}
	}

	if (!bFoundAny)
	{
		return;
	}

	const TArray<FEmoEmotionRegistration> OldRegistrations = EmotionRegistrations;
	ClearAllTimedEmotionRegistrationTimersForSource(SourceId);
	EmotionRegistrations.RemoveAll([SourceId](const FEmoEmotionRegistration& Registration)
		{
			return Registration.SourceId == SourceId;
		});
	OnRep_EmotionRegistrations(OldRegistrations);
	ForceOwnerNetUpdate();
}

void UEmoComponent::ClearAllEmotionRegistrations()
{
	if (!IsAuthorityOwner() || EmotionRegistrations.IsEmpty())
	{
		return;
	}

	const TArray<FEmoEmotionRegistration> OldRegistrations = EmotionRegistrations;
	for (TPair<FName, FTimerHandle>& Pair : TimedEmotionRegistrationClearHandles)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(Pair.Value);
		}
	}
	TimedEmotionRegistrationClearHandles.Reset();
	EmotionRegistrations.Reset();
	OnRep_EmotionRegistrations(OldRegistrations);
	ForceOwnerNetUpdate();
}

FGameplayTag UEmoComponent::GetDisplayedEmotionTagForViewerTags(FGameplayTagContainer ViewerTags) const
{
	ViewerTags = SanitizeViewerTags(ViewerTags);
	return ResolveDisplayedRegistration(this, EmotionRegistrations, ViewerTags).EmotionTag;
}

bool UEmoComponent::TryResolveDisplayedEmotionIconForViewerTags(
	FGameplayTagContainer ViewerTags,
	TSoftObjectPtr<UTexture2D>& OutIconTexture,
	FGameplayTag& OutResolvedEmotionTag) const
{
	const FGameplayTag DisplayTag = GetDisplayedEmotionTagForViewerTags(ViewerTags);
	return TryResolveEmotionIconForTag(DisplayTag, OutIconTexture, OutResolvedEmotionTag);
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

bool UEmoComponent::AreTagsEqual(const FGameplayTag& Left, const FGameplayTag& Right)
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

bool UEmoComponent::AreViewerTagContainersEquivalent(const FGameplayTagContainer& Left, const FGameplayTagContainer& Right)
{
	return Left.Num() == Right.Num() && Left.HasAllExact(Right) && Right.HasAllExact(Left);
}

bool UEmoComponent::AreRegistrationsEquivalent(const FEmoEmotionRegistration& Left, const FEmoEmotionRegistration& Right)
{
	return Left.SourceId == Right.SourceId
		&& AreTagsEqual(Left.EmotionTag, Right.EmotionTag)
		&& Left.Priority == Right.Priority
		&& Left.WriteSerial == Right.WriteSerial
		&& AreViewerTagContainersEquivalent(Left.TargetViewerTags, Right.TargetViewerTags);
}

FGameplayTagContainer UEmoComponent::SanitizeViewerTags(const FGameplayTagContainer& ViewerTags)
{
	FGameplayTagContainer SanitizedTags;

	TArray<FGameplayTag> ViewerTagArray;
	ViewerTags.GetGameplayTagArray(ViewerTagArray);
	for (const FGameplayTag& Tag : ViewerTagArray)
	{
		if (Tag.IsValid())
		{
			SanitizedTags.AddTag(Tag);
		}
	}

	return SanitizedTags;
}

FName UEmoComponent::MakeRegistrationTimerKey(const FName SourceId, const FGameplayTagContainer& TargetViewerTags)
{
	return FName(*FString::Printf(
		TEXT("%s|%s"),
		*SourceId.ToString(),
		*BuildSortedViewerTagKey(TargetViewerTags)));
}

int32 UEmoComponent::FindEmotionRegistrationIndex(const FName SourceId, const FGameplayTagContainer& TargetViewerTags) const
{
	for (int32 Index = 0; Index < EmotionRegistrations.Num(); ++Index)
	{
		const FEmoEmotionRegistration& Registration = EmotionRegistrations[Index];
		if (Registration.SourceId == SourceId
			&& AreViewerTagContainersEquivalent(Registration.TargetViewerTags, TargetViewerTags))
		{
			return Index;
		}
	}

	return INDEX_NONE;
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

float UEmoComponent::ResolveTimedEmotionRegistrationDurationSeconds(const float RequestedDurationSeconds) const
{
	if (RequestedDurationSeconds > 0.0f)
	{
		return RequestedDurationSeconds;
	}

	const UEmoSettings* Settings = GetDefault<UEmoSettings>();
	const float DefaultDuration = Settings ? Settings->DefaultTimedSystemOverrideDurationSeconds : 1.5f;
	return FMath::Max(0.01f, DefaultDuration);
}

void UEmoComponent::SetTimedEmotionRegistrationClearTimer(
	const FName SourceId,
	const FGameplayTagContainer& TargetViewerTags,
	const float DurationSeconds)
{
	UWorld* World = GetWorld();
	if (!World || SourceId.IsNone())
	{
		return;
	}

	const FGameplayTagContainer SanitizedTags = SanitizeViewerTags(TargetViewerTags);
	const FName TimerKey = MakeRegistrationTimerKey(SourceId, SanitizedTags);
	FTimerHandle& Handle = TimedEmotionRegistrationClearHandles.FindOrAdd(TimerKey);
	FTimerDelegate Delegate;
	Delegate.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(UEmoComponent, HandleTimedEmotionRegistrationClear), SourceId, SanitizedTags);
	World->GetTimerManager().SetTimer(Handle, Delegate, FMath::Max(0.01f, DurationSeconds), false);
}

void UEmoComponent::ClearTimedEmotionRegistrationTimer(FName SourceId, const FGameplayTagContainer& TargetViewerTags)
{
	const FName TimerKey = MakeRegistrationTimerKey(SourceId, SanitizeViewerTags(TargetViewerTags));
	UWorld* World = GetWorld();
	FTimerHandle* Handle = TimedEmotionRegistrationClearHandles.Find(TimerKey);
	if (World && Handle)
	{
		World->GetTimerManager().ClearTimer(*Handle);
	}

	TimedEmotionRegistrationClearHandles.Remove(TimerKey);
}

void UEmoComponent::ClearAllTimedEmotionRegistrationTimersForSource(const FName SourceId)
{
	TArray<FName> KeysToRemove;
	for (const TPair<FName, FTimerHandle>& Pair : TimedEmotionRegistrationClearHandles)
	{
		if (Pair.Key.ToString().StartsWith(SourceId.ToString() + TEXT("|")))
		{
			KeysToRemove.Add(Pair.Key);
		}
	}

	for (const FName TimerKey : KeysToRemove)
	{
		if (UWorld* World = GetWorld())
		{
			if (FTimerHandle* Handle = TimedEmotionRegistrationClearHandles.Find(TimerKey))
			{
				World->GetTimerManager().ClearTimer(*Handle);
			}
		}

		TimedEmotionRegistrationClearHandles.Remove(TimerKey);
	}
}

void UEmoComponent::HandleTimedEmotionRegistrationClear(const FName SourceId, FGameplayTagContainer TargetViewerTags)
{
	TargetViewerTags = SanitizeViewerTags(TargetViewerTags);
	ClearTimedEmotionRegistrationTimer(SourceId, TargetViewerTags);
	ClearEmotionRegistration(SourceId, TargetViewerTags);
}

void UEmoComponent::BroadcastRegistrationDelta(const TArray<FEmoEmotionRegistration>& OldRegistrations)
{
	const FGameplayTagContainer EmptyViewerTags;
	const FGameplayTag OldDisplayedTag = ResolveDisplayedRegistration(this, OldRegistrations, EmptyViewerTags).EmotionTag;
	const FGameplayTag NewDisplayedTag = GetDisplayedEmotionTagForViewerTags(EmptyViewerTags);
	const bool bGlobalDisplayChanged = !AreTagsEqual(OldDisplayedTag, NewDisplayedTag);

	OnEmotionDisplayStateChanged.Broadcast();

	if (bGlobalDisplayChanged)
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

	if (OldRegistrations.Num() != EmotionRegistrations.Num())
	{
		OnEmotionQueueChanged.Broadcast(EmotionRegistrations.Num());
	}

#if WITH_EDITOR
	RefreshEditorPreviewBillboard();
#endif
}

void UEmoComponent::OnRep_EmotionRegistrations(const TArray<FEmoEmotionRegistration>& OldRegistrations)
{
	if (OldRegistrations.Num() == EmotionRegistrations.Num())
	{
		bool bAnyChanged = false;
		for (int32 Index = 0; Index < EmotionRegistrations.Num(); ++Index)
		{
			if (!AreRegistrationsEquivalent(OldRegistrations[Index], EmotionRegistrations[Index]))
			{
				bAnyChanged = true;
				break;
			}
		}

		if (!bAnyChanged)
		{
			return;
		}
	}

	BroadcastRegistrationDelta(OldRegistrations);
}

void UEmoComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UEmoComponent, RegisteredSpeakerTag);
	DOREPLIFETIME(UEmoComponent, EmotionRegistrations);
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

	TArray<UBillboardComponent*> BillboardComponents;
	OwnerActor->GetComponents(BillboardComponents);

	UBillboardComponent* PreviewBillboardComponent = EditorPreviewBillboardComponent.Get();
	if (!IsValid(PreviewBillboardComponent))
	{
		const FName ExpectedBillboardName = GetEditorPreviewBillboardComponentName(this);
		for (UBillboardComponent* ExistingBillboardComponent : BillboardComponents)
		{
			if (ExistingBillboardComponent && ExistingBillboardComponent->GetFName() == ExpectedBillboardName)
			{
				PreviewBillboardComponent = ExistingBillboardComponent;
				break;
			}
		}

		if (!PreviewBillboardComponent && BillboardComponents.Num() == 1)
		{
			PreviewBillboardComponent = BillboardComponents[0];
		}

		EditorPreviewBillboardComponent = PreviewBillboardComponent;
	}

	if (PreviewBillboardComponent)
	{
		for (UBillboardComponent* ExistingBillboardComponent : BillboardComponents)
		{
			if (ExistingBillboardComponent && ExistingBillboardComponent != PreviewBillboardComponent)
			{
				ExistingBillboardComponent->DestroyComponent();
			}
		}
	}
	else
	{
		const FName ExpectedBillboardName = GetEditorPreviewBillboardComponentName(this);
		PreviewBillboardComponent = NewObject<UBillboardComponent>(OwnerActor, ExpectedBillboardName, RF_Transient | RF_TextExportTransient);
		if (!PreviewBillboardComponent)
		{
			return;
		}
		OwnerActor->AddInstanceComponent(PreviewBillboardComponent);
		PreviewBillboardComponent->RegisterComponentWithWorld(World);
		EditorPreviewBillboardComponent = PreviewBillboardComponent;
	}

	PreviewBillboardComponent->CreationMethod = EComponentCreationMethod::Instance;
	PreviewBillboardComponent->bIsEditorOnly = true;
	PreviewBillboardComponent->SetHiddenInGame(true);
	PreviewBillboardComponent->SetMobility(EComponentMobility::Movable);
	PreviewBillboardComponent->ComponentTags.AddUnique(FName(TEXT("Emo.EditorPreviewBillboard")));
	if (!PreviewBillboardComponent->IsRegistered())
	{
		PreviewBillboardComponent->RegisterComponentWithWorld(World);
	}

	if (USceneComponent* RootComponent = OwnerActor->GetRootComponent())
	{
		if (PreviewBillboardComponent->GetAttachParent() != RootComponent)
		{
			PreviewBillboardComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
		}
	}

	PreviewBillboardComponent->SetSprite(LoadedTexture);
	PreviewBillboardComponent->SetVisibility(true, true);

	const float PreviewScale = FMath::Max(0.05f, IconScreenSize / 64.0f);
	PreviewBillboardComponent->SetRelativeScale3D(FVector(PreviewScale));

	FVector PreviewWorldLocation = FVector::ZeroVector;
	if (!GetEditorPreviewAnchorLocationExcludingBillboards(OwnerActor, PreviewBillboardComponent, PreviewWorldLocation))
	{
		PreviewWorldLocation = GetEmotionAnchorWorldLocation();
	}

	PreviewBillboardComponent->SetWorldLocation(PreviewWorldLocation);
#endif
}

void UEmoComponent::DestroyEditorPreviewBillboard()
{
#if WITH_EDITORONLY_DATA
	AActor* OwnerActor = GetOwner();
	TArray<UBillboardComponent*> BillboardComponents;
	if (OwnerActor)
	{
		OwnerActor->GetComponents(BillboardComponents);
	}

	for (UBillboardComponent* BillboardComponent : BillboardComponents)
	{
		if (BillboardComponent)
		{
			BillboardComponent->DestroyComponent();
		}
	}

	EditorPreviewBillboardComponent = nullptr;
#endif
}
#endif
