/**
 * @file ARInvaderGameMode.h
 * @brief ARInvaderGameMode header for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARGameModeBase.h"
#include "ARInvaderGameMode.generated.h"

class AController;
class APawn;
class FProperty;
class APlayerController;
class UScriptStruct;

UCLASS()
class ALIENRAMEN_API AARInvaderGameMode : public AARGameModeBase
{
	GENERATED_BODY()

public:
	AARInvaderGameMode();

protected:
	virtual void BeginPlay() override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual void RestartPlayer(AController* NewPlayer) override;
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;

private:
	static FProperty* FindPropertyByNamePrefix(const UScriptStruct* StructType, const FString& Prefix);
	bool ResolveInvaderPawnClassFromShipTag(FGameplayTag ShipTag, TSubclassOf<APawn>& OutPawnClass) const;
	static bool FindFirstTagUnderRoot(const FGameplayTagContainer& InTags, const FGameplayTag& RootTag, FGameplayTag& OutTag);
};
