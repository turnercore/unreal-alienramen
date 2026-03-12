#include "ARTransitionBlueprintLibrary.h"

FARTransitionContext UARTransitionBlueprintLibrary::MakeTransitionContext(
	const EARTransitionSourceMode SourceMode,
	const EARTransitionReason Reason,
	const FString& DestinationURL,
	const bool bFreshLoadEntry)
{
	FARTransitionContext Context;
	Context.SourceMode = SourceMode;
	Context.Reason = Reason;
	Context.DestinationURL = DestinationURL;
	Context.bFreshLoadEntry = bFreshLoadEntry;
	return Context;
}

FString UARTransitionBlueprintLibrary::BuildTransitionTravelURL(const FString& TransitionMapURL, const FARTransitionContext& Context)
{
	return ARTransition::BuildTransitionTravelURL(TransitionMapURL, Context);
}

FARTransitionContext UARTransitionBlueprintLibrary::ApplyTransitionContextFromTravelOptions(
	const FString& OptionsString,
	const FARTransitionContext& ExistingContext)
{
	FARTransitionContext Context = ExistingContext;
	ARTransition::ApplyTransitionContextFromTravelOptions(OptionsString, Context);
	return Context;
}

