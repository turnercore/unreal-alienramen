#include "ARShopGameMode.h"

AARShopGameMode::AARShopGameMode()
{
	ModeTag = FGameplayTag::RequestGameplayTag(TEXT("Mode.Shop"), false);
	if (!ModeTag.IsValid())
	{
		// Legacy fallback while content tags migrate to Mode.Shop.
		ModeTag = FGameplayTag::RequestGameplayTag(TEXT("Mode.RamenShop"), false);
	}
}
