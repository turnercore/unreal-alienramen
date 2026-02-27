#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ARSaveUserSettings.generated.h"

UCLASS(Config=GameUserSettings, DefaultConfig, BlueprintType)
class ALIENRAMEN_API UARSaveUserSettings : public UObject
{
	GENERATED_BODY()

public:
	// Number of revisioned backups to keep per save slot base.
	// Example: with 5, saving revision 10 keeps 6..10 and prunes 0..5.
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Save", meta=(ClampMin="1", UIMin="1", ClampMax="100", UIMax="100"))
	int32 MaxBackupRevisions = 5;
};

