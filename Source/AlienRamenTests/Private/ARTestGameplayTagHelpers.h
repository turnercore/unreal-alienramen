#pragma once

#include "GameplayTagContainer.h"

namespace ARTestGameplayTags
{
	static inline FGameplayTag RequestTagNoCrash(const TCHAR* TagName)
	{
		return FGameplayTag::RequestGameplayTag(FName(TagName), false);
	}
}
