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
		return EARRamenStationType::Broth;
	case 1:
		return EARRamenStationType::Noodles;
	default:
		return EARRamenStationType::Toppings;
	}
}

bool AARRamenBowlActor::TryApplyFillFromStation(
	const EARRamenStationType StationType,
	const EARAffinityColor StationColor,
	const FGameplayTag StationMeatTag,
	const EARVendingQualityTier StationQualityTier)
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

	const int32 PreviousFillStep = FillStep;
	FARRamenBowlSlotSpec* SlotToFill = nullptr;
	switch (FillStep)
	{
	case 0:
		SlotToFill = &BowlSpec.Broth;
		break;
	case 1:
		SlotToFill = &BowlSpec.Noodles;
		break;
	case 2:
		SlotToFill = &BowlSpec.Toppings;
		break;
	default:
		return false;
	}
	if (!SlotToFill)
	{
		return false;
	}

	SlotToFill->SlotType = StationType;
	SlotToFill->Color = StationColor;
	SlotToFill->MeatTag = StationMeatTag;
	SlotToFill->QualityTier = StationQualityTier;

	++FillStep;
	BroadcastFillStepChanged(PreviousFillStep, FillStep);
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

	const int32 PreviousFillStep = FillStep;
	BowlSpec = FARRamenBowlSpec();
	FillStep = 0;
	BroadcastFillStepChanged(PreviousFillStep, FillStep);
	ForceNetUpdate();
	UE_LOG(ARLog, Verbose, TEXT("[Shop|Bowl] Cleared bowl '%s'."), *GetNameSafe(this));
}

void AARRamenBowlActor::OnRep_FillStep(const int32 PreviousFillStep)
{
	BroadcastFillStepChanged(PreviousFillStep, FillStep);
}

void AARRamenBowlActor::BroadcastFillStepChanged(const int32 PreviousFillStep, const int32 NewFillStep)
{
	if (PreviousFillStep == NewFillStep)
	{
		return;
	}

	OnFillStepChanged.Broadcast(PreviousFillStep, NewFillStep);
}

void AARRamenBowlActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AARRamenBowlActor, BowlSpec);
	DOREPLIFETIME(AARRamenBowlActor, FillStep);
}
