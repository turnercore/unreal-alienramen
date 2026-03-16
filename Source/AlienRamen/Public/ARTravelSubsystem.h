/**
 * @file ARTravelSubsystem.h
 * @brief Authority travel orchestration separate from persistence ownership.
 */
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ARTransitionTypes.h"
#include "ARTravelSubsystem.generated.h"

class UARSaveSubsystem;
class UWorld;
struct FGameplayTag;

/**
 * GameInstance subsystem that owns authority travel execution.
 * Persistence remains in UARSaveSubsystem; this subsystem only coordinates
 * readiness checks, optional pre-travel save, and world travel calls.
 */
UCLASS()
class ALIENRAMEN_API UARTravelSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * Authority-only helper that travels into the map recorded by the currently loaded save
	 * using a SaveLoad transition context.
	 */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Travel", meta = (BlueprintAuthorityOnly))
	bool TravelToLoadedSaveDestination(bool bUseOpenLevelInPIE = false, const FString& TransitionMapURL = TEXT("/Game/Maps/Lvl_Loading"));

	/** C++ helper for save-load travel URL assembly: resolves fallback destination from mode tag and applies transition context options. */
	static FString BuildLoadedSaveTravelURL(const FString& PreferredDestinationURL, FGameplayTag SavedModeTag, const FString& TransitionMapURL, FARTransitionContext& OutTransitionContext);

	/**
	 * Authority-only server travel helper.
	 * Flow: optional readiness gate -> capture GameState travel snapshot -> optional disk save -> ServerTravel with enforced listen option.
	 */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Travel", meta = (BlueprintAuthorityOnly))
	bool RequestServerTravel(
		const FString& URL,
		bool bSkipReadyChecks = false,
		bool bAbsolute = false,
		bool bSkipGameNotify = false,
		bool bPersistSaveBeforeTravel = true);

	/**
	 * Authority-only non-networked level open.
	 * Enforces listen option so host remains authoritative.
	 */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Travel", meta = (BlueprintAuthorityOnly))
	bool RequestOpenLevel(
		const FString& LevelName,
		const FString& Options = TEXT(""),
		bool bSkipReadyChecks = false,
		bool bAbsolute = false,
		bool bPersistSaveBeforeTravel = true);

private:
	UARSaveSubsystem* ResolveSaveSubsystem() const;
	bool ArePlayersReadyForTravel(bool bSkipReadyChecks, FString& OutError) const;
	bool CaptureGameStateForTravel(UWorld* World, UARSaveSubsystem* SaveSubsystem) const;
	static bool SplitTravelURL(const FString& InTravelURL, FString& OutLevelName, FString& OutOptions);
};
