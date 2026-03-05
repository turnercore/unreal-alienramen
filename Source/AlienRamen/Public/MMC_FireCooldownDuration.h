/**
 * @file MMC_FireCooldownDuration.h
 * @brief MMC FireCooldownDuration header for Alien Ramen.
 */
#pragma once

// MMC_FireCooldownDuration.h

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
