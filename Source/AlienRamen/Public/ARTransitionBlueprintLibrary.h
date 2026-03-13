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
	/** Build a transition context for travel. Set `bFreshLoadEntry` to true when entering a map fresh (not returning from another mode). */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Transition")
	static FARTransitionContext MakeTransitionContext(
		EARTransitionSourceMode SourceMode,
		EARTransitionReason Reason,
		const FString& DestinationURL,
		bool bFreshLoadEntry = false);

	/** Combine a transition map URL with serialized context options (ARTrSource/Reason/Dest/Fresh). Use the result with `OpenLevel` or server travel. */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Transition")
	static FString BuildTransitionTravelURL(const FString& TransitionMapURL, const FARTransitionContext& Context);

	/**
	 * Parse travel options (e.g., `?ARTrSource=Shop?ARTrDest=/Game/Maps/Invader`) into a context struct.
	 * Call this early in transition maps to reconstruct the payload produced by `BuildTransitionTravelURL`.
	 */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Transition")
	static FARTransitionContext ApplyTransitionContextFromTravelOptions(
		const FString& OptionsString,
		const FARTransitionContext& ExistingContext);
};

