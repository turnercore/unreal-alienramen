#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ARPlayerController.generated.h"

class UARAbilitySet;

UCLASS()
class ALIENRAMEN_API AARPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AARPlayerController();

	// Common abilities/effects every pawn gets when possessed (server grants via pawn).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AR|Abilities")
	TObjectPtr<UARAbilitySet> CommonAbilitySet;

	// Convenience accessor
	UFUNCTION(BlueprintCallable, Category = "AR|Abilities")
	const UARAbilitySet* GetCommonAbilitySet() const { return CommonAbilitySet; }

	// Client endpoint for persisting server-canonical save snapshots locally.
	UFUNCTION(Client, Reliable)
	void ClientPersistCanonicalSave(const TArray<uint8>& SaveBytes, FName SlotBaseName, int32 SlotNumber);

	// Client requests current server-canonical save snapshot on join/connect.
	UFUNCTION(Server, Reliable)
	void ServerRequestCanonicalSaveSync();

	// Session leave entrypoint for UI/BP. Routes to server when called by clients.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Session")
	void LeaveSession();

	// Server-side leave request handler.
	UFUNCTION(Server, Reliable)
	void ServerLeaveSession();

	// Controller travel entrypoint for UI/BP. Routes to server when called by clients.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Travel")
	void TryStartTravel(const FString& URL, const FString& Options = "", bool bSkipReadyChecks = false, bool bAbsolute = false, bool bSkipGameNotify = false);

	// Server-side travel request handler.
	UFUNCTION(Server, Reliable)
	void ServerTryStartTravel(const FString& URL, const FString& Options = "", bool bSkipReadyChecks = false, bool bAbsolute = false, bool bSkipGameNotify = false);

protected:
	virtual void BeginPlay() override;

private:
	void LeaveSessionInternal();
	void TryStartTravelInternal(const FString& URL, const FString& Options, bool bSkipReadyChecks, bool bAbsolute, bool bSkipGameNotify);

	UPROPERTY(Transient)
	bool bRequestedInitialCanonicalSaveSync = false;
};
