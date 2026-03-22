#include "EmoHUDBase.h"

#include "EmoComponent.h"
#include "EmoComponentRegistrySubsystem.h"
#include "EmoSettings.h"
#include "EmoLog.h"
#include "CanvasItem.h"
#include "Engine/AssetManager.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Stats/Stats.h"

DECLARE_STATS_GROUP(TEXT("AR Emotion HUD"), STATGROUP_EmoHUD, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("Emotion HUD Render"), STAT_EmoHUD_Render, STATGROUP_EmoHUD);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Emotion HUD Candidates"), STAT_EmoHUD_Candidates, STATGROUP_EmoHUD);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Emotion HUD Drawn"), STAT_EmoHUD_Drawn, STATGROUP_EmoHUD);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Emotion HUD Occlusion Traces"), STAT_EmoHUD_OcclusionTraces, STATGROUP_EmoHUD);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Emotion HUD Async Requests"), STAT_EmoHUD_AsyncRequests, STATGROUP_EmoHUD);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Emotion HUD Distance Culled"), STAT_EmoHUD_DistanceCulled, STATGROUP_EmoHUD);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Emotion HUD FOV Culled"), STAT_EmoHUD_FOVCulled, STATGROUP_EmoHUD);

AEmoHUDBase::AEmoHUDBase()
{
}

void AEmoHUDBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CleanupAsyncEmotionLoads();
	CachedEmotionComponents.Reset();
	PendingAsyncIconLoads.Reset();
	ActiveProjectionCanvas.Reset();
	ActiveProjectionController.Reset();
	ActiveProjectionViewedEmotionTags.Reset();
	Super::EndPlay(EndPlayReason);
}

void AEmoHUDBase::RefreshEmotionComponentCacheIfNeeded()
{
	const double NowSeconds = FPlatformTime::Seconds();
	const bool bNeedsRefresh = EmotionComponentCacheRefreshSeconds <= 0.0f
		|| LastEmotionComponentCacheRefreshTimeSeconds < 0.0
		|| (NowSeconds - LastEmotionComponentCacheRefreshTimeSeconds) >= static_cast<double>(EmotionComponentCacheRefreshSeconds);

	if (!bNeedsRefresh)
	{
		return;
	}

	LastEmotionComponentCacheRefreshTimeSeconds = NowSeconds;
	CachedEmotionComponents.Reset();

	UWorld* World = GetWorld();
	if (UEmoComponentRegistrySubsystem* Registry = World ? World->GetSubsystem<UEmoComponentRegistrySubsystem>() : nullptr)
	{
		Registry->GetRegisteredEmotionComponents(CachedEmotionComponents);
	}
}

void AEmoHUDBase::QueueAsyncIconLoad(const TSoftObjectPtr<UTexture2D>& IconPtr)
{
	UWorld* World = GetWorld();
	if (!World || World->bIsTearingDown || HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed))
	{
		return;
	}

	if (IconPtr.IsNull())
	{
		return;
	}

	const FSoftObjectPath IconPath = IconPtr.ToSoftObjectPath();
	if (!IconPath.IsValid() || PendingAsyncIconLoads.Contains(IconPath))
	{
		return;
	}

	PendingAsyncIconLoads.Add(IconPath);
	INC_DWORD_STAT(STAT_EmoHUD_AsyncRequests);
	TWeakObjectPtr<AEmoHUDBase> WeakThis(this);
	TSharedPtr<FStreamableHandle> Handle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		IconPath,
		FStreamableDelegate::CreateLambda([WeakThis, IconPath]()
			{
				if (AEmoHUDBase* HUD = WeakThis.Get())
				{
					HUD->PendingAsyncIconLoads.Remove(IconPath);
				}
			}));

	if (Handle.IsValid())
	{
		ActiveAsyncIconHandles.Add(Handle);
	}
	else
	{
		PendingAsyncIconLoads.Remove(IconPath);
	}
}

void AEmoHUDBase::CleanupAsyncEmotionLoads()
{
	for (const TSharedPtr<FStreamableHandle>& Handle : ActiveAsyncIconHandles)
	{
		if (Handle.IsValid())
		{
			Handle->CancelHandle();
		}
	}

	ActiveAsyncIconHandles.Reset();
}

