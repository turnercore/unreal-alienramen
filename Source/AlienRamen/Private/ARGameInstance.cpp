#include "ARGameInstance.h"

#include "ARLog.h"
#include "ARSaveSubsystem.h"
#include "OnlineSessionSettings.h"

void UARGameInstance::Init()
{
	Super::Init();
	UE_LOG(ARLog, Log, TEXT("[GameInstance] Initialized: %s"), *GetNameSafe(this));
	BP_OnARGameInstanceInitialized();
}

void UARGameInstance::Shutdown()
{
	UE_LOG(ARLog, Log, TEXT("[GameInstance] Shutdown: %s"), *GetNameSafe(this));
	BP_OnARGameInstanceShutdown();
	Super::Shutdown();
}

UARSaveSubsystem* UARGameInstance::GetARSaveSubsystem() const
{
	return GetSubsystem<UARSaveSubsystem>();
}

// ---- Network compatibility helpers ----

namespace
{
	// Session setting key advertised so clients can pre-filter incompatible sessions.
	static const FName SessionSetting_ARProtocol(TEXT("ARProtocol"));
}

bool UARGameInstance::IsARProtocolCompatible(const int32 OtherProtocol)
{
	return OtherProtocol >= ARMinCompatibleProtocol && OtherProtocol <= ARProtocolVersion;
}

void UARGameInstance::ApplyARProtocolSessionSetting(FOnlineSessionSettings& SessionSettings)
{
	SessionSettings.Set(SessionSetting_ARProtocol, ARProtocolVersion, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
}

bool UARGameInstance::GetARProtocolFromSession(const FOnlineSessionSearchResult& SearchResult, int32& OutProtocol)
{
	OutProtocol = 0;

	int32 ProtocolValue = 0;
	if (SearchResult.Session.SessionSettings.Get(SessionSetting_ARProtocol, ProtocolValue))
	{
		OutProtocol = ProtocolValue;
		return true;
	}

	return false;
}

void UARGameInstance::BP_OnARGameInstanceInitialized_Implementation()
{
}

void UARGameInstance::BP_OnARGameInstanceShutdown_Implementation()
{
}
