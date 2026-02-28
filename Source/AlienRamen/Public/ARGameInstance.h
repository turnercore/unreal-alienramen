#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "OnlineSessionSettings.h"
#include "ARGameInstance.generated.h"

class UARSaveSubsystem;

UCLASS(BlueprintType, Blueprintable)
class ALIENRAMEN_API UARGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
	virtual void Shutdown() override;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Game Instance")
	UARSaveSubsystem* GetARSaveSubsystem() const;

	// ---- Network compatibility helpers ----

	// Current network protocol for cross-build compatibility gates.
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Net Compatibility")
	static int32 GetARProtocolVersion() { return ARProtocolVersion; }

	// Minimum protocol that can connect to this build.
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Net Compatibility")
	static int32 GetARMinCompatibleProtocol() { return ARMinCompatibleProtocol; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Net Compatibility")
	static bool IsARProtocolCompatible(int32 OtherProtocol);

	// Host helper: stamp session settings so clients can prefilter incompatible sessions (works with Advanced Sessions/Steam/LAN).
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Net Compatibility")
	static void ApplyARProtocolSessionSetting(UPARAM(ref) FOnlineSessionSettings& SessionSettings);

	// Client helper: read protocol value from a session search result and compare to local compatibility.
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Net Compatibility")
	static bool GetARProtocolFromSession(const FOnlineSessionSearchResult& SearchResult, int32& OutProtocol);

private:
	static constexpr int32 ARProtocolVersion = 1; // bump when a breaking network change ships
	static constexpr int32 ARMinCompatibleProtocol = 1; // oldest protocol accepted by this build

protected:
	UFUNCTION(BlueprintNativeEvent, Category = "Alien Ramen|Game Instance")
	void BP_OnARGameInstanceInitialized();
	virtual void BP_OnARGameInstanceInitialized_Implementation();

	UFUNCTION(BlueprintNativeEvent, Category = "Alien Ramen|Game Instance")
	void BP_OnARGameInstanceShutdown();
	virtual void BP_OnARGameInstanceShutdown_Implementation();
};