int32 AEmoHUDBase::RenderEmotionView()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(EmoHUD_EmotionRender);
	SCOPE_CYCLE_COUNTER(STAT_EmoHUD_Render);

	const APlayerController* LocalController = GetOwningPlayerController();
	if (!Canvas || !Canvas->Canvas || !LocalController || !bEnableEmotionView || IsEmotionRenderingSuppressed())
	{
		return 0;
	}

	ActiveProjectionCanvas = Canvas;
	ActiveProjectionController = LocalController;
	ActiveProjectionViewedEmotionTags = ViewedEmotionTags;
	ActiveAsyncIconHandles.RemoveAll([](const TSharedPtr<FStreamableHandle>& Handle)
		{
			return !Handle.IsValid() || Handle->HasLoadCompleted();
		});

	RefreshEmotionComponentCacheIfNeeded();

	FVector ViewLocation = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;
	LocalController->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FVector ViewForward = ViewRotation.Vector();
	const float MaxDistanceSq = (bEnableDistanceCull && MaxEmotionRenderDistance > 0.0f) ? FMath::Square(MaxEmotionRenderDistance) : 0.0f;
	const float MinViewDot = (bEnableFOVCull && MaxEmotionViewAngleDegrees < 180.0f)
		? FMath::Cos(FMath::DegreesToRadians(FMath::Max(0.0f, MaxEmotionViewAngleDegrees)))
		: -1.0f;

	uint32 CandidateCount = 0;
	uint32 DistanceCulledCount = 0;
	uint32 FOVCulledCount = 0;
	const APawn* LocalPawn = LocalController->GetPawn();
	int32 DrawnEmotionCount = 0;
	OcclusionTraceCountThisFrame = 0;

	for (int32 Index = CachedEmotionComponents.Num() - 1; Index >= 0; --Index)
	{
		UEmoComponent* EmotionComponent = CachedEmotionComponents[Index].Get();
		if (!IsValid(EmotionComponent) || EmotionComponent->IsTemplate() || EmotionComponent->GetWorld() != GetWorld())
		{
			CachedEmotionComponents.RemoveAtSwap(Index);
			continue;
		}

		++CandidateCount;
		AActor* OwnerActor = EmotionComponent->GetOwner();
		if (!IsValid(OwnerActor) || OwnerActor->IsHidden())
		{
			continue;
		}

		if (bHideOwningPawnEmotion && OwnerActor == LocalPawn)
		{
			continue;
		}

		const FVector AnchorWorldLocation = EmotionComponent->GetEmotionAnchorWorldLocation();
		if (MaxDistanceSq > 0.0f)
		{
			const float DistanceSq = FVector::DistSquared(ViewLocation, AnchorWorldLocation);
			if (DistanceSq > MaxDistanceSq)
			{
				++DistanceCulledCount;
				continue;
			}
		}

		if (MinViewDot > -1.0f)
		{
			const FVector ToAnchor = AnchorWorldLocation - ViewLocation;
			if (!ToAnchor.IsNearlyZero())
			{
				const float ViewDot = FVector::DotProduct(ViewForward, ToAnchor.GetSafeNormal());
				if (ViewDot < MinViewDot)
				{
					++FOVCulledCount;
					continue;
				}
			}
		}

		FVector2D ScreenPosition = FVector2D::ZeroVector;
		FGameplayTag DisplayedEmotionTag;
		TSoftObjectPtr<UTexture2D> DisplayedIcon;
		if (!TryProjectEmotionForComponent(EmotionComponent, ScreenPosition, DisplayedEmotionTag, DisplayedIcon))
		{
			continue;
		}

		UTexture2D* IconTexture = DisplayedIcon.Get();
		if (!IconTexture)
		{
			if (bAsyncLoadEmotionIcons)
			{
				QueueAsyncIconLoad(DisplayedIcon);
				continue;
			}

			IconTexture = DisplayedIcon.LoadSynchronous();
		}

		if (!IconTexture || !IconTexture->GetResource())
		{
			continue;
		}

		const float TextureWidth = static_cast<float>(FMath::Max(1, IconTexture->GetSurfaceWidth()));
		const float TextureHeight = static_cast<float>(FMath::Max(1, IconTexture->GetSurfaceHeight()));
		const float TextureMax = FMath::Max(TextureWidth, TextureHeight);
		constexpr float RuntimeSizeCalibration = 3.5f;
		const float EffectiveRenderScale = FMath::Max(0.01f, EmotionIconRenderScale * RuntimeSizeCalibration);
		const float DesiredWorldMaxDimension = FMath::Max(1.0f, EmotionComponent->GetIconScreenSize() * EffectiveRenderScale);
		const float BaseWidth = DesiredWorldMaxDimension * (TextureWidth / TextureMax);
		const float BaseHeight = DesiredWorldMaxDimension * (TextureHeight / TextureMax);

		const FVector CameraRight = LocalController->PlayerCameraManager ? LocalController->PlayerCameraManager->GetActorRightVector() : FVector::RightVector;
		const FVector CameraUp = LocalController->PlayerCameraManager ? LocalController->PlayerCameraManager->GetActorUpVector() : FVector::UpVector;

		auto ProjectPointToScreen = [this, LocalController](const FVector& WorldPoint, FVector2D& OutPoint) -> bool
		{
			if (const UCanvas* ProjectionCanvas = ActiveProjectionCanvas.Get())
			{
				const FVector Projected = ProjectionCanvas->Project(WorldPoint, true);
				if (Projected.Z <= 0.0f)
				{
					return false;
				}

				OutPoint = FVector2D(Projected.X, Projected.Y);
				return true;
			}

			return LocalController->ProjectWorldLocationToScreen(WorldPoint, OutPoint, true);
		};

		FVector2D LeftPoint = FVector2D::ZeroVector;
		FVector2D RightPoint = FVector2D::ZeroVector;
		FVector2D UpPoint = FVector2D::ZeroVector;
		FVector2D DownPoint = FVector2D::ZeroVector;
		FVector2D DrawExtent = FVector2D(BaseWidth, BaseHeight);
		const bool bProjectedSprite =
			ProjectPointToScreen(AnchorWorldLocation - (CameraRight * (BaseWidth * 0.5f)), LeftPoint)
			&& ProjectPointToScreen(AnchorWorldLocation + (CameraRight * (BaseWidth * 0.5f)), RightPoint)
			&& ProjectPointToScreen(AnchorWorldLocation + (CameraUp * (BaseHeight * 0.5f)), UpPoint)
			&& ProjectPointToScreen(AnchorWorldLocation - (CameraUp * (BaseHeight * 0.5f)), DownPoint);
		if (bProjectedSprite)
		{
			DrawExtent.X = FMath::Max(1.0f, FMath::Abs(RightPoint.X - LeftPoint.X));
			DrawExtent.Y = FMath::Max(1.0f, FMath::Abs(DownPoint.Y - UpPoint.Y));
		}

		const float MinimumScreenDimension = FMath::Max(0.0f, MinimumIconScreenSizePixels);
		if (MinimumScreenDimension > 0.0f)
		{
			const float CurrentMaxDimension = FMath::Max(DrawExtent.X, DrawExtent.Y);
			if (CurrentMaxDimension > 0.0f && CurrentMaxDimension < MinimumScreenDimension)
			{
				DrawExtent *= (MinimumScreenDimension / CurrentMaxDimension);
			}
		}

		const FVector2D DrawPosition(ScreenPosition.X - (DrawExtent.X * 0.5f), ScreenPosition.Y - (DrawExtent.Y * 0.5f));
		const FTexture* IconResource = IconTexture->GetResource();
		if (!IconResource)
		{
			continue;
		}

		FCanvasTileItem TileItem(DrawPosition, IconResource, DrawExtent, FLinearColor::White);
		TileItem.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(TileItem);
		++DrawnEmotionCount;
	}

	ActiveProjectionCanvas.Reset();
	ActiveProjectionController.Reset();
	ActiveProjectionViewedEmotionTags.Reset();
	SET_DWORD_STAT(STAT_EmoHUD_Candidates, CandidateCount);
	SET_DWORD_STAT(STAT_EmoHUD_Drawn, static_cast<uint32>(DrawnEmotionCount));
	SET_DWORD_STAT(STAT_EmoHUD_OcclusionTraces, OcclusionTraceCountThisFrame);
	SET_DWORD_STAT(STAT_EmoHUD_DistanceCulled, DistanceCulledCount);
	SET_DWORD_STAT(STAT_EmoHUD_FOVCulled, FOVCulledCount);

	if (ShouldLogEmotionRenderVerbose())
	{
		static double LastVerboseSummarySeconds = 0.0;
		const double NowSeconds = FPlatformTime::Seconds();
		if ((NowSeconds - LastVerboseSummarySeconds) >= 1.0)
		{
			UE_LOG(
				EmoLog,
				Verbose,
				TEXT("[Emotion][HUD] Native draw summary: Drawn=%d HUD='%s' Controller='%s' HideOccluded=%d TraceChannel=%d."),
				DrawnEmotionCount,
				*GetNameSafe(this),
				*GetNameSafe(LocalController),
				bHideOccludedEmotion ? 1 : 0,
				static_cast<int32>(OcclusionTraceChannel));
			LastVerboseSummarySeconds = NowSeconds;
		}
	}

	return DrawnEmotionCount;
}

