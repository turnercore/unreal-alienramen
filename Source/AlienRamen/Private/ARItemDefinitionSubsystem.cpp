#include "ARItemDefinitionSubsystem.h"

#include "ARLog.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "GameplayTagsManager.h"
#include "StructUtils/InstancedStruct.h"
#include "TagKeySubsystem.h"

namespace
{
	static EARAffinityColor SanitizeAffinityColor(const EARAffinityColor InColor)
	{
		return InColor == EARAffinityColor::Unknown ? EARAffinityColor::None : InColor;
	}

	static FGameplayTag ResolveMeatRootTag()
	{
		return FGameplayTag::RequestGameplayTag(TEXT("Item.Meat"), false);
	}
}

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
	UTagKeySubsystem* Resolver = GameInstance ? GameInstance->GetSubsystem<UTagKeySubsystem>() : nullptr;

	auto TryResolveDirect = [Resolver](const FGameplayTag TagToResolve, FAREnergyDrinkDefRow& OutDef) -> bool
	{
		if (!Resolver || !TagToResolve.IsValid())
		{
			return false;
		}

		FInstancedStruct RowData;
		FString ResolveError;
		if (!Resolver->TryResolveRowStructForTag(TagToResolve, RowData, ResolveError))
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

bool UARItemDefinitionSubsystem::ResolveMeatDefinition(const FGameplayTag MeatTag, FARMeatDefinitionRow& OutMeatDef) const
{
	return ResolveMeatDefinition_Internal(MeatTag, OutMeatDef);
}

bool UARItemDefinitionSubsystem::ResolveFirstMeatDefinitionForColor(const EARAffinityColor Color, FARMeatDefinitionRow& OutMeatDef) const
{
	OutMeatDef = FARMeatDefinitionRow();

	TArray<TPair<FName, FARMeatDefinitionRow>> Definitions;
	if (!GatherAllMeatDefinitions(Definitions))
	{
		return false;
	}

	const EARAffinityColor SanitizedColor = SanitizeAffinityColor(Color);
	for (const TPair<FName, FARMeatDefinitionRow>& Entry : Definitions)
	{
		const FARMeatDefinitionRow& Row = Entry.Value;
		if (SanitizeAffinityColor(Row.Color) != SanitizedColor)
		{
			continue;
		}

		OutMeatDef = Row;
		return OutMeatDef.MeatTag.IsValid();
	}

	return false;
}

bool UARItemDefinitionSubsystem::ResolveFirstMeatTagForColor(const EARAffinityColor Color, FGameplayTag& OutMeatTag) const
{
	OutMeatTag = FGameplayTag();

	FARMeatDefinitionRow MeatDef;
	if (!ResolveFirstMeatDefinitionForColor(Color, MeatDef))
	{
		return false;
	}

	OutMeatTag = MeatDef.MeatTag;
	return OutMeatTag.IsValid();
}

bool UARItemDefinitionSubsystem::GetMeatTagsForColor(const EARAffinityColor Color, TArray<FGameplayTag>& OutMeatTags) const
{
	OutMeatTags.Reset();

	TArray<TPair<FName, FARMeatDefinitionRow>> Definitions;
	if (!GatherAllMeatDefinitions(Definitions))
	{
		return false;
	}

	const EARAffinityColor SanitizedColor = SanitizeAffinityColor(Color);
	for (const TPair<FName, FARMeatDefinitionRow>& Entry : Definitions)
	{
		const FARMeatDefinitionRow& Row = Entry.Value;
		if (SanitizeAffinityColor(Row.Color) == SanitizedColor && Row.MeatTag.IsValid())
		{
			OutMeatTags.Add(Row.MeatTag);
		}
	}

	return OutMeatTags.Num() > 0;
}

int32 UARItemDefinitionSubsystem::ResolveCombinedMeatItemValue(const FARRamenBowlSpec& BowlSpec) const
{
	int32 TotalValue = 0;

	const FGameplayTag SlotTags[3] =
	{
		BowlSpec.NoodlesMeatTag,
		BowlSpec.BrothMeatTag,
		BowlSpec.ToppingsMeatTag
	};
	const EARAffinityColor SlotColors[3] =
	{
		BowlSpec.NoodlesColor,
		BowlSpec.BrothColor,
		BowlSpec.ToppingsColor
	};

	for (int32 SlotIndex = 0; SlotIndex < UE_ARRAY_COUNT(SlotTags); ++SlotIndex)
	{
		FARMeatDefinitionRow MeatDef;
		const FGameplayTag SlotMeatTag = SlotTags[SlotIndex];
		const EARAffinityColor SlotColor = SanitizeAffinityColor(SlotColors[SlotIndex]);
		if (!ResolveMeatDefinition(SlotMeatTag, MeatDef))
		{
			// Compatibility fallback for older bowls authored before meat-slot tags existed.
			// Do not infer value from pure None slots (empty processing should remain value-free).
			if (SlotColor == EARAffinityColor::None)
			{
				continue;
			}

			if (!ResolveFirstMeatDefinitionForColor(SlotColor, MeatDef))
			{
				continue;
			}
		}

		if (!MeatDef.ItemTag.IsValid())
		{
			continue;
		}

		FARScrapyardItemDefRow ItemDef;
		if (!ResolveItemDefinition(MeatDef.ItemTag, ItemDef))
		{
			continue;
		}

		TotalValue += FMath::Max(0, ItemDef.SellMoneyValue);
	}

	return TotalValue;
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
	UTagKeySubsystem* Resolver = GameInstance ? GameInstance->GetSubsystem<UTagKeySubsystem>() : nullptr;
	if (!Resolver)
	{
		return false;
	}

	FInstancedStruct RowData;
	FString ResolveError;
	if (!Resolver->TryResolveRowStructForTag(ItemTag, RowData, ResolveError))
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

bool UARItemDefinitionSubsystem::ResolveMeatDefinition_Internal(const FGameplayTag MeatTag, FARMeatDefinitionRow& OutMeatDef) const
{
	OutMeatDef = FARMeatDefinitionRow();
	if (!MeatTag.IsValid())
	{
		return false;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UTagKeySubsystem* Resolver = GameInstance ? GameInstance->GetSubsystem<UTagKeySubsystem>() : nullptr;
	if (!Resolver)
	{
		return false;
	}

	FInstancedStruct RowData;
	FString ResolveError;
	if (!Resolver->TryResolveRowStructForTag(MeatTag, RowData, ResolveError))
	{
		return false;
	}

	const FARMeatDefinitionRow* TypedDef = RowData.GetPtr<FARMeatDefinitionRow>();
	if (!TypedDef)
	{
		UE_LOG(
			ARLog,
			Warning,
			TEXT("[Items] Resolved row for '%s' was not FARMeatDefinitionRow."),
			*MeatTag.ToString());
		return false;
	}

	OutMeatDef = *TypedDef;
	if (!OutMeatDef.MeatTag.IsValid())
	{
		OutMeatDef.MeatTag = MeatTag;
	}
	OutMeatDef.Color = SanitizeAffinityColor(OutMeatDef.Color);
	OutMeatDef.StationFillAmount = FMath::Max(1, OutMeatDef.StationFillAmount);
	return true;
}

bool UARItemDefinitionSubsystem::GatherAllMeatDefinitions(TArray<TPair<FName, FARMeatDefinitionRow>>& OutDefinitions) const
{
	OutDefinitions.Reset();

	UGameInstance* GameInstance = GetGameInstance();
	UTagKeySubsystem* Resolver = GameInstance ? GameInstance->GetSubsystem<UTagKeySubsystem>() : nullptr;
	const FGameplayTag MeatRootTag = ResolveMeatRootTag();
	if (!Resolver || !MeatRootTag.IsValid())
	{
		return false;
	}

	TArray<FName> RowNames;
	FString Error;
	if (!Resolver->TryGetRowNamesForRootTag(MeatRootTag, RowNames, Error))
	{
		return false;
	}

	UDataTable* DataTable = nullptr;
	if (!Resolver->TryResolveDataTableForRootTag(MeatRootTag, DataTable, Error) || !DataTable)
	{
		return false;
	}

	RowNames.Sort([](const FName A, const FName B)
	{
		return A.LexicalLess(B);
	});

	OutDefinitions.Reserve(RowNames.Num());
	for (const FName RowName : RowNames)
	{
		const FARMeatDefinitionRow* TypedRow = DataTable->FindRow<FARMeatDefinitionRow>(RowName, TEXT("GatherAllMeatDefinitions"), false);
		if (!TypedRow)
		{
			continue;
		}

		FARMeatDefinitionRow CopiedRow = *TypedRow;
		if (!CopiedRow.MeatTag.IsValid())
		{
			const FString FullTagName = FString::Printf(TEXT("%s.%s"), *MeatRootTag.ToString(), *RowName.ToString());
			CopiedRow.MeatTag = UGameplayTagsManager::Get().RequestGameplayTag(FName(*FullTagName), false);
		}
		CopiedRow.Color = SanitizeAffinityColor(CopiedRow.Color);
		CopiedRow.StationFillAmount = FMath::Max(1, CopiedRow.StationFillAmount);
		OutDefinitions.Add(TPair<FName, FARMeatDefinitionRow>(RowName, CopiedRow));
	}

	return OutDefinitions.Num() > 0;
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
