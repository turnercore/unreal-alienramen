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
	static FGameplayTag ResolveMeatRootTag()
	{
		return FGameplayTag::RequestGameplayTag(TEXT("Item.Meat"), false);
	}

	static FGameplayTag ResolveItemRootTag()
	{
		return FGameplayTag::RequestGameplayTag(TEXT("Item"), false);
	}

	static FGameplayTag ResolveLegacyMeatTagForColor(const EARAffinityColor Color)
	{
		switch (Color)
		{
		case EARAffinityColor::Red:
			return FGameplayTag::RequestGameplayTag(TEXT("Item.Meat.Red"), false);
		case EARAffinityColor::Blue:
			return FGameplayTag::RequestGameplayTag(TEXT("Item.Meat.Blue"), false);
		case EARAffinityColor::White:
			return FGameplayTag::RequestGameplayTag(TEXT("Item.Meat.White"), false);
		case EARAffinityColor::Colorless:
			return FGameplayTag::RequestGameplayTag(TEXT("Item.Meat.Colorless"), false);
		case EARAffinityColor::None:
		default:
			return FGameplayTag::RequestGameplayTag(TEXT("Item.Meat.None"), false);
		}
	}

	static FName ExtractLeafRowNameFromTag(const FGameplayTag Tag)
	{
		if (!Tag.IsValid())
		{
			return NAME_None;
		}

		const FString TagString = Tag.ToString();
		int32 LastDotIndex = INDEX_NONE;
		if (TagString.FindLastChar(TEXT('.'), LastDotIndex) && LastDotIndex >= 0 && LastDotIndex < TagString.Len() - 1)
		{
			return FName(*TagString.Mid(LastDotIndex + 1));
		}

		return FName(*TagString);
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

bool UARItemDefinitionSubsystem::ResolveFirstMeatDefinition(FARMeatDefinitionRow& OutMeatDef) const
{
	OutMeatDef = FARMeatDefinitionRow();

	TArray<TPair<FName, FARMeatDefinitionRow>> Definitions;
	if (!GatherAllMeatDefinitions(Definitions))
	{
		return false;
	}

	for (const TPair<FName, FARMeatDefinitionRow>& Entry : Definitions)
	{
		const FARMeatDefinitionRow& Row = Entry.Value;
		if (!Row.MeatTag.IsValid())
		{
			continue;
		}

		OutMeatDef = Row;
		return OutMeatDef.MeatTag.IsValid();
	}

	return false;
}

bool UARItemDefinitionSubsystem::ResolveMeatDefinitionForEnemy(const FGameplayTag EnemyIdentifierTag, FARMeatDefinitionRow& OutMeatDef) const
{
	OutMeatDef = FARMeatDefinitionRow();
	if (!EnemyIdentifierTag.IsValid())
	{
		return false;
	}

	TArray<TPair<FName, FARMeatDefinitionRow>> Definitions;
	if (!GatherAllMeatDefinitions(Definitions))
	{
		return false;
	}

	for (const TPair<FName, FARMeatDefinitionRow>& Entry : Definitions)
	{
		const FARMeatDefinitionRow& Row = Entry.Value;
		if (!Row.MeatTag.IsValid() || !Row.EnemyIdentifierTag.IsValid())
		{
			continue;
		}

		if (!Row.EnemyIdentifierTag.MatchesTagExact(EnemyIdentifierTag))
		{
			continue;
		}

		OutMeatDef = Row;
		return true;
	}

	return false;
}

bool UARItemDefinitionSubsystem::ResolveFirstMeatDefinitionForColor(const EARAffinityColor Color, FARMeatDefinitionRow& OutMeatDef) const
{
	OutMeatDef = FARMeatDefinitionRow();

	const FGameplayTag LegacyColorTag = ResolveLegacyMeatTagForColor(Color);
	if (LegacyColorTag.IsValid() && ResolveMeatDefinition(LegacyColorTag, OutMeatDef))
	{
		return true;
	}

	return ResolveFirstMeatDefinition(OutMeatDef);
}

bool UARItemDefinitionSubsystem::ResolveFirstMeatTag(FGameplayTag& OutMeatTag) const
{
	OutMeatTag = FGameplayTag();

	FARMeatDefinitionRow MeatDef;
	if (!ResolveFirstMeatDefinition(MeatDef))
	{
		return false;
	}

	OutMeatTag = MeatDef.MeatTag;
	return OutMeatTag.IsValid();
}

bool UARItemDefinitionSubsystem::ResolveFirstMeatTagForColor(const EARAffinityColor Color, FGameplayTag& OutMeatTag) const
{
	OutMeatTag = FGameplayTag();

	FARMeatDefinitionRow MeatDef;
	if (ResolveFirstMeatDefinitionForColor(Color, MeatDef) && MeatDef.MeatTag.IsValid())
	{
		OutMeatTag = MeatDef.MeatTag;
		return true;
	}

	return false;
}

bool UARItemDefinitionSubsystem::GetMeatTagsForColor(const EARAffinityColor Color, TArray<FGameplayTag>& OutMeatTags) const
{
	OutMeatTags.Reset();

	FGameplayTag MeatTag;
	if (!ResolveFirstMeatTagForColor(Color, MeatTag) || !MeatTag.IsValid())
	{
		return false;
	}

	OutMeatTags.Add(MeatTag);
	return true;
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
	for (int32 SlotIndex = 0; SlotIndex < UE_ARRAY_COUNT(SlotTags); ++SlotIndex)
	{
		FARMeatDefinitionRow MeatDef;
		const FGameplayTag SlotMeatTag = SlotTags[SlotIndex];
		if (!ResolveMeatDefinition(SlotMeatTag, MeatDef))
		{
			continue;
		}

		if (!MeatDef.MeatTag.IsValid())
		{
			continue;
		}

		FARScrapyardItemDefRow ItemDef;
		if (!ResolveItemDefinition(MeatDef.MeatTag, ItemDef))
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

	auto TryResolveViaItemRootRowName = [this, Resolver, ItemTag, &OutItemDef]() -> bool
	{
		const FGameplayTag ItemRootTag = ResolveItemRootTag();
		const FGameplayTag MeatRootTag = ResolveMeatRootTag();
		if (!ItemRootTag.IsValid() || !MeatRootTag.IsValid() || !ItemTag.MatchesTag(MeatRootTag))
		{
			return false;
		}

		UDataTable* ItemDataTable = nullptr;
		FString ItemTableResolveError;
		if (!Resolver->TryResolveDataTableForRootTag(ItemRootTag, ItemDataTable, ItemTableResolveError) || !ItemDataTable)
		{
			return false;
		}

		const FName ItemRowName = ExtractLeafRowNameFromTag(ItemTag);
		if (ItemRowName.IsNone())
		{
			return false;
		}

		const FARScrapyardItemDefRow* ItemRow = ItemDataTable->FindRow<FARScrapyardItemDefRow>(ItemRowName, TEXT("ResolveItemDefinition_ItemRootFallback"), false);
		if (!ItemRow)
		{
			return false;
		}

		OutItemDef = *ItemRow;
		if (!OutItemDef.ItemTag.IsValid())
		{
			OutItemDef.ItemTag = ItemTag;
		}
		ApplyKnowledgeTextFallback(ItemTag, OutItemDef);
		return true;
	};

	FInstancedStruct RowData;
	FString ResolveError;
	if (!Resolver->TryResolveRowStructForTag(ItemTag, RowData, ResolveError))
	{
		if (TryResolveViaItemRootRowName())
		{
			return true;
		}

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
		if (TryResolveViaItemRootRowName())
		{
			return true;
		}

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
