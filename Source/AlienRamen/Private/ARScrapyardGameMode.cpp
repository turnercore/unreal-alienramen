#include "ARScrapyardGameMode.h"

AARScrapyardGameMode::AARScrapyardGameMode()
{
	ModeTag = FGameplayTag::RequestGameplayTag(TEXT("Mode.Scrapyard"), false);
	ensureMsgf(ModeTag.IsValid(), TEXT("[ScrapyardGameMode] Required gameplay tag 'Mode.Scrapyard' is missing."));
}
