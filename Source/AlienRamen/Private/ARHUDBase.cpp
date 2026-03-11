#include "ARHUDBase.h"

#include "AREmotionComponent.h"
#include "ARPlayerController.h"
#include "ARPlayerStateBase.h"
#include "GameFramework/Actor.h"

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
		return false;
	}

	const AARPlayerStateBase* LocalPlayerState = LocalController->GetPlayerState<AARPlayerStateBase>();
	const EARPlayerSlot ViewerSlot = LocalPlayerState ? LocalPlayerState->GetPlayerSlot() : EARPlayerSlot::Unknown;
	if (!EmotionComponent->TryResolveDisplayedEmotionIconForPlayerSlot(ViewerSlot, OutDisplayedIcon, OutDisplayedEmotionTag))
	{
		return false;
	}

	return LocalController->ProjectWorldLocationToScreen(EmotionComponent->GetEmotionAnchorWorldLocation(), OutScreenPosition, true);
}

