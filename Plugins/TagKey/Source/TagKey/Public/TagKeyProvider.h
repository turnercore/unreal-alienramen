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

/**
 * RAII registration token for provider implementations that want automatic unregister-on-destroy
 * semantics instead of calling FTagKeyRouteProviderRegistry::UnregisterProvider manually.
 */
class TAGKEY_API FTagKeyRouteProviderRegistration
{
public:
	FTagKeyRouteProviderRegistration() = default;
	explicit FTagKeyRouteProviderRegistration(ITagKeyRouteProvider* Provider);
	~FTagKeyRouteProviderRegistration();

	FTagKeyRouteProviderRegistration(const FTagKeyRouteProviderRegistration&) = delete;
	FTagKeyRouteProviderRegistration& operator=(const FTagKeyRouteProviderRegistration&) = delete;
	FTagKeyRouteProviderRegistration(FTagKeyRouteProviderRegistration&& Other) noexcept;
	FTagKeyRouteProviderRegistration& operator=(FTagKeyRouteProviderRegistration&& Other) noexcept;

	void Reset();
	explicit operator bool() const { return Provider != nullptr; }

private:
	ITagKeyRouteProvider* Provider = nullptr;
};

class TAGKEY_API FTagKeyRouteProviderRegistry
{
public:
	static void RegisterProvider(ITagKeyRouteProvider* Provider);
	static void UnregisterProvider(ITagKeyRouteProvider* Provider);
	[[nodiscard]] static FTagKeyRouteProviderRegistration RegisterProviderScoped(ITagKeyRouteProvider* Provider);
	static void GetProviders(TArray<ITagKeyRouteProvider*>& OutProviders);
	static uint64 GetProviderGeneration();
};
