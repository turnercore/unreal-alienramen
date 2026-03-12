#include "ARRamenMeatActor.h"

#include "Net/UnrealNetwork.h"

AARRamenMeatActor::AARRamenMeatActor()
{
	bReplicates = true;
}

void AARRamenMeatActor::SetMeatData(const EARAffinityColor NewColor, const int32 NewAmount)
{
	if (!HasAuthority())
	{
		return;
	}

	MeatColor = NewColor;
	MeatAmount = FMath::Max(1, NewAmount);
	ForceNetUpdate();
}

void AARRamenMeatActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AARRamenMeatActor, MeatColor);
	DOREPLIFETIME(AARRamenMeatActor, MeatAmount);
}
