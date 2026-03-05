#include "ARShopGameMode.h"

AARShopGameMode::AARShopGameMode()
{
	ModeTag = FGameplayTag::RequestGameplayTag(TEXT("Mode.Shop"), false);
}
