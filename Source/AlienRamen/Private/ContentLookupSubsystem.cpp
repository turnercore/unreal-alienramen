// ContentLookupSubsystem.cpp

#include "ContentLookupSubsystem.h"
#include "ARContentLookupSettings.h"
#include "ARLog.h"
#include "UObject/SoftObjectPtr.h"
#include "StructUtils/InstancedStruct.h"

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

	// If runtime Registry isn't already set (e.g. by BP), load from project settings asset.
	const UARContentLookupSettings* Settings = GetDefault<UARContentLookupSettings>();
	const TSoftObjectPtr<UContentLookupRegistry> RegistryAsset = Settings ? Settings->RegistryAsset : TSoftObjectPtr<UContentLookupRegistry>();
	UE_LOG(ARLog, Verbose, TEXT("[ContentLookup] Initialize with settings RegistryAsset path: %s"),
		*RegistryAsset.ToSoftObjectPath().ToString());

	if (!Registry && !RegistryAsset.IsNull())
	{
		Registry = RegistryAsset.LoadSynchronous();
		if (!Registry)
		{
			UE_LOG(ARLog, Warning, TEXT("[ContentLookup] Failed to load RegistryAsset from settings: %s"),
				*RegistryAsset.ToSoftObjectPath().ToString());
		}
	}

	// Optional validation
	{
		FString Err;
		if (!ValidateRegistry(Err))
		{
			UE_LOG(ARLog, Warning, TEXT("[ContentLookup] Registry validation warning: %s"), *Err);
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
	if (!InRegistry)
	{
		UE_LOG(ARLog, Warning, TEXT("[ContentLookup] SetRegistry called with null; ignoring."));
		return;
	}

	Registry = InRegistry;
	ClearCache();

	UE_LOG(ARLog, Log, TEXT("[ContentLookup] Registry override set: %s"), *GetNameSafe(Registry));
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
		UE_LOG(ARLog, Warning, TEXT("[ContentLookup] GetTableAndRowNameFromTag failed: %s"), *OutError);
		return false;
	}

	FGameplayTag MatchedRoot;
	UDataTable* DT = ResolveTableForTag(Tag, MatchedRoot, OutError);
	if (!DT)
	{
		UE_LOG(ARLog, Warning, TEXT("[ContentLookup] Could not resolve table for tag '%s': %s"), *Tag.ToString(), *OutError);
		return false;
	}

	const FName RowName = GetLeafRowNameFromTag(Tag);
	if (RowName.IsNone())
	{
		OutError = TEXT("Computed RowName is None (empty leaf).");
		UE_LOG(ARLog, Warning, TEXT("[ContentLookup] Could not compute RowName for tag '%s': %s"), *Tag.ToString(), *OutError);
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

		UE_LOG(ARLog, Warning, TEXT("[ContentLookup] %s"), *OutError);
		return false;
	}

	return true;
}

bool UContentLookupSubsystem::ValidateRegistry(FString& OutError)
{
	OutError.Reset();

	UContentLookupRegistry* Active = GetActiveRegistry();
	if (!Active)
	{
		OutError = TEXT("No registry set (RegistryAsset and Registry are both null).");
		UE_LOG(ARLog, Warning, TEXT("[ContentLookup] Registry validation failed: %s"), *OutError);
		return false;
	}

	if (Active->Routes.Num() == 0)
	{
		OutError = TEXT("Registry has zero routes.");
		UE_LOG(ARLog, Warning, TEXT("[ContentLookup] Registry validation failed: %s"), *OutError);
		return false;
	}

	// Check for duplicates + nulls
	TSet<FGameplayTag> Seen;
	for (const FContentLookupRoute& Route : Active->Routes)
	{
		if (!Route.RootTag.IsValid())
		{
			OutError = TEXT("Registry contains a route with an invalid RootTag.");
			UE_LOG(ARLog, Warning, TEXT("[ContentLookup] Registry validation failed: %s"), *OutError);
			return false;
		}

		if (Seen.Contains(Route.RootTag))
		{
			OutError = FString::Printf(TEXT("Duplicate RootTag in registry: %s"), *Route.RootTag.ToString());
			UE_LOG(ARLog, Warning, TEXT("[ContentLookup] Registry validation failed: %s"), *OutError);
			return false;
		}
		Seen.Add(Route.RootTag);

		if (Route.DataTable.IsNull())
		{
			OutError = FString::Printf(TEXT("Route '%s' has a null DataTable reference."), *Route.RootTag.ToString());
			UE_LOG(ARLog, Warning, TEXT("[ContentLookup] Registry validation failed: %s"), *OutError);
			return false;
		}
	}

	return true;
}

