#include "ARScrapyardCarryItemBase.h"

AARScrapyardCarryItemBase::AARScrapyardCarryItemBase()
{
}

void AARScrapyardCarryItemBase::SetScrapyardItemTag(const FGameplayTag NewItemTag)
{
	if (!HasAuthority())
	{
		return;
	}

	ScrapyardItemTag = NewItemTag;
}

void AARScrapyardCarryItemBase::SetFallbackScrapCost(const int32 NewFallbackCost)
{
	if (!HasAuthority())
	{
		return;
	}

	FallbackScrapCost = FMath::Max(0, NewFallbackCost);
}

