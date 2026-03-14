#pragma once

#include "CoreMinimal.h"
#include "TagKeyTypes.h"

class TAGKEY_API ITagKeyRouteProvider
{
public:
	virtual ~ITagKeyRouteProvider() = default;

	virtual FName GetProviderName() const = 0;
	virtual int32 GetProviderPriority() const { return 0; }
	virtual void GetProvidedRoutes(TArray<FTagKeyRoute>& OutRoutes) const = 0;
};

class TAGKEY_API FTagKeyRouteProviderRegistry
{
public:
	static void RegisterProvider(ITagKeyRouteProvider* Provider);
	static void UnregisterProvider(ITagKeyRouteProvider* Provider);
	static void GetProviders(TArray<ITagKeyRouteProvider*>& OutProviders);
	static uint64 GetProviderGeneration();
};
