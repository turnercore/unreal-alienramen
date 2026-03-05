#include "ARScrapyardGameMode.h"

AARScrapyardGameMode::AARScrapyardGameMode()
{
	ModeTag = FGameplayTag::RequestGameplayTag(TEXT("Mode.Scrapyard"), false);
}
