#include "ARHUDEmotionViewComponent.h"

#include "AREmotionComponent.h"
#include "AREmotionSettings.h"
#include "ARLog.h"
#include "ARPlayerStateBase.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Actor.h"
#include "GameFramework/HUD.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "UObject/UObjectIterator.h"

UARHUDEmotionViewComponent::UARHUDEmotionViewComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

int32 UARHUDEmotionViewComponent::RenderEmotionView(AHUD* HUD, UCanvas* InCanvas, const APlayerController* LocalController)
{
	if (!HUD || !InCanvas || !InCanvas->Canvas || !LocalController || !bEnableEmotionView || IsEmotionViewSuppressed())
	{
		return 0;
	}

	if (ResolveOwningHUD() != HUD)
	{
		return 0;
	}

	ActiveProjectionCanvas = InCanvas;

	const APawn* LocalPawn = LocalController->GetPawn();
	int32 DrawnEmotionCount = 0;

	for (TObjectIterator<UAREmotionComponent> It; It; ++It)
	{
		const UAREmotionComponent* EmotionComponent = *It;
		if (!IsValid(EmotionComponent) || EmotionComponent->IsTemplate() || EmotionComponent->GetWorld() != HUD->GetWorld())
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

		if (bHideOccludedEmotion && !IsEmotionVisibleForViewer(EmotionComponent, LocalController))
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

		// Match editor preview sizing semantics by projecting a camera-facing world-space sprite.
		const float TextureWidth = static_cast<float>(FMath::Max(1, IconTexture->GetSurfaceWidth()));
		const float TextureHeight = static_cast<float>(FMath::Max(1, IconTexture->GetSurfaceHeight()));
		const float TextureMax = FMath::Max(TextureWidth, TextureHeight);
		const float DesiredWorldMaxDimension = FMath::Max(1.0f, EmotionComponent->GetIconScreenSize() * EmotionIconRenderScale);
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

		const FVector2D DrawPosition(ScreenPosition.X - (DrawExtent.X * 0.5f), ScreenPosition.Y - (DrawExtent.Y * 0.5f));

		const FTexture* IconResource = IconTexture->GetResource();
		if (!IconResource)
		{
			continue;
		}

		FCanvasTileItem TileItem(DrawPosition, IconResource, DrawExtent, FLinearColor::White);
		TileItem.BlendMode = SE_BLEND_Translucent;
		InCanvas->DrawItem(TileItem);
		++DrawnEmotionCount;
	}

	ActiveProjectionCanvas.Reset();

	if (ShouldLogEmotionRenderVerbose())
	{
		static double LastVerboseSummarySeconds = 0.0;
		const double NowSeconds = FPlatformTime::Seconds();
		if ((NowSeconds - LastVerboseSummarySeconds) >= 1.0)
		{
			UE_LOG(
				ARLog,
				Verbose,
				TEXT("[Emotion][HUD] Native draw summary: Drawn=%d HUD='%s' Controller='%s'."),
				DrawnEmotionCount,
				*GetNameSafe(HUD),
				*GetNameSafe(LocalController));
			LastVerboseSummarySeconds = NowSeconds;
		}
	}

	return DrawnEmotionCount;
}

bool UARHUDEmotionViewComponent::IsEmotionVisibleForViewer(const UAREmotionComponent* EmotionComponent, const APlayerController* LocalController) const
{
	if (!bHideOccludedEmotion || !EmotionComponent || !LocalController)
	{
		return true;
	}

	UWorld* World = GetWorld();
	const APlayerCameraManager* CameraManager = LocalController->PlayerCameraManager;
	const AActor* EmotionOwner = EmotionComponent->GetOwner();
	if (!World || !CameraManager || !EmotionOwner)
	{
		return true;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ARHUDEmotionOcclusion), false);
	if (const APawn* LocalPawn = LocalController->GetPawn())
	{
		QueryParams.AddIgnoredActor(LocalPawn);
	}

	FHitResult Hit;
	const bool bHit = World->LineTraceSingleByChannel(
		Hit,
		CameraManager->GetCameraLocation(),
		EmotionComponent->GetEmotionAnchorWorldLocation(),
		OcclusionTraceChannel,
		QueryParams);
	if (!bHit)
	{
		return true;
	}

	const AActor* HitActor = Hit.GetActor();
	if (!HitActor)
	{
		return false;
	}

	return HitActor == EmotionOwner
		|| HitActor->IsAttachedTo(EmotionOwner)
		|| EmotionOwner->IsAttachedTo(HitActor);
}

bool UARHUDEmotionViewComponent::TryProjectEmotionForActor(
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

bool UARHUDEmotionViewComponent::TryProjectEmotionForComponent(
	const UAREmotionComponent* EmotionComponent,
	FVector2D& OutScreenPosition,
	FGameplayTag& OutDisplayedEmotionTag,
	TSoftObjectPtr<UTexture2D>& OutDisplayedIcon) const
{
	OutScreenPosition = FVector2D::ZeroVector;
	OutDisplayedEmotionTag = FGameplayTag();
	OutDisplayedIcon.Reset();

	const AHUD* OwnerHUD = ResolveOwningHUD();
	const APlayerController* LocalController = OwnerHUD ? OwnerHUD->GetOwningPlayerController() : nullptr;
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

void UARHUDEmotionViewComponent::SetEmotionViewEnabled(const bool bEnabled)
{
	bEnableEmotionView = bEnabled;
}

void UARHUDEmotionViewComponent::SetEmotionViewSuppressed(const bool bSuppressed, const FName Reason)
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

void UARHUDEmotionViewComponent::ClearEmotionViewSuppression(const FName Reason)
{
	if (Reason.IsNone())
	{
		SuppressionReasons.Reset();
		return;
	}

	SuppressionReasons.Remove(Reason);
}

void UARHUDEmotionViewComponent::ClearAllEmotionViewSuppression()
{
	SuppressionReasons.Reset();
}

AHUD* UARHUDEmotionViewComponent::ResolveOwningHUD() const
{
	return Cast<AHUD>(GetOwner());
}

bool UARHUDEmotionViewComponent::ShouldLogEmotionRenderVerbose()
{
	const UAREmotionSettings* Settings = GetDefault<UAREmotionSettings>();
	return Settings && Settings->bEnableVerboseRenderLogs;
}