bool AEmoHUDBase::IsEmotionVisibleForViewer(const UEmoComponent* EmotionComponent, const APlayerController* LocalController) const
{
	if (!bHideOccludedEmotion || !EmotionComponent || !LocalController)
	{
		return true;
	}

	UWorld* World = GetWorld();
	const AActor* EmotionOwner = EmotionComponent->GetOwner();
	if (!World || !EmotionOwner)
	{
		return true;
	}

	FVector ViewLocation = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;
	LocalController->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FVector AnchorTarget = EmotionComponent->GetEmotionAnchorWorldLocation();

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EmoHUDEmotionOcclusion), false);
	QueryParams.bTraceComplex = true;
	if (const APawn* LocalPawn = LocalController->GetPawn())
	{
		QueryParams.AddIgnoredActor(LocalPawn);
	}

	auto IsHitOnEmotionOwner = [&](const FHitResult& Hit) -> bool
	{
		const AActor* HitActor = Hit.GetActor();
		if (!HitActor)
		{
			return false;
		}

		return HitActor == EmotionOwner
			|| HitActor->IsAttachedTo(EmotionOwner)
			|| EmotionOwner->IsAttachedTo(HitActor);
	};

	auto IsBlockedOnChannel = [&](const ECollisionChannel Channel) -> bool
	{
		FHitResult Hit;
		++OcclusionTraceCountThisFrame;
		const bool bHit = World->LineTraceSingleByChannel(Hit, ViewLocation, AnchorTarget, Channel, QueryParams);
		return bHit && !IsHitOnEmotionOwner(Hit);
	};

	auto IsBlockedByWorldObject = [&]() -> bool
	{
		FCollisionObjectQueryParams ObjectQueryParams;
		ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
		ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
		ObjectQueryParams.AddObjectTypesToQuery(ECC_PhysicsBody);

		FHitResult Hit;
		++OcclusionTraceCountThisFrame;
		const bool bHit = World->LineTraceSingleByObjectType(Hit, ViewLocation, AnchorTarget, ObjectQueryParams, QueryParams);
		return bHit && !IsHitOnEmotionOwner(Hit);
	};

	if (IsBlockedByWorldObject())
	{
		return false;
	}

	if (IsBlockedOnChannel(OcclusionTraceChannel))
	{
		return false;
	}

	if (bUseOcclusionFallbackChannels
		&& OcclusionTraceChannel != ECollisionChannel::ECC_Visibility
		&& IsBlockedOnChannel(ECollisionChannel::ECC_Visibility))
	{
		return false;
	}

	if (bUseOcclusionFallbackChannels
		&& OcclusionTraceChannel != ECollisionChannel::ECC_Camera
		&& IsBlockedOnChannel(ECollisionChannel::ECC_Camera))
	{
		return false;
	}

	return true;
}

