#include "ARGameInstance.h"

#include "ARLog.h"
#include "ARSaveSubsystem.h"

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

void UARGameInstance::BP_OnARGameInstanceInitialized_Implementation()
{
}

void UARGameInstance::BP_OnARGameInstanceShutdown_Implementation()
{
}

