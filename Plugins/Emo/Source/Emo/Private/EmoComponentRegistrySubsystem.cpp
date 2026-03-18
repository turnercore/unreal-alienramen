#include "EmoComponentRegistrySubsystem.h"

#include "EmoComponent.h"

void UEmoComponentRegistrySubsystem::RegisterEmotionComponent(UEmoComponent* EmotionComponent)
{
	if (IsValid(EmotionComponent))
	{
		RegisteredEmotionComponents.Add(EmotionComponent);
	}
}

void UEmoComponentRegistrySubsystem::UnregisterEmotionComponent(UEmoComponent* EmotionComponent)
{
	if (EmotionComponent)
	{
		RegisteredEmotionComponents.Remove(EmotionComponent);
	}
}

void UEmoComponentRegistrySubsystem::GetRegisteredEmotionComponents(TArray<TWeakObjectPtr<UEmoComponent>>& OutComponents) const
{
	OutComponents.Reset();
	OutComponents.Reserve(RegisteredEmotionComponents.Num());

	for (const TWeakObjectPtr<UEmoComponent>& ComponentPtr : RegisteredEmotionComponents)
	{
		if (ComponentPtr.IsValid())
		{
			OutComponents.Add(ComponentPtr);
		}
	}
}