void AEmoHUDBase::RequestHUDInitialization(APlayerController* SourceController, APlayerState* CurrentPlayerState, AGameStateBase* CurrentGameState)
{
	if (!SourceController || !SourceController->IsLocalController())
	{
		return;
	}

	BP_OnHUDInitializationRequested(SourceController, CurrentPlayerState, CurrentGameState);
}

void AEmoHUDBase::SetViewedEmotionTags(FGameplayTagContainer NewViewedEmotionTags)
{
	ViewedEmotionTags = MoveTemp(NewViewedEmotionTags);
}

void AEmoHUDBase::DrawHUD()
{
	Super::DrawHUD();

	RenderEmotionView();
}

bool AEmoHUDBase::TryProjectEmotionForActor(
	AActor* TargetActor,
	FVector2D& OutScreenPosition,
	FGameplayTag& OutDisplayedEmotionTag,
	TSoftObjectPtr<UTexture2D>& OutDisplayedIcon) const
{
	OutScreenPosition = FVector2D::ZeroVector;
	OutDisplayedEmotionTag = FGameplayTag();
	OutDisplayedIcon.Reset();

	if (!TargetActor)
	{
		return false;
	}

	TArray<UEmoComponent*> EmotionComponents;
	TargetActor->GetComponents<UEmoComponent>(EmotionComponents);
	for (const UEmoComponent* EmotionComponent : EmotionComponents)
	{
		if (TryProjectEmotionForComponent(EmotionComponent, OutScreenPosition, OutDisplayedEmotionTag, OutDisplayedIcon))
		{
			return true;
		}
	}

	return false;
}

