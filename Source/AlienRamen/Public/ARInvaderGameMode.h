/**
 * @file ARInvaderGameMode.h
 * @brief ARInvaderGameMode header for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARGameModeBase.h"
#include "ARInvaderTypes.h"
#include "ARInvaderGameMode.generated.h"

class AController;
class APawn;
class APlayerController;
class UARInvaderDirectorSubsystem;

UCLASS()
class ALIENRAMEN_API AARInvaderGameMode : public AARGameModeBase
{
	GENERATED_BODY()

public:
	AARInvaderGameMode();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual void RestartPlayer(AController* NewPlayer) override;
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;

	/**
	 * Blueprint hook fired on authority immediately after the director reports a run end.
	 * Use this to orchestrate score cards/cinematics before calling FinalizeInvaderRunAndTravel.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|Invader|Run End", meta = (DisplayName = "On Invader Run Ended"))
	void OnInvaderRunEnded(EARInvaderRunEndReason EndReason);

public:
	/**
	 * Authority entrypoint for ending Invader mode and traveling out to the post-run destination.
	 * Uses InTravelURL when provided; otherwise falls back to DefaultPostRunTravelURL.
	 */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader|Run End", meta = (BlueprintAuthorityOnly))
	bool FinalizeInvaderRunAndTravel(const FString& InTravelURL = TEXT(""));

private:
	bool ResolveInvaderPawnClassForCharacterTag(FGameplayTag CharacterTag, const AARPlayerStateBase* OwnerPlayerState, TSubclassOf<APawn>& OutPawnClass) const;
	bool ResolveCharacterOwnedLoadout(FGameplayTag CharacterTag, const AARPlayerStateBase* OwnerPlayerState, FGameplayTagContainer& OutLoadoutTags) const;
	AARPlayerStateBase* ResolveCharacterOwnerForTag(FGameplayTag CharacterTag) const;
	bool ResolveCharacterSpawnTransform(FGameplayTag CharacterTag, const AARPlayerStateBase* OwnerPlayerState, FTransform& OutTransform) const;
	bool ReconcileInitialControlledCharacterPawns() const;
	bool TryRestoreMissingCharacterPawns() const;
	bool TryRestoreMissingCharacterPawn(FGameplayTag CharacterTag) const;
	void UpdateInactiveCharacterPawnDamageState() const;
	bool ResolveInvaderPawnClassFromShipTag(FGameplayTag ShipTag, TSubclassOf<APawn>& OutPawnClass) const;
	static bool FindFirstTagUnderRoot(const FGameplayTagContainer& InTags, const FGameplayTag& RootTag, FGameplayTag& OutTag);

	UFUNCTION()
	void HandleInvaderRunEnded(EARInvaderRunEndReason EndReason);

	void NotifyControllersInvaderRunEnded(EARInvaderRunEndReason EndReason);
	void TriggerAutoTravelAfterRunEnd();

	UFUNCTION()
	void HandleInvaderRunEndAutoTravelTimer();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Invader|Run End", meta = (AllowPrivateAccess = "true"))
	bool bAutoTravelAfterRunEnd = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Invader|Run End", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float AutoTravelAfterRunEndDelaySeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Invader|Run End", meta = (AllowPrivateAccess = "true", ToolTip = "Final gameplay map URL to load when Invader run finalizes (for example /Game/Maps/Lvl_Scrapyard)."))
	FString DefaultPostRunTravelURL = TEXT("/Game/Maps/Lvl_Scrapyard");

	UPROPERTY(Transient)
	EARInvaderRunEndReason LastHandledRunEndReason = EARInvaderRunEndReason::None;

	UPROPERTY(Transient)
	bool bRunEndTravelRequested = false;

	FTimerHandle RunEndAutoTravelTimer;
};