UContentLookupRegistry* UContentLookupSubsystem::GetActiveRegistry()
{
	// Runtime override wins
	if (Registry)
	{
		return Registry;
	}

	const UARContentLookupSettings* Settings = GetDefault<UARContentLookupSettings>();
	const TSoftObjectPtr<UContentLookupRegistry> RegistryAsset = Settings ? Settings->RegistryAsset : TSoftObjectPtr<UContentLookupRegistry>();

	// Load from settings soft ref and KEEP it alive by storing in Registry (hard ref UPROPERTY)
	if (!RegistryAsset.IsNull())
	{
		Registry = RegistryAsset.LoadSynchronous();
		if (!Registry)
		{
			UE_LOG(ARLog, Warning, TEXT("[ContentLookup] Failed loading RegistryAsset. Path=%s (World=%s, GI=%s)"),
				*RegistryAsset.ToSoftObjectPath().ToString(),
				*GetNameSafe(GetWorld()),
				*GetNameSafe(GetGameInstance()));
		}
		return Registry;
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
		OutError = FString::Printf(TEXT("No active registry. (World=%s, GI=%s)"),
			*GetNameSafe(GetWorld()),
			*GetNameSafe(GetGameInstance()));
		UE_LOG(ARLog, Warning, TEXT("[ContentLookup] ResolveTableForTag failed for '%s': %s"), *Tag.ToString(), *OutError);
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
		UE_LOG(ARLog, Warning, TEXT("[ContentLookup] ResolveTableForTag failed: %s"), *OutError);
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
		UE_LOG(ARLog, Warning, TEXT("[ContentLookup] LoadAndCacheTable failed: %s"), *OutError);
		return nullptr;
	}

	UDataTable* DT = TableRef.LoadSynchronous();
	if (!DT)
	{
		OutError = FString::Printf(TEXT("Failed to load DataTable for route '%s'."), *RootTag.ToString());
		UE_LOG(ARLog, Warning, TEXT("[ContentLookup] LoadAndCacheTable failed: %s"), *OutError);
		return nullptr;
	}

	LoadedTables.Add(RootTag, DT);
	return DT;
}

bool UContentLookupSubsystem::LookupWithGameplayTag(
	FGameplayTag Tag,
	FInstancedStruct& OutRow,
	FString& OutError
)
{
	OutRow.Reset();
	OutError.Reset();

	UDataTable* DT = nullptr;
	FName RowName = NAME_None;

	if (!GetTableAndRowNameFromTag(Tag, DT, RowName, OutError))
	{
		UE_LOG(ARLog, Warning, TEXT("[ContentLookup] LookupWithGameplayTag failed for '%s': %s"), *Tag.ToString(), *OutError);
		return false;
	}

	if (!DT)
	{
		OutError = FString::Printf(TEXT("Resolved DataTable is null for tag '%s'."), *Tag.ToString());
		UE_LOG(ARLog, Warning, TEXT("[ContentLookup] %s"), *OutError);
		return false;
	}

	const UScriptStruct* RowStruct = DT->GetRowStruct();
	if (!RowStruct)
	{
		OutError = FString::Printf(TEXT("DataTable '%s' has no RowStruct (tag '%s')."), *GetNameSafe(DT), *Tag.ToString());
		UE_LOG(ARLog, Warning, TEXT("[ContentLookup] %s"), *OutError);
		return false;
	}

	// Find the row as raw memory.
	// Using FindRow() is the most compatible approach.
	static const FString Context(TEXT("ContentLookupSubsystem::LookupWithGameplayTag"));
	const uint8* RowData = DT->FindRowUnchecked(RowName);

	// If FindRowUnchecked isn't available in your build, comment the line above and uncomment this:
	// const uint8* RowData = reinterpret_cast<const uint8*>(DT->FindRow<uint8>(RowName, Context, /*bWarnIfMissing*/ false));

	// More universally safe fallback (no template gymnastics):
	if (!RowData)
	{
		// FindRowUnchecked can return null if missing. Validate the row exists.
		if (!DT->GetRowMap().Contains(RowName))
		{
			OutError = FString::Printf(TEXT("Row '%s' not found in DataTable '%s' for tag '%s'."),
				*RowName.ToString(),
				*GetNameSafe(DT),
				*Tag.ToString());
			UE_LOG(ARLog, Warning, TEXT("[ContentLookup] %s"), *OutError);
			return false;
		}

		// Get raw pointer from row map
		const uint8* const* RowPtr = DT->GetRowMap().Find(RowName);
		RowData = RowPtr ? *RowPtr : nullptr;
	}

	if (!RowData)
	{
		OutError = FString::Printf(TEXT("Row '%s' could not be resolved in DataTable '%s' for tag '%s'."),
			*RowName.ToString(),
			*GetNameSafe(DT),
			*Tag.ToString());
		UE_LOG(ARLog, Warning, TEXT("[ContentLookup] %s"), *OutError);
		return false;
	}

	// Copy the row into an InstancedStruct
	OutRow.InitializeAs(RowStruct);
	void* Dest = OutRow.GetMutableMemory();
	check(Dest);

	RowStruct->CopyScriptStruct(Dest, RowData);
	return true;
}