bool AEmoHUDBase::TryProjectEmotionForComponent(
	const UEmoComponent* EmotionComponent,
	FVector2D& OutScreenPosition,
	FGameplayTag& OutDisplayedEmotionTag,
	TSoftObjectPtr<UTexture2D>& OutDisplayedIcon) const
{
	OutScreenPosition = FVector2D::ZeroVector;
	OutDisplayedEmotionTag = FGameplayTag();
	OutDisplayedIcon.Reset();

	const APlayerController* LocalController = ActiveProjectionController.Get();
	if (!LocalController)
	{
		LocalController = GetOwningPlayerController();
	}

	if (!LocalController || !LocalController->IsLocalController() || !EmotionComponent)
	{
		if (ShouldLogEmotionRenderVerbose())
		{
			UE_LOG(
				EmoLog,
				Verbose,
				TEXT("[Emotion][HUD] Skip projection: LocalController=%s IsLocal=%d EmotionComponent=%s"),
				*GetNameSafe(LocalController),
				(LocalController && LocalController->IsLocalController()) ? 1 : 0,
				*GetNameSafe(EmotionComponent));
		}
		return false;
	}

	const FGameplayTagContainer& ViewerTags = ActiveProjectionViewedEmotionTags.IsEmpty()
		? ViewedEmotionTags
		: ActiveProjectionViewedEmotionTags;
	const FGameplayTag DisplayTag = EmotionComponent->GetDisplayedEmotionTagForViewerTags(ViewerTags);
	if (!DisplayTag.IsValid())
	{
		if (ShouldLogEmotionRenderVerbose())
		{
			UE_LOG(
				EmoLog,
				Verbose,
				TEXT("[Emotion][HUD] Skip projection for '%s': no displayed emotion tag for local viewer."),
				*GetNameSafe(EmotionComponent->GetOwner()));
		}
		return false;
	}

	if (!EmotionComponent->TryResolveEmotionIconForTag(DisplayTag, OutDisplayedIcon, OutDisplayedEmotionTag))
	{
		if (ShouldLogEmotionRenderVerbose())
		{
			UE_LOG(
				EmoLog,
				Verbose,
				TEXT("[Emotion][HUD] Resolve failed for '%s': DisplayTag=%s"),
				*GetNameSafe(EmotionComponent->GetOwner()),
				*DisplayTag.ToString());
		}
		return false;
	}

	if (bHideOccludedEmotion && !IsEmotionVisibleForViewer(EmotionComponent, LocalController))
	{
		return false;
	}

	const FVector AnchorWorldLocation = EmotionComponent->GetEmotionAnchorWorldLocation();
	bool bProjected = false;

	const UCanvas* ProjectionCanvas = ActiveProjectionCanvas.Get();
	if (ProjectionCanvas)
	{
		const FVector Projected = ProjectionCanvas->Project(AnchorWorldLocation, true);
		bProjected = Projected.Z > 0.0f;
		if (bProjected)
		{
			OutScreenPosition = FVector2D(Projected.X, Projected.Y);
		}
	}

	if (!bProjected)
	{
		bProjected = LocalController->ProjectWorldLocationToScreen(AnchorWorldLocation, OutScreenPosition, true);
	}

	if (ShouldLogEmotionRenderVerbose())
	{
		UE_LOG(
			EmoLog,
			Verbose,
			TEXT("[Emotion][HUD] Projection %s for '%s': DisplayTag=%s ResolvedTag=%s Icon=%s Screen=(%.1f,%.1f) Mode=%s"),
			bProjected ? TEXT("success") : TEXT("failed"),
			*GetNameSafe(EmotionComponent->GetOwner()),
			*DisplayTag.ToString(),
			*OutDisplayedEmotionTag.ToString(),
			OutDisplayedIcon.IsNull() ? TEXT("<none>") : *OutDisplayedIcon.ToSoftObjectPath().ToString(),
			OutScreenPosition.X,
			OutScreenPosition.Y,
			ProjectionCanvas ? TEXT("CanvasProject") : TEXT("ControllerProject"));
	}

	return bProjected;
}

void AEmoHUDBase::SetEmotionRenderingSuppressed(const bool bSuppressed, const FName Reason)
{
	if (bSuppressed)
	{
		SuppressionReasons.Add(Reason);
		return;
	}

	if (Reason.IsNone())
	{
		SuppressionReasons.Reset();
		return;
	}

	SuppressionReasons.Remove(Reason);
}

bool AEmoHUDBase::ShouldLogEmotionRenderVerbose()
{
	const UEmoSettings* Settings = GetDefault<UEmoSettings>();
	return Settings && Settings->bEnableVerboseRenderLogs;
}
