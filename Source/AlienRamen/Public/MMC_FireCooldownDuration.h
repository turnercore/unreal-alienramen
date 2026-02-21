// MMC_FireCooldownDuration.h
#pragma once

#include "CoreMinimal.h"
#include "GameplayModMagnitudeCalculation.h"
#include "MMC_FireCooldownDuration.generated.h"

UCLASS()
class ALIENRAMEN_API UMMC_FireCooldownDuration : public UGameplayModMagnitudeCalculation
{
	GENERATED_BODY()

public:
	UMMC_FireCooldownDuration();

	virtual float CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const override;

private:
	FGameplayEffectAttributeCaptureDefinition FireRateDef;
};