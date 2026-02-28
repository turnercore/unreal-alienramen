#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
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

protected:
	UFUNCTION(BlueprintNativeEvent, Category = "Alien Ramen|Game Instance")
	void BP_OnARGameInstanceInitialized();
	virtual void BP_OnARGameInstanceInitialized_Implementation();

	UFUNCTION(BlueprintNativeEvent, Category = "Alien Ramen|Game Instance")
	void BP_OnARGameInstanceShutdown();
	virtual void BP_OnARGameInstanceShutdown_Implementation();
};

