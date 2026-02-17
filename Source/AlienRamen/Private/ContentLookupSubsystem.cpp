// ContentLookupSubsystem.cpp

#include "ContentLookupSubsystem.h"
#include "UObject/SoftObjectPtr.h"

DEFINE_LOG_CATEGORY(LogContentLookup);

static FString GetLeafSegment(const FString& In)
{
	int32 LastDot = INDEX_NONE;
	if (In.FindLastChar(TEXT('.'), LastDot))
	{
		return In.Mid(LastDot + 1);
	}
	return In;
}

void UContentLookupSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// If no runtime registry set, try to load the asset one.
	if (!Registry && RegistryAsset.IsValid())
	{
		Registry = RegistryAsset.Get();
	}

	if (!Registry && !RegistryAsset.IsNull())
	{
		Registry = RegistryAsset.LoadSynchronous();
		if (!Registry)
		{
			UE_LOG(LogContentLookup, Warning, TEXT("Initialize: Failed to load RegistryAsset."));
		}
	}

	// Optional: validate on startup (non-fatal).
	{
		FString Err;
		if (!ValidateRegistry(Err))
		{
			UE_LOG(LogContentLookup, Warning, TEXT("Initialize: Registry validation warning: %s"), *Err);
		}
	}
}

void UContentLookupSubsystem::Deinitialize()
{
	ClearCache();
	Registry = nullptr;

	Super::Deinitialize();
}

void UContentLookupSubsystem::SetRegistry(UContentLookupRegistry* InRegistry)
{
	Registry = InRegistry;
	ClearCache();

	FString Err;
	if (!ValidateRegistry(Err))
	{
		UE_LOG(LogContentLookup, Warning, TEXT("SetRegistry: Registry validation warning: %s"), *Err);
	}
}

void UContentLookupSubsystem::ClearCache()
{
	LoadedTables.Reset();
}

FName UContentLookupSubsystem::GetLeafRowNameFromTag(FGameplayTag Tag)
{
	const FString TagStr = Tag.GetTagName().ToString();
	return FName(*GetLeafSegment(TagStr));
}

bool UContentLookupSubsystem::GetTableAndRowNameFromTag(
	FGameplayTag Tag,
	UDataTable*& OutDataTable,
	FName& OutRowName,
	FString& OutError
)
{
	OutDataTable = nullptr;
	OutRowName = NAME_None;
	OutError.Reset();

	if (!Tag.IsValid())
	{
		OutError = TEXT("Tag is invalid.");
		UE_LOG(LogContentLookup, Warning, TEXT("GetTableAndRowNameFromTag: %s"), *OutError);
		return false;
	}

	FGameplayTag MatchedRoot;
	UDataTable* DT = ResolveTableForTag(Tag, MatchedRoot, OutError);
	if (!DT)
	{
		UE_LOG(LogContentLookup, Warning, TEXT("GetTableAndRowNameFromTag(%s): %s"), *Tag.ToString(), *OutError);
		return false;
	}

	const FName RowName = GetLeafRowNameFromTag(Tag);
	if (RowName.IsNone())
	{
		OutError = TEXT("Computed RowName is None (empty leaf).");
		UE_LOG(LogContentLookup, Warning, TEXT("GetTableAndRowNameFromTag(%s): %s"), *Tag.ToString(), *OutError);
		return false;
	}

	OutDataTable = DT;
	OutRowName = RowName;
	return true;
}

bool UContentLookupSubsystem::DoesRowExistForTag(FGameplayTag Tag, FString& OutError)
{
	OutError.Reset();

	UDataTable* DT = nullptr;
	FName RowName = NAME_None;
	if (!GetTableAndRowNameFromTag(Tag, DT, RowName, OutError))
	{
		return false;
	}

	// Row existence check (fast)
	if (!DT->GetRowMap().Contains(RowName))
	{
		OutError = FString::Printf(TEXT("Row '%s' not found in DataTable '%s' for tag '%s'."),
			*RowName.ToString(),
			*GetNameSafe(DT),
			*Tag.ToString());

		UE_LOG(LogContentLookup, Warning, TEXT("%s"), *OutError);
		return false;
	}

	return true;
}

