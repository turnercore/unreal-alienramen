#include "ARScrapyardSpawnRules.h"

FARScrapyardRarityBudget FARScrapyardSpawnRules::GetBudgetForRarity(const EARScrapyardItemRarity Rarity) const
{
	if (const FARScrapyardRarityBudget* Found = RarityBudgets.Find(Rarity))
	{
		return *Found;
	}

	return FARScrapyardRarityBudget();
}

UARScrapyardSpawnRuleSet::UARScrapyardSpawnRuleSet()
{
	// Provide sensible defaults so designers can duplicate and tweak.
	Rules.RarityBudgets.Add(EARScrapyardItemRarity::Common, { 3, 12 });
	Rules.RarityBudgets.Add(EARScrapyardItemRarity::Uncommon, { 2, 8 });
	Rules.RarityBudgets.Add(EARScrapyardItemRarity::Rare, { 1, 5 });
	Rules.RarityBudgets.Add(EARScrapyardItemRarity::Epic, { 0, 3 });
	Rules.RarityBudgets.Add(EARScrapyardItemRarity::Legendary, { 0, 1 });
}

