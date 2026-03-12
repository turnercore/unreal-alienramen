#include "ARTransitionGameState.h"

#include "ARGameStateModeStructs.h"
#include "Net/UnrealNetwork.h"

AARTransitionGameState::AARTransitionGameState()
{
	ClassStateStruct = FARTransitionGameStateData::StaticStruct();
}

UScriptStruct* AARTransitionGameState::GetStateStruct_Implementation() const
{
	return ClassStateStruct ? ClassStateStruct.Get() : FARTransitionGameStateData::StaticStruct();
}

void AARTransitionGameState::SetTransitionContext(const FARTransitionContext& NewContext)
{
	if (!HasAuthority())
	{
		return;
	}

	if (TransitionContext.SourceMode == NewContext.SourceMode
		&& TransitionContext.Reason == NewContext.Reason
		&& TransitionContext.DestinationURL == NewContext.DestinationURL
		&& TransitionContext.bFreshLoadEntry == NewContext.bFreshLoadEntry)
	{
		return;
	}

	const FARTransitionContext OldContext = TransitionContext;
	TransitionContext = NewContext;
	OnRep_TransitionContext(OldContext);
	ForceNetUpdate();
}

void AARTransitionGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AARTransitionGameState, TransitionContext);
}

void AARTransitionGameState::OnRep_TransitionContext(const FARTransitionContext& OldContext)
{
	(void)OldContext;
	OnTransitionContextChanged.Broadcast(TransitionContext);
}

