/**
 * @file ARTransitionTypes.h
 * @brief Shared transition-map context and URL option helpers.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARTransitionTypes.generated.h"

UENUM(BlueprintType)
enum class EARTransitionSourceMode : uint8
{
	Unknown = 0,
	Lobby = 1,
	Shop = 2,
	Invader = 3,
	Scrapyard = 4,
	SaveLoad = 5
};

UENUM(BlueprintType)
enum class EARTransitionReason : uint8
{
	None = 0,
	GenericContinue = 1,
	ShopToInvader = 2,
	InvaderToScrapyard = 3,
	ScrapyardToShop = 4,
	SaveLoadEntry = 5
};

UENUM(BlueprintType)
enum class EARTravelRoutePolicy : uint8
{
	ModeDefault = 0,
	ForceTransitionMap = 1,
	ForceDirect = 2
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARTransitionContext
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Transition")
	EARTransitionSourceMode SourceMode = EARTransitionSourceMode::Unknown;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Transition")
	EARTransitionReason Reason = EARTransitionReason::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Transition")
	FString DestinationURL;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Transition")
	bool bFreshLoadEntry = false;
};

namespace ARTransition
{
	ALIENRAMEN_API extern const TCHAR* OptionSourceMode;
	ALIENRAMEN_API extern const TCHAR* OptionReason;
	ALIENRAMEN_API extern const TCHAR* OptionDestinationURL;
	ALIENRAMEN_API extern const TCHAR* OptionFreshLoad;

	ALIENRAMEN_API FString AppendTransitionContextOptions(const FString& URL, const FARTransitionContext& Context);
	ALIENRAMEN_API FString BuildTransitionTravelURL(const FString& TransitionMapURL, const FARTransitionContext& Context);
	ALIENRAMEN_API void ApplyTransitionContextFromTravelOptions(const FString& OptionsString, FARTransitionContext& InOutContext);
	ALIENRAMEN_API FString LexToString(EARTransitionSourceMode SourceMode);
	ALIENRAMEN_API FString LexToString(EARTransitionReason Reason);
	ALIENRAMEN_API FString LexToString(EARTravelRoutePolicy RoutePolicy);
}
