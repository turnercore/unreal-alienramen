#include "ARModeMapPathUtility.h"

FString ARModeMapPath::ResolveDefaultMapPathForModeTag(const FGameplayTag& ModeTag)
{
	if (!ModeTag.IsValid())
	{
		return FString();
	}

	const FGameplayTag LobbyModeTag = FGameplayTag::RequestGameplayTag(TEXT("Mode.Lobby"), false);
	if (LobbyModeTag.IsValid() && ModeTag.MatchesTagExact(LobbyModeTag))
	{
		return TEXT("/Game/Maps/Lvl_MultiplayerLobby");
	}

	const FGameplayTag ShopModeTag = FGameplayTag::RequestGameplayTag(TEXT("Mode.Shop"), false);
	if (ShopModeTag.IsValid() && ModeTag.MatchesTagExact(ShopModeTag))
	{
		return TEXT("/Game/Maps/Lvl_RamenShop");
	}

	const FGameplayTag InvaderModeTag = FGameplayTag::RequestGameplayTag(TEXT("Mode.Invader"), false);
	if (InvaderModeTag.IsValid() && ModeTag.MatchesTagExact(InvaderModeTag))
	{
		return TEXT("/Game/Maps/Lvl_Invader");
	}

	const FGameplayTag ScrapyardModeTag = FGameplayTag::RequestGameplayTag(TEXT("Mode.Scrapyard"), false);
	if (ScrapyardModeTag.IsValid() && ModeTag.MatchesTagExact(ScrapyardModeTag))
	{
		return TEXT("/Game/Maps/Lvl_Scrapyard");
	}

	const FGameplayTag TransitionModeTag = FGameplayTag::RequestGameplayTag(TEXT("Mode.Transition"), false);
	if (TransitionModeTag.IsValid() && ModeTag.MatchesTagExact(TransitionModeTag))
	{
		return TEXT("/Game/Maps/Lvl_Loading");
	}

	return FString();
}
