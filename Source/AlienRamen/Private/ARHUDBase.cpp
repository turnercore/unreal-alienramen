#include "ARHUDBase.h"

#include "AREmotionComponent.h"
#include "AREmotionSettings.h"
#include "ARLog.h"
#include "ARPlayerController.h"
#include "ARPlayerStateBase.h"
#include "GameFramework/Actor.h"

namespace
{
	static bool ShouldLogEmotionRenderVerbose()
	{
		const UAREmotionSettings* Settings = GetDefault<UAREmotionSettings>();
		return Settings && Settings->bEnableVerboseRenderLogs;
	}
}

void AARHUDBase::RequestHUDInitialization(AARPlayerController* SourceController, APlayerState* CurrentPlayerState, AGameStateBase* CurrentGameState)
{
	if (!SourceController || !SourceController->IsLocalController())
	{
		return;
	}

	BP_OnHUDInitializationRequested(SourceController, CurrentPlayerState, CurrentGameState);
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

	const APlayerController* LocalController = PlayerOwner;
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

