/**
 * @file EmoComponentRegistrySubsystem.h
 * @brief World-scoped registry of active emotion components for HUD projection.
 */
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "EmoComponentRegistrySubsystem.generated.h"

class UEmoComponent;

UCLASS()
class EMO_API UEmoComponentRegistrySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void RegisterEmotionComponent(UEmoComponent* EmotionComponent);
	void UnregisterEmotionComponent(UEmoComponent* EmotionComponent);
	void GetRegisteredEmotionComponents(TArray<TWeakObjectPtr<UEmoComponent>>& OutComponents) const;

private:
	TSet<TWeakObjectPtr<UEmoComponent>> RegisteredEmotionComponents;
};
