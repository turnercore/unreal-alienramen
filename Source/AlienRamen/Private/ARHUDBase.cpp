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
	const UAREmotionComponent* EmotionComponent = TargetActor ? TargetActor->FindComponentByClass<UAREmotionComponent>() : nullptr;
	return TryProjectEmotionForComponent(EmotionComponent, OutScreenPosition, OutDisplayedEmotionTag, OutDisplayedIcon);
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

	const AARPlayerController* LocalController = Cast<AARPlayerController>(PlayerOwner);
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

