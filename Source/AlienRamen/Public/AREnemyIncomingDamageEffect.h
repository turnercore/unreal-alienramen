#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "AREnemyIncomingDamageEffect.generated.h"

/**
 * Runtime damage effect used to route damage through IncomingDamage meta-attribute.
 */
UCLASS()
class ALIENRAMEN_API UAREnemyIncomingDamageEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UAREnemyIncomingDamageEffect();
};
