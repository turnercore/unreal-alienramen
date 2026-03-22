#include "ARASCAttributeWidgetBase.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Actor.h"

void UARASCAttributeWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();

	if (bAutoBindOwningPawnOnConstruct && !BoundASC.IsValid())
	{
		TryBindOwningPawn();
	}
}

void UARASCAttributeWidgetBase::NativeDestruct()
{
	DeinitializeASCAttributeWidget();
	Super::NativeDestruct();
}

void UARASCAttributeWidgetBase::InitializeFromASC(UAbilitySystemComponent* InASC)
{
	UnbindSourceActorEndPlay();
	BoundSourceActor.Reset();
	InitializeFromASCInternal(InASC);
}

void UARASCAttributeWidgetBase::InitializeFromActor(AActor* InActor)
{
	if (!InActor)
	{
		DeinitializeASCAttributeWidget();
		return;
	}

	if (BoundSourceActor.Get() != InActor)
	{
		UnbindSourceActorEndPlay();
		BoundSourceActor = InActor;
		BindSourceActorEndPlay();
	}

	UAbilitySystemComponent* ResolvedASC = ResolveAbilitySystemComponentFromActor(InActor);
	if (!ResolvedASC)
	{
		DeinitializeASCAttributeWidget();
		return;
	}

	InitializeFromASCInternal(ResolvedASC);
}

void UARASCAttributeWidgetBase::DeinitializeASCAttributeWidget()
{
	const bool bHadAnyBindingOrState =
		BoundASC.IsValid()
		|| BoundSourceActor.IsValid()
		|| TrackedAttributeRuntimes.Num() > 0
		|| bHasASCAttributeWidgetInitialized;

	UnbindTrackedAttributeDelegates();
	UnbindSourceActorEndPlay();
	BoundASC.Reset();
	BoundSourceActor.Reset();
	TrackedAttributeRuntimes.Reset();
	bHasASCAttributeWidgetInitialized = false;

	if (bHadAnyBindingOrState)
	{
		OnASCAttributeWidgetDeinitialized.Broadcast();
		BP_OnASCAttributeWidgetDeinitialized();
	}
}

bool UARASCAttributeWidgetBase::RefreshResolvedASCBinding()
{
	AActor* SourceActor = BoundSourceActor.Get();
	if (!SourceActor)
	{
		return false;
	}

	UAbilitySystemComponent* ResolvedASC = ResolveAbilitySystemComponentFromActor(SourceActor);
	if (!ResolvedASC)
	{
		DeinitializeASCAttributeWidget();
		return false;
	}

	if (ResolvedASC != BoundASC.Get() || !bHasASCAttributeWidgetInitialized)
	{
		InitializeFromASCInternal(ResolvedASC);
	}
	else
	{
		RefreshTrackedAttributeSnapshot(true);
	}

	return BoundASC.Get() == ResolvedASC;
}

bool UARASCAttributeWidgetBase::TryBindOwningPawn()
{
	AActor* OwningPawn = GetOwningPlayerPawn();
	if (!OwningPawn)
	{
		return false;
	}

	InitializeFromActor(OwningPawn);
	return BoundSourceActor.Get() == OwningPawn && BoundASC.IsValid();
}

bool UARASCAttributeWidgetBase::GetTrackedAttributeValue(const FName AttributeName, float& OutValue) const
{
	for (const FARASCTrackedAttributeRuntime& RuntimeState : TrackedAttributeRuntimes)
	{
		if (RuntimeState.AttributeName == AttributeName && RuntimeState.bHasCachedValue)
		{
			OutValue = RuntimeState.CachedValue;
			return true;
		}
	}

	OutValue = 0.0f;
	return false;
}

void UARASCAttributeWidgetBase::BuildTrackedAttributeDefinitions(TArray<FARASCTrackedAttributeDefinition>& OutDefinitions) const
{
	OutDefinitions.Reset();
}

void UARASCAttributeWidgetBase::HandleTrackedAttributeValueChanged(
	const FARASCTrackedAttributeRuntime& RuntimeState,
	const float NewValue,
	const float OldValue)
{
	(void)RuntimeState;
	(void)NewValue;
	(void)OldValue;
}

