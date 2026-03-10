#pragma once

#include "CoreMinimal.h"
#include "TagContentResolverTypes.h"

class TAGCONTENTRESOLVER_API ITagContentResolverRouteProvider
{
public:
	virtual ~ITagContentResolverRouteProvider() = default;

	virtual FName GetProviderName() const = 0;
	virtual int32 GetProviderPriority() const { return 0; }
	virtual void GetProvidedRoutes(TArray<FTagContentResolverRoute>& OutRoutes) const = 0;
};

class TAGCONTENTRESOLVER_API FTagContentResolverRouteProviderRegistry
{
public:
	static void RegisterProvider(ITagContentResolverRouteProvider* Provider);
	static void UnregisterProvider(ITagContentResolverRouteProvider* Provider);
	static void GetProviders(TArray<ITagContentResolverRouteProvider*>& OutProviders);
	static uint64 GetProviderGeneration();
};
