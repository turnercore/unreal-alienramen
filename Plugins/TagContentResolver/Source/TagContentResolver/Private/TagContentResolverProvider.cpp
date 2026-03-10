#include "TagContentResolverProvider.h"

#include "HAL/CriticalSection.h"
#include "Misc/ScopeLock.h"

namespace
{
	FCriticalSection GProviderMutex;
	TArray<ITagContentResolverRouteProvider*> GProviders;
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
	}
}

void FTagContentResolverRouteProviderRegistry::UnregisterProvider(ITagContentResolverRouteProvider* Provider)
{
	if (!Provider)
	{
		return;
	}

	FScopeLock Lock(&GProviderMutex);
	GProviders.RemoveSwap(Provider);
}

void FTagContentResolverRouteProviderRegistry::GetProviders(TArray<ITagContentResolverRouteProvider*>& OutProviders)
{
	FScopeLock Lock(&GProviderMutex);
	OutProviders = GProviders;
}
