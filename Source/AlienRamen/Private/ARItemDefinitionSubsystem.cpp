#include "ARItemDefinitionSubsystem.h"

#include "ARLog.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/GameInstance.h"
#include "StructUtils/InstancedStruct.h"
#include "TagContentResolverSubsystem.h"

bool UARItemDefinitionSubsystem::ResolveItemDefinition(const FGameplayTag ItemTag, FARScrapyardItemDefRow& OutItemDef) const
{
	return ResolveItemDefinition_Internal(ItemTag, OutItemDef);
}

bool UARItemDefinitionSubsystem::ResolveEnergyDrinkDefinition(
	const FGameplayTag ItemTag,
	FAREnergyDrinkDefRow& OutEnergyDrinkDef) const
{
	OutEnergyDrinkDef = FAREnergyDrinkDefRow();
	if (!ItemTag.IsValid())
	{
		return false;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UTagContentResolverSubsystem* Resolver = GameInstance ? GameInstance->GetSubsystem<UTagContentResolverSubsystem>() : nullptr;

	auto TryResolveDirect = [Resolver](const FGameplayTag TagToResolve, FAREnergyDrinkDefRow& OutDef) -> bool
	{
		if (!Resolver || !TagToResolve.IsValid())
		{
			return false;
		}

		FInstancedStruct RowData;
		FString ResolveError;
		if (!Resolver->TryResolveRowForTag(TagToResolve, RowData, ResolveError))
		{
			return false;
		}

		const FAREnergyDrinkDefRow* TypedDef = RowData.GetPtr<FAREnergyDrinkDefRow>();
		if (!TypedDef)
		{
			return false;
		}

		OutDef = *TypedDef;
		return true;
	};

	if (TryResolveDirect(ItemTag, OutEnergyDrinkDef))
	{
		return true;
	}

	FARScrapyardItemDefRow ItemDef;
	if (!ResolveItemDefinition(ItemTag, ItemDef))
	{
		return false;
	}

	const FGameplayTag LinkedEnergyDrinkTag = ItemDef.EnergyDrinkTag.IsValid() ? ItemDef.EnergyDrinkTag : ItemTag;
	if (TryResolveDirect(LinkedEnergyDrinkTag, OutEnergyDrinkDef))
	{
		return true;
	}

	if (ItemDef.RunBuffGameplayEffects.IsEmpty() && ItemDef.RunBuffGrantedTags.IsEmpty())
	{
		return false;
	}

	OutEnergyDrinkDef.EnergyDrinkTag = LinkedEnergyDrinkTag;
	OutEnergyDrinkDef.RunBuffGameplayEffects = ItemDef.RunBuffGameplayEffects;
	OutEnergyDrinkDef.RunBuffGrantedTags = ItemDef.RunBuffGrantedTags;
	OutEnergyDrinkDef.StackRule = ItemDef.StackRule;
	OutEnergyDrinkDef.MaxStackCount = FMath::Max(1, ItemDef.MaxStackCount);
	return true;
}

bool UARItemDefinitionSubsystem::ResolveItemActorClass(const FGameplayTag ItemTag, TSubclassOf<AActor>& OutActorClass) const
{
	OutActorClass = nullptr;

	FARScrapyardItemDefRow ItemDef;
	if (!ResolveItemDefinition(ItemTag, ItemDef))
	{
		return false;
	}

	if (ItemDef.ItemModelClass.IsNull())
	{
		return false;
	}

	UClass* ResolvedClass = ItemDef.ItemModelClass.LoadSynchronous();
	if (!ResolvedClass || !ResolvedClass->IsChildOf(AActor::StaticClass()))
	{
		return false;
	}

	OutActorClass = ResolvedClass;
	return true;
}

bool UARItemDefinitionSubsystem::ApplyItemPhysicsProperties(AActor* Actor, const FGameplayTag ItemTag) const
{
	if (!Actor || !ItemTag.IsValid())
	{
		return false;
	}

	FARScrapyardItemDefRow ItemDef;
	if (!ResolveItemDefinition(ItemTag, ItemDef))
	{
		return false;
	}

	const float SanitizedWeight = FMath::Max(0.0f, ItemDef.Weight);
	if (SanitizedWeight <= 0.0f)
	{
		return false;
	}

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	Actor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
	if (PrimitiveComponents.IsEmpty())
	{
		return false;
	}

	UPrimitiveComponent* TargetPrimitive = Cast<UPrimitiveComponent>(Actor->GetRootComponent());
	if (!TargetPrimitive)
	{
		for (UPrimitiveComponent* Primitive : PrimitiveComponents)
		{
			if (Primitive && Primitive->IsSimulatingPhysics())
			{
				TargetPrimitive = Primitive;
				break;
			}
		}
	}

	if (!TargetPrimitive)
	{
		for (UPrimitiveComponent* Primitive : PrimitiveComponents)
		{
			if (Primitive && Primitive->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
			{
				TargetPrimitive = Primitive;
				break;
			}
		}
	}

	if (!TargetPrimitive)
	{
		TargetPrimitive = PrimitiveComponents[0];
	}

	TargetPrimitive->SetMassOverrideInKg(NAME_None, SanitizedWeight, true);
	if (TargetPrimitive->IsSimulatingPhysics())
	{
		TargetPrimitive->WakeAllRigidBodies();
	}
	return true;
}

bool UARItemDefinitionSubsystem::ResolveItemDefinition_Internal(
	const FGameplayTag ItemTag,
	FARScrapyardItemDefRow& OutItemDef) const
{
	OutItemDef = FARScrapyardItemDefRow();
	if (!ItemTag.IsValid())
	{
		return false;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UTagContentResolverSubsystem* Resolver = GameInstance ? GameInstance->GetSubsystem<UTagContentResolverSubsystem>() : nullptr;
	if (!Resolver)
	{
		return false;
	}

	FInstancedStruct RowData;
	FString ResolveError;
	if (!Resolver->TryResolveRowForTag(ItemTag, RowData, ResolveError))
	{
		UE_LOG(
			ARLog,
			Warning,
			TEXT("[Items] Failed resolving item row for '%s': %s"),
			*ItemTag.ToString(),
			*ResolveError);
		return false;
	}

	const FARScrapyardItemDefRow* TypedDef = RowData.GetPtr<FARScrapyardItemDefRow>();
	if (!TypedDef)
	{
		UE_LOG(
			ARLog,
			Warning,
			TEXT("[Items] Resolved row for '%s' was not FARScrapyardItemDefRow."),
			*ItemTag.ToString());
		return false;
	}

	OutItemDef = *TypedDef;
	if (!OutItemDef.ItemTag.IsValid())
	{
		OutItemDef.ItemTag = ItemTag;
	}
	ApplyKnowledgeTextFallback(ItemTag, OutItemDef);
	return true;
}

void UARItemDefinitionSubsystem::ApplyKnowledgeTextFallback(
	const FGameplayTag ItemTag,
	FARScrapyardItemDefRow& InOutDef)
{
	const bool bKnowledgeGated = !InOutDef.RequiredCharacterKnowledgeTags.IsEmpty() || !InOutDef.RequiredKnowledgeTags.IsEmpty();
	if (!bKnowledgeGated)
	{
		return;
	}

	if (InOutDef.AltDisplayName.IsEmpty())
	{
		UE_LOG(
			ARLog,
			Warning,
			TEXT("[Items] Item '%s' is knowledge-gated but AltDisplayName is missing. Falling back to DisplayName."),
			*ItemTag.ToString());
		InOutDef.AltDisplayName = InOutDef.DisplayName;
	}

	if (InOutDef.AltDescription.IsEmpty())
	{
		UE_LOG(
			ARLog,
			Warning,
			TEXT("[Items] Item '%s' is knowledge-gated but AltDescription is missing. Falling back to Description."),
			*ItemTag.ToString());
		InOutDef.AltDescription = InOutDef.Description;
	}
}
