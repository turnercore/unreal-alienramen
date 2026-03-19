/**
 * @file ARGameInstance.h
 * @brief ARGameInstance header for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "ARGameInstance.generated.h"

class UARSaveSubsystem;
class UARSessionSubsystem;
class UARTravelSubsystem;
class UARParleySaveBridge;
class UARFactionVotingSubsystem;
class FOnlineSessionSettings;
class FOnlineSessionSearchResult;
class IConsoleObject;

/** GameInstance root that owns the save subsystem and build/network compatibility helpers. */
UCLASS(BlueprintType, Blueprintable)
class ALIENRAMEN_API UARGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
	virtual void Shutdown() override;

	/** Accessor for the save subsystem (authority for save/load/hydration). */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Game Instance")
	UARSaveSubsystem* GetARSaveSubsystem() const;

	/** Accessor for the session subsystem (authority for session join/host flows). */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Game Instance")
	UARSessionSubsystem* GetARSessionSubsystem() const;

	/** Accessor for the travel subsystem (authority for travel execution flows). */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Game Instance")
	UARTravelSubsystem* GetARTravelSubsystem() const;

	/** Accessor for the AR-owned faction voting subsystem (election/vote runtime). */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Game Instance")
	UARFactionVotingSubsystem* GetARFactionVotingSubsystem() const;

	// ---- Network compatibility helpers ----

	// Current network protocol for cross-build compatibility gates.
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Net Compatibility")
	static int32 GetARProtocolVersion() { return ARProtocolVersion; }

	// Minimum protocol that can connect to this build.
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Net Compatibility")
	static int32 GetARMinCompatibleProtocol() { return ARMinCompatibleProtocol; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Net Compatibility")
	static bool IsARProtocolCompatible(int32 OtherProtocol);

	// C++ host helper: stamp session settings so clients can prefilter incompatible sessions (works with Advanced Sessions/Steam/LAN).
	static void ApplyARProtocolSessionSetting(UPARAM(ref) FOnlineSessionSettings& SessionSettings);

	// C++ client helper: read protocol value from a session search result and compare to local compatibility.
	static bool GetARProtocolFromSession(const FOnlineSessionSearchResult& SearchResult, int32& OutProtocol);

	// ---- Build/version helpers ----
	// Project version from DefaultGame.ini (ProjectVersion). Use this on main menu/UI.
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Build")
	static FString GetARProjectVersion();

	// Composite fingerprint: "<ProjectVersion> | NetProto:<proto> | SaveSchema:<schema>".
	// Useful for bug reports and displaying build info in menus.
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Build")
	static FString GetARBuildFingerprint();

private:
	static constexpr int32 ARProtocolVersion = 1; // bump when a breaking network change ships
	static constexpr int32 ARMinCompatibleProtocol = 1; // oldest protocol accepted by this build
	void RegisterDebugConsoleCommands();
	void UnregisterDebugConsoleCommands();
	void HandleConsoleArDebug(const TArray<FString>& Args);
	void HandleConsoleArDebugAll(const TArray<FString>& Args);
	bool TryResolveDebugVerbosityArg(const TArray<FString>& Args, FString& OutTargetVerbosity) const;
	IConsoleObject* CmdArDebug = nullptr;
	IConsoleObject* CmdArDebugAll = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UARParleySaveBridge> ParleySaveBridge;

protected:
	UFUNCTION(BlueprintNativeEvent, Category = "Alien Ramen|Game Instance")
	void BP_OnARGameInstanceInitialized();
	virtual void BP_OnARGameInstanceInitialized_Implementation();

	UFUNCTION(BlueprintNativeEvent, Category = "Alien Ramen|Game Instance")
	void BP_OnARGameInstanceShutdown();
	virtual void BP_OnARGameInstanceShutdown_Implementation();
};
