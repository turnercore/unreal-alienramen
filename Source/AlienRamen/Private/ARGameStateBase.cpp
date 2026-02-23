#include "ARGameStateBase.h"

AARGameStateBase::AARGameStateBase()
{
	bReplicates = true;
}

bool AARGameStateBase::ApplyStateFromStruct_Implementation(const FInstancedStruct& SavedState)
{
	if (!SavedState.IsValid())
	{
		return false;
	}

	if (!HasAuthority())
	{
		ServerApplyStateFromStruct(SavedState);
		return true;
	}

	return IStructSerializable::ApplyStateFromStruct_Implementation(SavedState);
}

void AARGameStateBase::ServerApplyStateFromStruct_Implementation(const FInstancedStruct& SavedState)
{
	IStructSerializable::ApplyStateFromStruct_Implementation(SavedState);
}
