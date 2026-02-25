#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AREnemyFacingLibrary.generated.h"

UCLASS()
class ALIENRAMEN_API UAREnemyFacingLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Re-orient actor to face straight down gameplay progression (toward low-X/player side).
	// Useful after collision/movement responses that can perturb yaw.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Enemy|Facing")
	static void ReorientEnemyFacingDown(
		AActor* EnemyActor,
		bool bUseDirectorSettingsOffset = true,
		float AdditionalYawOffset = 0.f,
		bool bZeroPitchAndRoll = true);
};

