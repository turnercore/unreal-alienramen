#include "ARAIControllerBase.h"

#include "ARLog.h"

bool AARAIControllerBase::ReceivePawnSignal(
	FGameplayTag SignalTag,
	AActor* RelatedActor,
	FVector WorldLocation,
	float ScalarValue,
	bool bForwardToStateTree)
{
	(void)bForwardToStateTree;

	if (!HasAuthority())
	{
		UE_LOG(ARLog, Verbose, TEXT("[AIBase] Ignored pawn signal on non-authority controller '%s' (Tag=%s)."),
			*GetNameSafe(this), *SignalTag.ToString());
		return false;
	}

	if (!SignalTag.IsValid())
	{
		UE_LOG(ARLog, Warning, TEXT("[AIBase] Ignored pawn signal on '%s': invalid signal tag."), *GetNameSafe(this));
		return false;
	}

	BP_OnPawnSignal(SignalTag, RelatedActor, WorldLocation, ScalarValue);
	return true;
}
