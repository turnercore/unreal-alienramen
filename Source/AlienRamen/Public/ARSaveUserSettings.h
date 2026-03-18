/**
 * @file ARSaveUserSettings.h
 * @brief ARSaveUserSettings header for Alien Ramen.
 */
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

	// When canonical save payload bytes reach this size, the save subsystem emits a warning log so growth is visible before chunking becomes necessary.
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Save", meta = (ClampMin = "1", UIMin = "1"))
	int32 CanonicalSaveWarningSizeBytes = 524288;

	// When canonical save payload bytes reach this size, the save subsystem escalates the payload log to error severity.
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Save", meta = (ClampMin = "1", UIMin = "1"))
	int32 CanonicalSaveCriticalSizeBytes = 1048576;

	// Emits a log entry for each canonical save payload so payload growth can be tracked over time.
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Save")
	bool bLogCanonicalSavePayloadSize = true;
};

