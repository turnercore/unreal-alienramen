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

FTagKeyRouteProviderRegistration::FTagKeyRouteProviderRegistration(ITagKeyRouteProvider* InProvider)
	: Provider(InProvider)
{
	FTagKeyRouteProviderRegistry::RegisterProvider(Provider);
}

FTagKeyRouteProviderRegistration::~FTagKeyRouteProviderRegistration()
{
	Reset();
}

FTagKeyRouteProviderRegistration::FTagKeyRouteProviderRegistration(FTagKeyRouteProviderRegistration&& Other) noexcept
	: Provider(Other.Provider)
{
	Other.Provider = nullptr;
}

FTagKeyRouteProviderRegistration& FTagKeyRouteProviderRegistration::operator=(FTagKeyRouteProviderRegistration&& Other) noexcept
{
	if (this != &Other)
	{
		Reset();
		Provider = Other.Provider;
		Other.Provider = nullptr;
	}
	return *this;
}

void FTagKeyRouteProviderRegistration::Reset()
{
	if (Provider)
	{
		FTagKeyRouteProviderRegistry::UnregisterProvider(Provider);
		Provider = nullptr;
	}
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

FTagKeyRouteProviderRegistration FTagKeyRouteProviderRegistry::RegisterProviderScoped(ITagKeyRouteProvider* Provider)
{
	return FTagKeyRouteProviderRegistration(Provider);
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
