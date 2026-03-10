#include "ARRamenBowlActor.h"

#include "Net/UnrealNetwork.h"

AARRamenBowlActor::AARRamenBowlActor()
{
	bReplicates = true;
}

EARRamenStationType AARRamenBowlActor::GetNextRequiredStationType() const
{
	switch (FillStep)
	{
	case 0:
		return EARRamenStationType::Noodles;
	case 1:
		return EARRamenStationType::Broth;
	default:
		return EARRamenStationType::Toppings;
	}
}

bool AARRamenBowlActor::TryApplyFillFromStation(const EARRamenStationType StationType, const EARAffinityColor StationColor)
{
	if (!HasAuthority() || IsComplete() || StationType != GetNextRequiredStationType())
	{
		return false;
	}

	switch (FillStep)
	{
	case 0:
		BowlSpec.NoodlesColor = StationColor;
		break;
	case 1:
		BowlSpec.BrothColor = StationColor;
		break;
	case 2:
		BowlSpec.ToppingsColor = StationColor;
		break;
	default:
		return false;
	}

	++FillStep;
	ForceNetUpdate();
	return true;
}

void AARRamenBowlActor::ClearBowl()
{
	if (!HasAuthority())
	{
		return;
	}

	BowlSpec = FARRamenBowlSpec();
	FillStep = 0;
	ForceNetUpdate();
}

void AARRamenBowlActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AARRamenBowlActor, BowlSpec);
	DOREPLIFETIME(AARRamenBowlActor, FillStep);
}
