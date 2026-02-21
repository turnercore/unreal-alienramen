// MMC_FireCooldownDuration.cpp
#include "MMC_FireCooldownDuration.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "ARAttributeSetCore.h" // <-- fix include path to your AttributeSet header

UMMC_FireCooldownDuration::UMMC_FireCooldownDuration()
{
	// Capture FireRate from the Source (the shooter)
	FireRateDef = FGameplayEffectAttributeCaptureDefinition(
		UARAttributeSetCore::GetFireRateAttribute(),
		EGameplayEffectAttributeCaptureSource::Source,
		/*bSnapshot*/ false
	);

	RelevantAttributesToCapture.Add(FireRateDef);
}

float UMMC_FireCooldownDuration::CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const
{
	FAggregatorEvaluateParameters Params;
	Params.SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	Params.TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

	float FireRate = 0.f;
	GetCapturedAttributeMagnitude(FireRateDef, Spec, Params, FireRate);

	// Shots per second -> seconds per shot
	const float SafeRate = FMath::Max(0.01f, FireRate);
	const float CooldownSeconds = 1.f / SafeRate;

	return CooldownSeconds;
}