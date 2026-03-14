#include "TagKeyProvider.h"

#include "HAL/CriticalSection.h"
#include "Misc/ScopeLock.h"
#include "Templates/Atomic.h"

namespace
{
	FCriticalSection GProviderMutex;
	TArray<ITagKeyRouteProvider*> GProviders;
	TAtomic<uint64> GProviderGeneration(0);
}

void FTagKeyRouteProviderRegistry::RegisterProvider(ITagKeyRouteProvider* Provider)
{
	if (!Provider)
	{
		return;
	}

	FScopeLock Lock(&GProviderMutex);
	if (!GProviders.Contains(Provider))
	{
		GProviders.Add(Provider);
		++GProviderGeneration;
	}
}

void FTagKeyRouteProviderRegistry::UnregisterProvider(ITagKeyRouteProvider* Provider)
{
	if (!Provider)
	{
		return;
	}

	FScopeLock Lock(&GProviderMutex);
	if (GProviders.RemoveSwap(Provider) > 0)
	{
		++GProviderGeneration;
	}
}

void FTagKeyRouteProviderRegistry::GetProviders(TArray<ITagKeyRouteProvider*>& OutProviders)
{
	FScopeLock Lock(&GProviderMutex);
	OutProviders = GProviders;
}

uint64 FTagKeyRouteProviderRegistry::GetProviderGeneration()
{
	return GProviderGeneration.Load();
}
