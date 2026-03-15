#include "Misc/AutomationTest.h"

#include "Async/Async.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "GameplayTagContainer.h"
#include "TagKeyProvider.h"
#include "TagKeySettings.h"
#include "TagKeySubsystem.h"
#include "TagKeyTypes.h"

namespace
{
	class FScopedTagResolverSettingsOverride
	{
	public:
		FScopedTagResolverSettingsOverride()
		{
			if (UTagKeySettings* Settings = GetMutableDefault<UTagKeySettings>())
			{
				OriginalProjectRoutes = Settings->ProjectRoutes;
				OriginalPreloadPolicy = Settings->PreloadPolicy;
				OriginalAutoPreloadRowSoftReferences = Settings->bAutoPreloadRowSoftReferences;
				OriginalDedupeFailures = Settings->bDeduplicateFailureLogs;
				OriginalMaxRememberedFailureLogs = Settings->MaxRememberedFailureLogs;
			}
		}

		~FScopedTagResolverSettingsOverride()
		{
			if (UTagKeySettings* Settings = GetMutableDefault<UTagKeySettings>())
			{
				Settings->ProjectRoutes = OriginalProjectRoutes;
				Settings->PreloadPolicy = OriginalPreloadPolicy;
				Settings->bAutoPreloadRowSoftReferences = OriginalAutoPreloadRowSoftReferences;
				Settings->bDeduplicateFailureLogs = OriginalDedupeFailures;
				Settings->MaxRememberedFailureLogs = OriginalMaxRememberedFailureLogs;
			}
		}

	private:
		TArray<FTagKeyProjectRoute> OriginalProjectRoutes;
		ETagKeyPreloadPolicy OriginalPreloadPolicy = ETagKeyPreloadPolicy::CriticalRoots;
		bool OriginalAutoPreloadRowSoftReferences = true;
		bool OriginalDedupeFailures = true;
		int32 OriginalMaxRememberedFailureLogs = 256;
	};

	class FTestRouteProvider final : public ITagKeyRouteProvider
	{
	public:
		virtual FName GetProviderName() const override
		{
			return TEXT("AlienRamen.TagKey.Tests.Provider");
		}

		virtual int32 GetProviderPriority() const override
		{
			return 1000;
		}

		virtual void GetProvidedRoutes(TArray<FTagKeyRoute>& OutRoutes) const override
		{
			OutRoutes = Routes;
		}

		TArray<FTagKeyRoute> Routes;
	};

