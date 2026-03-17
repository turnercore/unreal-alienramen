#include "ARRamenBowlActor.h"

#include "ARLog.h"
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

bool AARRamenBowlActor::TryApplyFillFromStation(const EARRamenStationType StationType, const EARAffinityColor StationColor, const FGameplayTag StationMeatTag)
{
	if (!HasAuthority() || IsComplete() || StationType != GetNextRequiredStationType())
	{
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[Shop|Bowl] Fill rejected bowl='%s' authority=%d complete=%d station='%s' required='%s'."),
			*GetNameSafe(this),
			HasAuthority() ? 1 : 0,
			IsComplete() ? 1 : 0,
			*StaticEnum<EARRamenStationType>()->GetValueAsString(StationType),
			*StaticEnum<EARRamenStationType>()->GetValueAsString(GetNextRequiredStationType()));
		return false;
	}

	switch (FillStep)
	{
	case 0:
		BowlSpec.NoodlesColor = StationColor;
		BowlSpec.NoodlesMeatTag = StationMeatTag;
		break;
	case 1:
		BowlSpec.BrothColor = StationColor;
		BowlSpec.BrothMeatTag = StationMeatTag;
		break;
	case 2:
		BowlSpec.ToppingsColor = StationColor;
		BowlSpec.ToppingsMeatTag = StationMeatTag;
		break;
	default:
		return false;
	}

	++FillStep;
	ForceNetUpdate();
	UE_LOG(
		ARLog,
		Verbose,
		TEXT("[Shop|Bowl] Fill applied bowl='%s' step=%d station='%s' color=%d complete=%d."),
		*GetNameSafe(this),
		FillStep,
		*StaticEnum<EARRamenStationType>()->GetValueAsString(StationType),
		static_cast<int32>(StationColor),
		IsComplete() ? 1 : 0);
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
	UE_LOG(ARLog, Verbose, TEXT("[Shop|Bowl] Cleared bowl '%s'."), *GetNameSafe(this));
}

void AARRamenBowlActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AARRamenBowlActor, BowlSpec);
	DOREPLIFETIME(AARRamenBowlActor, FillStep);
}
