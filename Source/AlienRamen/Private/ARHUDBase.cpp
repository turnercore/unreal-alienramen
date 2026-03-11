#include "ARHUDBase.h"

#include "AREmotionComponent.h"
#include "AREmotionSettings.h"
#include "ARLog.h"
#include "ARPlayerController.h"
#include "ARPlayerStateBase.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "UObject/UObjectIterator.h"

AARHUDBase::AARHUDBase()
{
}

int32 AARHUDBase::RenderEmotionView()
{
	const APlayerController* LocalController = GetOwningPlayerController();
	if (!Canvas || !Canvas->Canvas || !LocalController || !bEnableEmotionView || IsEmotionRenderingSuppressed())
	{
		return 0;
	}

	ActiveProjectionCanvas = Canvas;
	ActiveProjectionController = LocalController;

	const APawn* LocalPawn = LocalController->GetPawn();
	int32 DrawnEmotionCount = 0;

	for (TObjectIterator<UAREmotionComponent> It; It; ++It)
	{
		const UAREmotionComponent* EmotionComponent = *It;
		if (!IsValid(EmotionComponent) || EmotionComponent->IsTemplate() || EmotionComponent->GetWorld() != GetWorld())
		{
			continue;
		}

		AActor* OwnerActor = EmotionComponent->GetOwner();
		if (!IsValid(OwnerActor) || OwnerActor->IsHidden())
		{
			continue;
		}

		if (bHideOwningPawnEmotion && OwnerActor == LocalPawn)
		{
			continue;
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

		const FVector AnchorWorldLocation = EmotionComponent->GetEmotionAnchorWorldLocation();
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

	if (ShouldLogEmotionRenderVerbose())
	{
		static double LastVerboseSummarySeconds = 0.0;
		const double NowSeconds = FPlatformTime::Seconds();
		if ((NowSeconds - LastVerboseSummarySeconds) >= 1.0)
		{
			UE_LOG(
				ARLog,
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

bool AARHUDBase::IsEmotionVisibleForViewer(const UAREmotionComponent* EmotionComponent, const APlayerController* LocalController) const
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

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ARHUDEmotionOcclusion), false);
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

	if (OcclusionTraceChannel != ECollisionChannel::ECC_Visibility
		&& IsBlockedOnChannel(ECollisionChannel::ECC_Visibility))
	{
		return false;
	}

	if (OcclusionTraceChannel != ECollisionChannel::ECC_Camera
		&& IsBlockedOnChannel(ECollisionChannel::ECC_Camera))
	{
		return false;
	}

	return true;
}

void AARHUDBase::RequestHUDInitialization(AARPlayerController* SourceController, APlayerState* CurrentPlayerState, AGameStateBase* CurrentGameState)
{
	if (!SourceController || !SourceController->IsLocalController())
	{
		return;
	}

	BP_OnHUDInitializationRequested(SourceController, CurrentPlayerState, CurrentGameState);
}

void AARHUDBase::DrawHUD()
{
	Super::DrawHUD();

	const int32 DrawnEmotionCount = RenderEmotionView();
	static double LastHUDRenderLogSeconds = 0.0;
	const double NowSeconds = FPlatformTime::Seconds();
	if ((NowSeconds - LastHUDRenderLogSeconds) >= 1.0)
	{
		UE_LOG(
			ARLog,
			VeryVerbose,
			TEXT("[Emotion][HUD] AARHUDBase::DrawHUD emitted DrawnEmotionCount=%d HUD='%s'."),
			DrawnEmotionCount,
			*GetNameSafe(this));
		LastHUDRenderLogSeconds = NowSeconds;
	}
}

bool AARHUDBase::TryProjectEmotionForActor(
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

	TArray<UAREmotionComponent*> EmotionComponents;
	TargetActor->GetComponents<UAREmotionComponent>(EmotionComponents);
	for (const UAREmotionComponent* EmotionComponent : EmotionComponents)
	{
		if (TryProjectEmotionForComponent(EmotionComponent, OutScreenPosition, OutDisplayedEmotionTag, OutDisplayedIcon))
		{
			return true;
		}
	}

	return false;
}

bool AARHUDBase::TryProjectEmotionForComponent(
	const UAREmotionComponent* EmotionComponent,
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
				ARLog,
				Verbose,
				TEXT("[Emotion][HUD] Skip projection: LocalController=%s IsLocal=%d EmotionComponent=%s"),
				*GetNameSafe(LocalController),
				(LocalController && LocalController->IsLocalController()) ? 1 : 0,
				*GetNameSafe(EmotionComponent));
		}
		return false;
	}

	if (bHideOccludedEmotion && !IsEmotionVisibleForViewer(EmotionComponent, LocalController))
	{
		return false;
	}

	const AARPlayerStateBase* LocalPlayerState = LocalController->GetPlayerState<AARPlayerStateBase>();
	const EARPlayerSlot ViewerSlot = LocalPlayerState ? LocalPlayerState->GetPlayerSlot() : EARPlayerSlot::Unknown;
	if (ViewerSlot == EARPlayerSlot::Unknown)
	{
		if (ShouldLogEmotionRenderVerbose())
		{
			UE_LOG(
				ARLog,
				Verbose,
				TEXT("[Emotion][HUD] Skip projection for '%s': Viewer slot unknown."),
				*GetNameSafe(EmotionComponent->GetOwner()));
		}
		return false;
	}

	const FGameplayTag DisplayTag = EmotionComponent->GetDisplayedEmotionTagForPlayerSlot(ViewerSlot);
	if (!DisplayTag.IsValid())
	{
		if (ShouldLogEmotionRenderVerbose())
		{
			UE_LOG(
				ARLog,
				Verbose,
				TEXT("[Emotion][HUD] Skip projection for '%s': no displayed emotion tag for slot %d."),
				*GetNameSafe(EmotionComponent->GetOwner()),
				static_cast<int32>(ViewerSlot));
		}
		return false;
	}

	if (!EmotionComponent->TryResolveEmotionIconForTag(DisplayTag, OutDisplayedIcon, OutDisplayedEmotionTag))
	{
		if (ShouldLogEmotionRenderVerbose())
		{
			UE_LOG(
				ARLog,
				Verbose,
				TEXT("[Emotion][HUD] Resolve failed for '%s': DisplayTag=%s Slot=%d"),
				*GetNameSafe(EmotionComponent->GetOwner()),
				*DisplayTag.ToString(),
				static_cast<int32>(ViewerSlot));
		}
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
			ARLog,
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

void AARHUDBase::SetEmotionRenderingSuppressed(const bool bSuppressed, const FName Reason)
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

bool AARHUDBase::ShouldLogEmotionRenderVerbose()
{
	const UAREmotionSettings* Settings = GetDefault<UAREmotionSettings>();
	return Settings && Settings->bEnableVerboseRenderLogs;
}