void UARASCAttributeWidgetBase::AddTrackedAttributeDefinition(
	TArray<FARASCTrackedAttributeDefinition>& OutDefinitions,
	const FGameplayAttribute& Attribute,
	const FName AttributeName) const
{
	if (!Attribute.IsValid())
	{
		return;
	}

	for (const FARASCTrackedAttributeDefinition& Existing : OutDefinitions)
	{
		if (Existing.Attribute == Attribute)
		{
			return;
		}
	}

	FARASCTrackedAttributeDefinition Definition;
	Definition.Attribute = Attribute;
	Definition.AttributeName = ResolveAttributeName(Attribute, AttributeName);
	OutDefinitions.Add(Definition);
}

void UARASCAttributeWidgetBase::InitializeFromASCInternal(UAbilitySystemComponent* InASC)
{
	if (!InASC)
	{
		DeinitializeASCAttributeWidget();
		return;
	}

	if (BoundASC.Get() == InASC && bHasASCAttributeWidgetInitialized)
	{
		RefreshTrackedAttributeSnapshot(true);
		return;
	}

	UnbindTrackedAttributeDelegates();
	BoundASC = InASC;
	TrackedAttributeRuntimes.Reset();

	TArray<FARASCTrackedAttributeDefinition> Definitions;
	BuildTrackedAttributeDefinitions(Definitions);

	TrackedAttributeRuntimes.Reserve(Definitions.Num());
	for (const FARASCTrackedAttributeDefinition& Definition : Definitions)
	{
		if (!Definition.Attribute.IsValid())
		{
			continue;
		}

		FARASCTrackedAttributeRuntime RuntimeState;
		RuntimeState.Attribute = Definition.Attribute;
		RuntimeState.AttributeName = ResolveAttributeName(Definition.Attribute, Definition.AttributeName);
		TrackedAttributeRuntimes.Add(RuntimeState);
	}

	BindTrackedAttributeDelegates();
	bHasASCAttributeWidgetInitialized = true;
	OnASCAttributeWidgetInitialized.Broadcast(BoundASC.Get());
	BP_OnASCAttributeWidgetInitialized(BoundASC.Get(), BoundSourceActor.Get());
	RefreshTrackedAttributeSnapshot(true);
}

void UARASCAttributeWidgetBase::BindSourceActorEndPlay()
{
	if (AActor* SourceActor = BoundSourceActor.Get())
	{
		SourceActor->OnEndPlay.AddUniqueDynamic(this, &UARASCAttributeWidgetBase::HandleBoundSourceActorEndPlay);
	}
}

void UARASCAttributeWidgetBase::UnbindSourceActorEndPlay()
{
	if (AActor* SourceActor = BoundSourceActor.Get())
	{
		SourceActor->OnEndPlay.RemoveDynamic(this, &UARASCAttributeWidgetBase::HandleBoundSourceActorEndPlay);
	}
}

void UARASCAttributeWidgetBase::BindTrackedAttributeDelegates()
{
	UAbilitySystemComponent* ASC = BoundASC.Get();
	if (!ASC)
	{
		return;
	}

	for (FARASCTrackedAttributeRuntime& RuntimeState : TrackedAttributeRuntimes)
	{
		if (!RuntimeState.Attribute.IsValid() || RuntimeState.DelegateHandle.IsValid())
		{
			continue;
		}

		RuntimeState.DelegateHandle = ASC->GetGameplayAttributeValueChangeDelegate(RuntimeState.Attribute)
			.AddUObject(this, &UARASCAttributeWidgetBase::HandleBoundASCAttributeChanged);
	}
}

