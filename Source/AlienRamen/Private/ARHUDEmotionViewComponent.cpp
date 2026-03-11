#include "ARHUDEmotionViewComponent.h"

#include "AREmotionComponent.h"
#include "AREmotionSettings.h"
#include "ARLog.h"
#include "ARPlayerStateBase.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Actor.h"
#include "GameFramework/HUD.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "UObject/UObjectIterator.h"

UARHUDEmotionViewComponent::UARHUDEmotionViewComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

int32 UARHUDEmotionViewComponent::RenderEmotionView()
{
	AHUD* OwnerHUD = ResolveOwningHUD();
	if (!OwnerHUD || !bEnableEmotionView || IsEmotionViewSuppressed() || !OwnerHUD->Canvas)
	{
		return 0;
	}

	const APlayerController* LocalController = OwnerHUD->PlayerOwner;
	if (!LocalController || !LocalController->IsLocalController())
	{
		return 0;
	}

	const APawn* LocalPawn = LocalController->GetPawn();
	int32 DrawnEmotionCount = 0;

	for (TObjectIterator<UAREmotionComponent> It; It; ++It)
	{
		const UAREmotionComponent* EmotionComponent = *It;
		if (!EmotionComponent || EmotionComponent->IsTemplate() || EmotionComponent->GetWorld() != OwnerHUD->GetWorld())
		{
			continue;
		}

		AActor* OwnerActor = EmotionComponent->GetOwner();
		if (!OwnerActor || OwnerActor->IsHidden())
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

		// Match editor preview sizing semantics:
		// billboard preview uses relative scale = IconScreenSize / 64 over source sprite dimensions.
		const float IconScale = FMath::Max(0.01f, (EmotionComponent->GetIconScreenSize() / 64.0f) * EmotionIconRenderScale);
		const float BaseWidth = static_cast<float>(FMath::Max(1, IconTexture->GetSurfaceWidth()));
		const float BaseHeight = static_cast<float>(FMath::Max(1, IconTexture->GetSurfaceHeight()));
		const FVector2D DrawExtent(BaseWidth * IconScale, BaseHeight * IconScale);
		const FVector2D DrawPosition(ScreenPosition.X - (DrawExtent.X * 0.5f), ScreenPosition.Y - (DrawExtent.Y * 0.5f));

		FCanvasTileItem TileItem(DrawPosition, IconTexture->GetResource(), DrawExtent, FLinearColor::White);
		TileItem.BlendMode = SE_BLEND_Translucent;
		OwnerHUD->Canvas->DrawItem(TileItem);
		++DrawnEmotionCount;
	}

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
				*GetNameSafe(OwnerHUD),
				*GetNameSafe(LocalController));
			LastVerboseSummarySeconds = NowSeconds;
		}
	}

	return DrawnEmotionCount;
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
	const APlayerController* LocalController = OwnerHUD ? OwnerHUD->PlayerOwner : nullptr;
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

	const bool bProjected = LocalController->ProjectWorldLocationToScreen(
		EmotionComponent->GetEmotionAnchorWorldLocation(),
		OutScreenPosition,
		true);
	if (ShouldLogEmotionRenderVerbose())
	{
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[Emotion][HUD] Projection %s for '%s': DisplayTag=%s ResolvedTag=%s Icon=%s Screen=(%.1f,%.1f)"),
			bProjected ? TEXT("success") : TEXT("failed"),
			*GetNameSafe(EmotionComponent->GetOwner()),
			*DisplayTag.ToString(),
			*OutDisplayedEmotionTag.ToString(),
			OutDisplayedIcon.IsNull() ? TEXT("<none>") : *OutDisplayedIcon.ToSoftObjectPath().ToString(),
			OutScreenPosition.X,
			OutScreenPosition.Y);
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
