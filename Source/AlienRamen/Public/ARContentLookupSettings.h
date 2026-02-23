#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ARContentLookupSettings.generated.h"

class UContentLookupRegistry;

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Alien Ramen Content Lookup"))
class ALIENRAMEN_API UARContentLookupSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Alien Ramen"); }

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Data")
	TSoftObjectPtr<UContentLookupRegistry> RegistryAsset;
};

