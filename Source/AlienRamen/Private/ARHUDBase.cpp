#include "ARHUDBase.h"

#include "ARHUDEmotionViewComponent.h"
#include "ARPlayerController.h"

AARHUDBase::AARHUDBase()
{
	EmotionViewComponent = CreateDefaultSubobject<UARHUDEmotionViewComponent>(TEXT("EmotionView"));
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

	if (!EmotionViewComponent)
	{
		return;
	}

	EmotionViewComponent->RenderEmotionView();
}

bool AARHUDBase::TryProjectEmotionForActor(
	AActor* TargetActor,
	FVector2D& OutScreenPosition,
	FGameplayTag& OutDisplayedEmotionTag,
	TSoftObjectPtr<UTexture2D>& OutDisplayedIcon) const
{
	if (!EmotionViewComponent)
	{
		return false;
	}

	return EmotionViewComponent->TryProjectEmotionForActor(TargetActor, OutScreenPosition, OutDisplayedEmotionTag, OutDisplayedIcon);
}

bool AARHUDBase::TryProjectEmotionForComponent(
	const UAREmotionComponent* EmotionComponent,
	FVector2D& OutScreenPosition,
	FGameplayTag& OutDisplayedEmotionTag,
	TSoftObjectPtr<UTexture2D>& OutDisplayedIcon) const
{
	if (!EmotionViewComponent)
	{
		return false;
	}

	return EmotionViewComponent->TryProjectEmotionForComponent(EmotionComponent, OutScreenPosition, OutDisplayedEmotionTag, OutDisplayedIcon);
}

void AARHUDBase::SetEmotionRenderingSuppressed(const bool bSuppressed, const FName Reason)
{
	if (!EmotionViewComponent)
	{
		return;
	}

	EmotionViewComponent->SetEmotionViewSuppressed(bSuppressed, Reason);
}
