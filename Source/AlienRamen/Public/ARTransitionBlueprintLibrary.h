/**
 * @file ARTransitionBlueprintLibrary.h
 * @brief Blueprint helpers for transition URL/context wiring.
 */
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ARTransitionTypes.h"
#include "ARTransitionBlueprintLibrary.generated.h"

UCLASS()
class ALIENRAMEN_API UARTransitionBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Transition")
	static FARTransitionContext MakeTransitionContext(
		EARTransitionSourceMode SourceMode,
		EARTransitionReason Reason,
		const FString& DestinationURL,
		bool bFreshLoadEntry = false);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Transition")
	static FString BuildTransitionTravelURL(const FString& TransitionMapURL, const FARTransitionContext& Context);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Transition")
	static FARTransitionContext ApplyTransitionContextFromTravelOptions(
		const FString& OptionsString,
		const FARTransitionContext& ExistingContext);
};