bool UContentLookupSubsystem::ValidateRegistry(FString& OutError) const
{
	OutError.Reset();

	UContentLookupRegistry* Active = GetActiveRegistry();
	if (!Active)
	{
		OutError = TEXT("No registry set (RegistryAsset and Registry are both null).");
		return false;
	}

	if (Active->Routes.Num() == 0)
	{
		OutError = TEXT("Registry has zero routes.");
		return false;
	}

	// Check for duplicates + nulls
	TSet<FGameplayTag> Seen;
	for (const FContentLookupRoute& Route : Active->Routes)
	{
		if (!Route.RootTag.IsValid())
		{
			OutError = TEXT("Registry contains a route with an invalid RootTag.");
			return false;
		}

		if (Seen.Contains(Route.RootTag))
		{
			OutError = FString::Printf(TEXT("Duplicate RootTag in registry: %s"), *Route.RootTag.ToString());
			return false;
		}
		Seen.Add(Route.RootTag);

		if (Route.DataTable.IsNull())
		{
			OutError = FString::Printf(TEXT("Route '%s' has a null DataTable reference."), *Route.RootTag.ToString());
			return false;
		}
	}

	return true;
}

UContentLookupRegistry* UContentLookupSubsystem::GetActiveRegistry() const
{
	if (Registry)
	{
		return Registry;
	}

	// If RegistryAsset was already resolved to a loaded object, prefer that.
	if (RegistryAsset.IsValid())
	{
		return RegistryAsset.Get();
	}

	return nullptr;
}

UDataTable* UContentLookupSubsystem::ResolveTableForTag(FGameplayTag Tag, FGameplayTag& OutMatchedRoot, FString& OutError)
{
	OutMatchedRoot = FGameplayTag();
	OutError.Reset();

	UContentLookupRegistry* Active = GetActiveRegistry();
	if (!Active)
	{
		OutError = TEXT("No active registry.");
		return nullptr;
	}

	// Best-match strategy: pick the most specific (longest) matching RootTag.
	// Example: if you ever had both Unlocks and Unlocks.Ships, choose Unlocks.Ships.
	int32 BestLen = -1;
	const FContentLookupRoute* BestRoute = nullptr;

	for (const FContentLookupRoute& Route : Active->Routes)
	{
		if (!Route.RootTag.IsValid())
		{
			continue;
		}

		if (Tag.MatchesTag(Route.RootTag))
		{
			const int32 Len = Route.RootTag.ToString().Len();
			if (Len > BestLen)
			{
				BestLen = Len;
				BestRoute = &Route;
				OutMatchedRoot = Route.RootTag;
			}
		}
	}

	if (!BestRoute)
	{
		OutError = FString::Printf(TEXT("No route found for tag '%s'."), *Tag.ToString());
		return nullptr;
	}

	// Cache hit?
	if (TObjectPtr<UDataTable>* Cached = LoadedTables.Find(OutMatchedRoot))
	{
		return Cached->Get();
	}

	return LoadAndCacheTable(OutMatchedRoot, BestRoute->DataTable, OutError);
}

UDataTable* UContentLookupSubsystem::LoadAndCacheTable(const FGameplayTag& RootTag, const TSoftObjectPtr<UDataTable>& TableRef, FString& OutError)
{
	OutError.Reset();

	if (TableRef.IsNull())
	{
		OutError = FString::Printf(TEXT("Route '%s' has a null DataTable reference."), *RootTag.ToString());
		return nullptr;
	}

	UDataTable* DT = TableRef.LoadSynchronous();
	if (!DT)
	{
		OutError = FString::Printf(TEXT("Failed to load DataTable for route '%s'."), *RootTag.ToString());
		return nullptr;
	}

	LoadedTables.Add(RootTag, DT);
	return DT;
}
