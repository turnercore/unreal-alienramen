#include "ARHUDBase.h"

#include "ARHUDEmotionViewComponent.h"
#include "ARLog.h"
#include "ARPlayerController.h"
#include "HAL/PlatformTime.h"

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

	const int32 DrawnEmotionCount = EmotionViewComponent->RenderEmotionView(this, Canvas, GetOwningPlayerController());
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
