#pragma once

#include "CoreMinimal.h"
#include "ARPlayerStateBase.h" // for EARCharacterChoice
#include "Engine/DeveloperSettings.h"
#include "ARTestingSettings.generated.h"

/**
 * Project Settings: Alien Ramen | Testing
 * Centralized config used by automation/functional tests to resolve maps, defaults, and optional debug save references.
 */
UCLASS(Config=Editor, DefaultConfig, meta=(DisplayName="Testing"))
class ALIENRAMENTESTS_API UARTestingSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UARTestingSettings();

	// Override category so it appears under Alien Ramen.
	virtual FName GetCategoryName() const override { return TEXT("Alien Ramen"); }
	virtual FName GetSectionName() const override { return TEXT("Testing"); }

public:
	// --- Maps ---

	/** Test map for Invader mode (soft reference). */
	UPROPERTY(EditAnywhere, Config, Category = "Maps")
	TSoftObjectPtr<UWorld> TestMap_Invader;

	/** Test map for Lobby/coop setup (soft reference). */
	UPROPERTY(EditAnywhere, Config, Category = "Maps")
	TSoftObjectPtr<UWorld> TestMap_Lobby;

	/** Optional test map for front-end/menu flows. */
	UPROPERTY(EditAnywhere, Config, Category = "Maps")
	TSoftObjectPtr<UWorld> TestMap_Menu;

	// --- Defaults ---

	/** Default player slot to use in tests ("P1", "P2", or "Auto"). */
	UPROPERTY(EditAnywhere, Config, Category = "Defaults")
	FName DefaultTestPlayerSlot;

	/** Default character choice for spawning test players. */
	UPROPERTY(EditAnywhere, Config, Category = "Defaults")
	EARCharacterChoice DefaultTestCharacter;

	/** Allow automation that spins up multiple clients (PIE). Gate for CI that cannot run multi-PIE. */
	UPROPERTY(EditAnywhere, Config, Category = "Defaults")
	bool bAllowNetworkedTests;

	/** Max wait time (seconds) for replication helpers in latent tests. */
	UPROPERTY(EditAnywhere, Config, Category = "Defaults", meta=(ClampMin="0.1", UIMin="0.1", UIMax="30"))
	float ReplicationWaitTimeoutSeconds;

	// --- Debug Save Reference (opt-in) ---

	/** Optional canonical slot base used by tests that opt-in to load a debug save. */
	UPROPERTY(EditAnywhere, Config, Category = "Debug Save")
	FName DebugSaveSlotBase;

	/** Optional revision for debug save (-1 means latest). */
	UPROPERTY(EditAnywhere, Config, Category = "Debug Save")
	int32 DebugSaveRevision;

	// --- Notes ---

	/** Free-form notes for QA/engineers (not used by code). */
	UPROPERTY(EditAnywhere, Config, Category = "Notes", meta=(MultiLine=true))
	FString Notes;
};