void UARASCAttributeWidgetBase::UnbindTrackedAttributeDelegates()
{
	UAbilitySystemComponent* ASC = BoundASC.Get();
	if (!ASC)
	{
		for (FARASCTrackedAttributeRuntime& RuntimeState : TrackedAttributeRuntimes)
		{
			RuntimeState.DelegateHandle.Reset();
		}
		return;
	}

	for (FARASCTrackedAttributeRuntime& RuntimeState : TrackedAttributeRuntimes)
	{
		if (RuntimeState.Attribute.IsValid() && RuntimeState.DelegateHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(RuntimeState.Attribute).Remove(RuntimeState.DelegateHandle);
			RuntimeState.DelegateHandle.Reset();
		}
	}
}

void UARASCAttributeWidgetBase::RefreshTrackedAttributeSnapshot(const bool bBroadcastSnapshotEvents)
{
	UAbilitySystemComponent* ASC = BoundASC.Get();
	if (!ASC)
	{
		return;
	}

	for (FARASCTrackedAttributeRuntime& RuntimeState : TrackedAttributeRuntimes)
	{
		if (!RuntimeState.Attribute.IsValid())
		{
			continue;
		}

		const float NewValue = ASC->GetNumericAttribute(RuntimeState.Attribute);
		const float OldValue = RuntimeState.bHasCachedValue ? RuntimeState.CachedValue : NewValue;
		RuntimeState.CachedValue = NewValue;
		RuntimeState.bHasCachedValue = true;

		if (bBroadcastSnapshotEvents)
		{
			BroadcastTrackedAttributeChange(RuntimeState, NewValue, OldValue);
		}
	}
}

void UARASCAttributeWidgetBase::BroadcastTrackedAttributeChange(
	const FARASCTrackedAttributeRuntime& RuntimeState,
	const float NewValue,
	const float OldValue)
{
	OnASCAttributeWidgetTrackedAttributeChanged.Broadcast(RuntimeState.AttributeName, NewValue, OldValue);
	BP_OnASCAttributeTrackedChanged(RuntimeState.AttributeName, NewValue, OldValue);
	HandleTrackedAttributeValueChanged(RuntimeState, NewValue, OldValue);
}

UAbilitySystemComponent* UARASCAttributeWidgetBase::ResolveAbilitySystemComponentFromActor(AActor* InActor) const
{
	IAbilitySystemInterface* AbilitySystemInterface = Cast<IAbilitySystemInterface>(InActor);
	return AbilitySystemInterface ? AbilitySystemInterface->GetAbilitySystemComponent() : nullptr;
}

int32 UARASCAttributeWidgetBase::FindRuntimeIndexForAttribute(const FGameplayAttribute& Attribute) const
{
	for (int32 Index = 0; Index < TrackedAttributeRuntimes.Num(); ++Index)
	{
		if (TrackedAttributeRuntimes[Index].Attribute == Attribute)
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

FName UARASCAttributeWidgetBase::ResolveAttributeName(const FGameplayAttribute& Attribute, const FName RequestedName) const
{
	if (RequestedName != NAME_None)
	{
		return RequestedName;
	}

	const FString AttributeName = Attribute.GetName();
	return AttributeName.IsEmpty() ? NAME_None : FName(*AttributeName);
}

void UARASCAttributeWidgetBase::HandleBoundASCAttributeChanged(const FOnAttributeChangeData& ChangeData)
{
	const int32 RuntimeIndex = FindRuntimeIndexForAttribute(ChangeData.Attribute);
	if (RuntimeIndex == INDEX_NONE)
	{
		return;
	}

	FARASCTrackedAttributeRuntime& RuntimeState = TrackedAttributeRuntimes[RuntimeIndex];
	const float OldValue = RuntimeState.bHasCachedValue ? RuntimeState.CachedValue : ChangeData.OldValue;
	const float NewValue = ChangeData.NewValue;
	RuntimeState.CachedValue = NewValue;
	RuntimeState.bHasCachedValue = true;
	BroadcastTrackedAttributeChange(RuntimeState, NewValue, OldValue);
}

void UARASCAttributeWidgetBase::HandleBoundSourceActorEndPlay(AActor* InActor, EEndPlayReason::Type EndPlayReason)
{
	(void)EndPlayReason;

	if (BoundSourceActor.Get() == InActor)
	{
		DeinitializeASCAttributeWidget();
	}
}
