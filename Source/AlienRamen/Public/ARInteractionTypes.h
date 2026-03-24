/**
 * @file ARInteractionTypes.h
 * @brief Shared interaction cue and interaction-context types.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARSaveTypes.h"
#include "GameplayTagContainer.h"
#include "ARInteractionTypes.generated.h"

/** Lightweight interaction action cue stream used by local animation and UI listeners. */
UENUM(BlueprintType)
enum class EARInteractionActionCue : uint8
{
	None UMETA(DisplayName = "None"),
	Throw UMETA(DisplayName = "Throw"),
	Consume UMETA(DisplayName = "Consume"),
	Kick UMETA(DisplayName = "Kick"),
	Slap UMETA(DisplayName = "Slap"),
	Custom UMETA(DisplayName = "Custom")
};

/**
 * Lightweight economy snapshot exposed alongside interaction context.
 *
 * This intentionally excludes full inventory payloads. It only carries the
 * simple currency/meat state that routing systems such as dialogue may need
 * during the current interaction.
 */
USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARInteractionEconomySnapshot
{
	GENERATED_BODY()

	/** Current world money total. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Interaction", meta = (ToolTip = "Current world money total at the time this interaction context was built."))
	int32 Money = 0;

	/** Current world scrap total. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Interaction", meta = (ToolTip = "Current world scrap total at the time this interaction context was built."))
	int32 Scrap = 0;

	/** Current persistent meat inventory snapshot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Interaction", meta = (ToolTip = "Current persistent meat inventory snapshot at the time this interaction context was built."))
	FARMeatState Meat;

	/** Current run-only scrap ledger. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Interaction", meta = (ToolTip = "Current run-only scrap ledger value at the time this interaction context was built."))
	int32 RunLedgerScrap = 0;

	/** Current run-only meat ledger. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Interaction", meta = (ToolTip = "Current run-only meat ledger snapshot at the time this interaction context was built."))
	FARMeatState RunLedgerMeat;
};

/**
 * Current interaction identity assembled for systems that need the whole picture.
 *
 * This is built on demand from the live owner objects so runtime-only data such
 * as ASC tags, speaker tags, and transient interaction tags cannot drift stale.
 */
USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARInteractionContext
{
	GENERATED_BODY()

	/** Save-wide progression currently owned by the authoritative GameState. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Interaction", meta = (ToolTip = "Save-wide progression currently owned by the authoritative GameState."))
	FGameplayTagContainer GameProgressionTags;

	/** Player-identity progression currently owned by the active PlayerState. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Interaction", meta = (ToolTip = "Player-identity progression currently owned by the active PlayerState."))
	FGameplayTagContainer PlayerProgressionTags;

	/** Character-owned progression currently owned by the active character runtime. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Interaction", meta = (ToolTip = "Character-owned progression currently owned by the active character runtime."))
	FGameplayTagContainer CharacterProgressionTags;

	/** Runtime-only interaction routing tags supplied by the caller for this specific query/session. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Interaction", meta = (ToolTip = "Runtime-only interaction routing tags supplied by the caller for this specific query or session."))
	FGameplayTagContainer TransientProgressionTags;

	/** Character-owned currently projected loadout tags. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Interaction", meta = (ToolTip = "Character-owned currently projected loadout tags for the active runtime."))
	FGameplayTagContainer LoadoutTags;

	/** Character-owned currently activated Invader upgrade tags. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Interaction", meta = (ToolTip = "Character-owned currently activated Invader upgrade tags for the active runtime."))
	FGameplayTagContainer InvaderUpgradeTags;

	/** Live ASC-owned gameplay tags pulled from the active character runtime. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Interaction", meta = (ToolTip = "Live ASC-owned gameplay tags pulled from the active character runtime when the context was built."))
	FGameplayTagContainer ASCTags;

	/** Player/character identity tags that are not progression-owned. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Interaction", meta = (ToolTip = "Current player and character identity tags such as the canonical character tag and player-slot tag."))
	FGameplayTagContainer IdentityTags;

	/** Speaker tags that identify who this player currently is in the interaction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Interaction", meta = (ToolTip = "Current runtime speaker tags that identify who this player currently is in the interaction."))
	FGameplayTagContainer SpeakerTags;

	/** Current pawn-derived speaker tag when available. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Interaction", meta = (ToolTip = "Current pawn-derived speaker tag when available."))
	FGameplayTag CurrentSpeakerTag;

	/** Canonical speaker tag derived from current character identity when available. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Interaction", meta = (ToolTip = "Canonical speaker tag derived from the current character identity when available."))
	FGameplayTag CanonicalSpeakerTag;

	/** Canonical gameplay-tag identity for the currently active character. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Interaction", meta = (ToolTip = "Canonical gameplay-tag identity for the currently active character."))
	FGameplayTag CurrentCharacterTag;

	/** Runtime player-slot tag for the currently active player. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Interaction", meta = (ToolTip = "Runtime player-slot tag for the currently active player."))
	FGameplayTag PlayerSlotTag;

	/** Lightweight economy values relevant to this interaction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Interaction", meta = (ToolTip = "Lightweight economy values relevant to the current interaction."))
	FARInteractionEconomySnapshot Economy;

	/** Final merged identity bucket consumed by systems such as Parley. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Interaction", meta = (ToolTip = "Merged identity bucket containing progression, runtime speaker identity, loadout, ASC, and caller-supplied transient tags."))
	FGameplayTagContainer CombinedTags;
};
