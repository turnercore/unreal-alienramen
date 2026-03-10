#include "TagContentResolverProvider.h"

#include "HAL/CriticalSection.h"
#include "Misc/ScopeLock.h"
#include "Templates/Atomic.h"

namespace
{
	FCriticalSection GProviderMutex;
	TArray<ITagContentResolverRouteProvider*> GProviders;
	TAtomic<uint64> GProviderGeneration(0);
}

void FTagContentResolverRouteProviderRegistry::RegisterProvider(ITagContentResolverRouteProvider* Provider)
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

void FTagContentResolverRouteProviderRegistry::UnregisterProvider(ITagContentResolverRouteProvider* Provider)
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

void FTagContentResolverRouteProviderRegistry::GetProviders(TArray<ITagContentResolverRouteProvider*>& OutProviders)
{
	FScopeLock Lock(&GProviderMutex);
	OutProviders = GProviders;
}

uint64 FTagContentResolverRouteProviderRegistry::GetProviderGeneration()
{
	return GProviderGeneration.Load();
}