	FGameplayTag RequestTagChecked(const TCHAR* TagName)
	{
		const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(TagName), false);
		checkf(Tag.IsValid(), TEXT("Expected gameplay tag '%s' to exist for automation test."), TagName);
		return Tag;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTagKey_OverlapRootsFailTest,
	"AlienRamen.TagKey.Validation.OverlappingRootsFail",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTagKey_OverlapRootsFailTest::RunTest(const FString& Parameters)
{
	FScopedTagResolverSettingsOverride ScopedSettings;
	UTagKeySettings* Settings = GetMutableDefault<UTagKeySettings>();
	TestNotNull(TEXT("Settings should exist"), Settings);
	if (!Settings)
	{
		return false;
	}

	const FGameplayTag ShopRoot = RequestTagChecked(TEXT("Shop"));
	const FGameplayTag ShopStationRoot = RequestTagChecked(TEXT("Shop.Station"));

	FTagKeyProjectRoute ParentRoute;
	ParentRoute.RootTag = ShopRoot;
	ParentRoute.DataTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/Data/DT_ShopStations.DT_ShopStations")));

	FTagKeyProjectRoute ChildRoute;
	ChildRoute.RootTag = ShopStationRoot;
	ChildRoute.DataTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/Data/DT_ShopStations.DT_ShopStations")));

	Settings->ProjectRoutes = { ParentRoute, ChildRoute };

	UDataTable* Table = nullptr;
	FString Error;
	const bool bResolved = UTagKeySubsystem::TryResolveDataTableForRootTagFromConfiguredRoutes(
		ShopStationRoot,
		Table,
		Error);

	TestFalse(TEXT("Overlapping route hierarchy must fail validation"), bResolved);
	TestTrue(
		TEXT("Failure should explicitly describe overlapping hierarchy"),
		Error.Contains(TEXT("Overlapping RootTag hierarchy detected")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTagKey_ProviderGenerationInvalidatesStaticCacheTest,
	"AlienRamen.TagKey.Cache.ProviderGenerationInvalidatesStaticHelpers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTagKey_ProviderGenerationInvalidatesStaticCacheTest::RunTest(const FString& Parameters)
{
	FScopedTagResolverSettingsOverride ScopedSettings;
	UTagKeySettings* Settings = GetMutableDefault<UTagKeySettings>();
	TestNotNull(TEXT("Settings should exist"), Settings);
	if (!Settings)
	{
		return false;
	}

	FTagKeyProjectRoute BaseRoute;
	BaseRoute.RootTag = RequestTagChecked(TEXT("Parley.Speaker"));
	BaseRoute.DataTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/Data/DT_Speakers.DT_Speakers")));
	Settings->ProjectRoutes = { BaseRoute };

	const FGameplayTag ProviderRoot = RequestTagChecked(TEXT("Parley.Factions"));
	FTestRouteProvider Provider;
	{
		FTagKeyRoute ProviderRoute;
		ProviderRoute.RootTag = ProviderRoot;
		ProviderRoute.DataTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/Data/DT_Factions.DT_Factions")));
		Provider.Routes = { ProviderRoute };
	}

	UDataTable* Table = nullptr;
	FString Error;

	const bool bResolvesBeforeRegister = UTagKeySubsystem::TryResolveDataTableForRootTagFromConfiguredRoutes(
		ProviderRoot,
		Table,
		Error);
	TestFalse(TEXT("Route from unregistered provider should not resolve"), bResolvesBeforeRegister);

	FTagKeyRouteProviderRegistry::RegisterProvider(&Provider);
	const bool bResolvesAfterRegister = UTagKeySubsystem::TryResolveDataTableForRootTagFromConfiguredRoutes(
		ProviderRoot,
		Table,
		Error);
	TestTrue(TEXT("Route should resolve after provider registration without manual rebuild"), bResolvesAfterRegister);

	FTagKeyRouteProviderRegistry::UnregisterProvider(&Provider);
	const bool bResolvesAfterUnregister = UTagKeySubsystem::TryResolveDataTableForRootTagFromConfiguredRoutes(
		ProviderRoot,
		Table,
		Error);
	TestFalse(TEXT("Route should stop resolving after provider unregister without manual rebuild"), bResolvesAfterUnregister);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTagKey_GameThreadEnforcementTest,
	"AlienRamen.TagKey.Threading.InstanceMethodsRequireGameThread",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTagKey_GameThreadEnforcementTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>(GetTransientPackage());
	TestNotNull(TEXT("Transient game instance should be constructible for subsystem guard-path test"), TestGameInstance);
	if (!TestGameInstance)
	{
		return false;
	}

	UTagKeySubsystem* Resolver = NewObject<UTagKeySubsystem>(TestGameInstance);
	TestNotNull(TEXT("Transient resolver should be constructible for guard-path test"), Resolver);
	if (!Resolver)
	{
		return false;
	}

	const TFuture<TPair<bool, FString>> WorkerResult = Async(EAsyncExecution::ThreadPool, [Resolver]()
	{
		FString Error;
		const bool bOk = Resolver->TryValidateRouteConfiguration(Error);
		return TPair<bool, FString>(bOk, Error);
	});

	const TPair<bool, FString> Result = WorkerResult.Get();
	TestFalse(TEXT("Non-game-thread call should fail"), Result.Key);
	TestTrue(
		TEXT("Failure should clearly explain game-thread requirement"),
		Result.Value.Contains(TEXT("must be called on the game thread")));

	return true;
}
