// ARShipCharacterBase.cpp

#include "ARShipCharacterBase.h"

#include "ARPlayerStateBase.h"
#include "ARPlayerController.h"
#include "ARAbilitySet.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Abilities/GameplayAbility.h"

#include "ContentLookupSubsystem.h"
#include "ARWeaponDefinition.h"

#include "UObject/UnrealType.h"

// --------------------
// Static names (row struct fields)
// --------------------

const FName AARShipCharacterBase::NAME_PrimaryWeapon(TEXT("PrimaryWeapon"));
const FName AARShipCharacterBase::NAME_StartupAbilities(TEXT("StartupAbilities"));
const FName AARShipCharacterBase::NAME_StartupEffects(TEXT("StartupEffects"));
const FName AARShipCharacterBase::NAME_ShipTags(TEXT("ShipTags"));
const FName AARShipCharacterBase::NAME_Stats(TEXT("Stats"));
const FName AARShipCharacterBase::NAME_MovementType(TEXT("MovementType"));
const FName AARShipCharacterBase::NAME_LoadoutTags(TEXT("LoadoutTags"));

// --------------------
// Tag roots
// --------------------

const FGameplayTag AARShipCharacterBase::TAGROOT_Ships =
FGameplayTag::RequestGameplayTag(TEXT("Unlocks.Ships"));

const FGameplayTag AARShipCharacterBase::TAGROOT_Secondaries =
FGameplayTag::RequestGameplayTag(TEXT("Unlocks.Secondaries"));

const FGameplayTag AARShipCharacterBase::TAGROOT_Gadgets =
FGameplayTag::RequestGameplayTag(TEXT("Unlocks.Gadgets"));

AARShipCharacterBase::AARShipCharacterBase()
{
	bReplicates = true;
}

UAbilitySystemComponent* AARShipCharacterBase::GetAbilitySystemComponent() const
{
	if (CachedASC) return CachedASC;

	const AARPlayerStateBase* PS = GetPlayerState<AARPlayerStateBase>();
	return PS ? PS->GetAbilitySystemComponent() : nullptr;
}

const UARWeaponDefinition* AARShipCharacterBase::GetPrimaryWeaponDefinition() const
{
	return CurrentPrimaryWeapon;
}

// --------------------
// Small reflection helpers for row structs
// --------------------

// Helper: Find a property by name prefix (handles Unreal's auto-generated suffixes like Stats_39_xxxx)
FProperty* AARShipCharacterBase::FindPropertyByNamePrefix(const UScriptStruct* StructType, const FString& Prefix)
{
	if (!StructType) return nullptr;

	for (TFieldIterator<FProperty> It(StructType); It; ++It)
	{
		FProperty* Prop = *It;
		if (Prop && Prop->GetName().StartsWith(Prefix))
		{
			return Prop;
		}
	}
	return nullptr;
}

static TSubclassOf<UGameplayEffect> ExtractEffectClass(const UScriptStruct* StructType, const void* StructData, FName PropName)
{
	if (!StructType || !StructData) return nullptr;

	FProperty* P = AARShipCharacterBase::FindPropertyByNamePrefix(StructType, PropName.ToString());
	if (!P) return nullptr;

	if (const FClassProperty* ClassProp = CastField<FClassProperty>(P))
	{
		UClass* C = Cast<UClass>(ClassProp->GetPropertyValue_InContainer(StructData));
		return C ? TSubclassOf<UGameplayEffect>(C) : nullptr;
	}

	if (const FSoftClassProperty* SoftClassProp = CastField<FSoftClassProperty>(P))
	{
		const FSoftObjectPtr Soft = SoftClassProp->GetPropertyValue_InContainer(StructData);
		UClass* C = Cast<UClass>(Soft.LoadSynchronous());
		return C ? TSubclassOf<UGameplayEffect>(C) : nullptr;
	}

	return nullptr;
}

static UARWeaponDefinition* ExtractWeaponDef(const UScriptStruct* StructType, const void* StructData, FName PropName)
{
	if (!StructType || !StructData) return nullptr;

	FProperty* P = AARShipCharacterBase::FindPropertyByNamePrefix(StructType, PropName.ToString());
	if (!P) return nullptr;

	if (const FObjectProperty* ObjProp = CastField<FObjectProperty>(P))
	{
		UObject* Obj = ObjProp->GetObjectPropertyValue_InContainer(StructData);
		return Cast<UARWeaponDefinition>(Obj);
	}

	if (const FSoftObjectProperty* SoftProp = CastField<FSoftObjectProperty>(P))
	{
		const FSoftObjectPtr Soft = SoftProp->GetPropertyValue_InContainer(StructData);
		UObject* Obj = Soft.LoadSynchronous();
		return Cast<UARWeaponDefinition>(Obj);
	}

	return nullptr;
}

static void GrantAbilityArrayFromStruct(
	UAbilitySystemComponent* ASC,
	const UScriptStruct* StructType,
	const void* StructData,
	FName ArrayPropName,
	TArray<FGameplayAbilitySpecHandle>& OutGrantedHandles
)
{
	if (!ASC || !StructType || !StructData) return;

	FProperty* P = AARShipCharacterBase::FindPropertyByNamePrefix(StructType, ArrayPropName.ToString());
	FArrayProperty* ArrayProp = CastField<FArrayProperty>(P);
	if (!ArrayProp) return;

	FProperty* Inner = ArrayProp->Inner;
	if (!Inner) return;

	FScriptArrayHelper Helper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(StructData));
	const int32 Num = Helper.Num();

	for (int32 i = 0; i < Num; ++i)
	{
		void* ElemPtr = Helper.GetRawPtr(i);

		UClass* AbilityClass = nullptr;

		if (FClassProperty* ClassProp = CastField<FClassProperty>(Inner))
		{
			AbilityClass = Cast<UClass>(ClassProp->GetPropertyValue(ElemPtr));
		}
		else if (FSoftClassProperty* SoftClassProp = CastField<FSoftClassProperty>(Inner))
		{
			const FSoftObjectPtr Soft = SoftClassProp->GetPropertyValue(ElemPtr);
			AbilityClass = Cast<UClass>(Soft.LoadSynchronous());
		}

		TSubclassOf<UGameplayAbility> AbilityGA = AbilityClass;
		if (!AbilityGA) continue;

		FGameplayAbilitySpec Spec(AbilityGA, 1);
		OutGrantedHandles.Add(ASC->GiveAbility(Spec));
	}
}

static void ApplyEffectArrayFromStruct(
	UAbilitySystemComponent* ASC,
	const UScriptStruct* StructType,
	const void* StructData,
	FName ArrayPropName,
	TArray<FActiveGameplayEffectHandle>& OutAppliedHandles
)
{
	if (!ASC || !StructType || !StructData) return;

	FProperty* P = AARShipCharacterBase::FindPropertyByNamePrefix(StructType, ArrayPropName.ToString());
	FArrayProperty* ArrayProp = CastField<FArrayProperty>(P);
	if (!ArrayProp) return;

	FProperty* Inner = ArrayProp->Inner;
	if (!Inner) return;

	FScriptArrayHelper Helper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(StructData));
	const int32 Num = Helper.Num();

	for (int32 i = 0; i < Num; ++i)
	{
		void* ElemPtr = Helper.GetRawPtr(i);

		UClass* EffectClass = nullptr;

		if (FClassProperty* ClassProp = CastField<FClassProperty>(Inner))
		{
			EffectClass = Cast<UClass>(ClassProp->GetPropertyValue(ElemPtr));
		}
		else if (FSoftClassProperty* SoftClassProp = CastField<FSoftClassProperty>(Inner))
		{
			const FSoftObjectPtr Soft = SoftClassProp->GetPropertyValue(ElemPtr);
			EffectClass = Cast<UClass>(Soft.LoadSynchronous());
		}

		TSubclassOf<UGameplayEffect> EffectGE = EffectClass;
		if (!EffectGE) continue;

		const FGameplayEffectContextHandle Ctx = ASC->MakeEffectContext();
		const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(EffectGE, 1.0f, Ctx);

		if (Spec.IsValid())
		{
			OutAppliedHandles.Add(ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()));
		}
	}
}

static void AppendTagContainerFromStruct(const UScriptStruct* StructType, const void* StructData, FName PropName, FGameplayTagContainer& OutTags)
{
	if (!StructType || !StructData) return;

	FProperty* P = AARShipCharacterBase::FindPropertyByNamePrefix(StructType, PropName.ToString());
	FStructProperty* SP = CastField<FStructProperty>(P);
	if (!SP) return;

	if (SP->Struct != FGameplayTagContainer::StaticStruct())
	{
		return;
	}

	const void* TagsPtr = SP->ContainerPtrToValuePtr<void>(StructData);
	const FGameplayTagContainer* TagContainer = reinterpret_cast<const FGameplayTagContainer*>(TagsPtr);
	if (TagContainer)
	{
		OutTags.AppendTags(*TagContainer);
	}
}

static bool ExtractGameplayTagFromStruct(const UScriptStruct* StructType, const void* StructData, FName PropName, FGameplayTag& OutTag)
{
	OutTag = FGameplayTag();

	if (!StructType || !StructData) return false;

	FProperty* P = AARShipCharacterBase::FindPropertyByNamePrefix(StructType, PropName.ToString());
	FStructProperty* SP = CastField<FStructProperty>(P);
	if (!SP) return false;

	if (SP->Struct != FGameplayTag::StaticStruct())
	{
		return false;
	}

	const void* TagPtr = SP->ContainerPtrToValuePtr<void>(StructData);
	const FGameplayTag* Tag = reinterpret_cast<const FGameplayTag*>(TagPtr);
	if (Tag && Tag->IsValid())
	{
		OutTag = *Tag;
		return true;
	}

	return false;
}

// --------------------
// AbilitySet grant (keeps your ActivationTag override support)
// --------------------

static void GrantAbilitySet(
	UAbilitySystemComponent* ASC,
	const UARAbilitySet* Set,
	TArray<FGameplayAbilitySpecHandle>& OutGranted,
	TArray<FActiveGameplayEffectHandle>& OutApplied
)
{
	if (!ASC || !Set) return;

	for (const FARAbilitySet_AbilityEntry& Entry : Set->Abilities)
	{
		if (!Entry.Ability) continue;

		FGameplayAbilitySpec Spec(Entry.Ability, Entry.Level);

		// If provided, inject tag at grant-time (safety net; canonical is still GA.AbilityTags)
		if (Entry.ActivationTag.IsValid())
		{
			Spec.GetDynamicSpecSourceTags().AddTag(Entry.ActivationTag);
		}

		OutGranted.Add(ASC->GiveAbility(Spec));
	}

	for (const FARAbilitySet_EffectEntry& Entry : Set->StartupEffects)
	{
		if (!Entry.Effect) continue;

		const FGameplayEffectContextHandle Ctx = ASC->MakeEffectContext();
		const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(Entry.Effect, Entry.Level, Ctx);

		if (Spec.IsValid())
		{
			OutApplied.Add(ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()));
		}
	}
}

// --------------------
// Possession / initialization
// --------------------

void AARShipCharacterBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	InitAbilityActorInfo();

	if (!HasAuthority())
	{
		return;
	}

	ClearAppliedLoadout();
	GrantCommonAbilitySetFromController(NewController);

	// 2) Read loadout tags from playerstate
	FGameplayTagContainer LoadoutTags;
	if (!GetPlayerLoadoutTags(LoadoutTags))
	{
		UE_LOG(LogTemp, Error, TEXT("PossessedBy: Failed to get LoadoutTags from PlayerState"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("PossessedBy: Got %d LoadoutTags:"), LoadoutTags.Num());
	for (const FGameplayTag& Tag : LoadoutTags)
	{
		UE_LOG(LogTemp, Warning, TEXT("  - %s"), *Tag.ToString());
	}
	ApplyLoadoutTagsToASC(LoadoutTags);

	// 3) Resolve + apply Ship baseline row
	{
		FGameplayTag ShipTag;
		if (!FindFirstTagUnderRoot(LoadoutTags, TAGROOT_Ships, ShipTag))
		{
			UE_LOG(LogTemp, Error, TEXT("PossessedBy: No ship tag found under root '%s'"), *TAGROOT_Ships.ToString());
			return;
		}

		UE_LOG(LogTemp, Warning, TEXT("PossessedBy: Found ShipTag = '%s'"), *ShipTag.ToString());

		FInstancedStruct ShipRow;
		FString Error;
		if (ResolveRowFromTag(ShipTag, ShipRow, Error))
		{
			UE_LOG(LogTemp, Warning, TEXT("PossessedBy: Successfully resolved ship row"));
			ApplyResolvedRowBaseline(ShipRow);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("PossessedBy: Failed to resolve ship row. Error: %s"), *Error);
			return;
		}
	}

	// 4) Resolve + apply Secondary row (optional)
	{
		FGameplayTag SecondaryTag;
		if (FindFirstTagUnderRoot(LoadoutTags, TAGROOT_Secondaries, SecondaryTag))
		{
			FInstancedStruct SecondaryRow;
			FString Error;
			if (ResolveRowFromTag(SecondaryTag, SecondaryRow, Error))
			{
				ApplyResolvedRowBaseline(SecondaryRow);
			}
		}
	}

	// 5) Resolve + apply Gadget row (optional)
	{
		FGameplayTag GadgetTag;
		if (FindFirstTagUnderRoot(LoadoutTags, TAGROOT_Gadgets, GadgetTag))
		{
			FInstancedStruct GadgetRow;
			FString Error;
			if (ResolveRowFromTag(GadgetTag, GadgetRow, Error))
			{
				ApplyResolvedRowBaseline(GadgetRow);
			}
		}
	}
}

void AARShipCharacterBase::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	InitAbilityActorInfo();
}

void AARShipCharacterBase::InitAbilityActorInfo()
{
	AARPlayerStateBase* PS = GetPlayerState<AARPlayerStateBase>();
	if (!PS) return;

	UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent();
	if (!ASC) return;

	CachedASC = ASC;

	// Owner = PlayerState, Avatar = this pawn
	ASC->InitAbilityActorInfo(PS, this);
}

void AARShipCharacterBase::UnPossessed()
{
	Super::UnPossessed();

	if (HasAuthority())
	{
		ClearAppliedLoadout();
	}
}

void AARShipCharacterBase::GrantCommonAbilitySetFromController(AController* NewController)
{
	AARPlayerController* ARPC = Cast<AARPlayerController>(NewController);
	if (!ARPC) return;

	const UARAbilitySet* Set = ARPC->GetCommonAbilitySet();
	if (!Set) return;

	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC) return;

	GrantAbilitySet(ASC, Set, GrantedAbilityHandles, AppliedEffectHandles);
}

// --------------------
// Baseline application for any row struct that contains the common fields:
// Stats, StartupAbilities, StartupEffects, ShipTags, MovementType, PrimaryWeapon(optional)
// --------------------

void AARShipCharacterBase::ApplyResolvedRowBaseline(const FInstancedStruct& RowStruct)
{
	if (!HasAuthority()) return;

	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC) return;

	const UScriptStruct* StructType = RowStruct.GetScriptStruct();
	const void* StructData = RowStruct.GetMemory();
	if (!StructType || !StructData) return;

	UE_LOG(LogTemp, Warning, TEXT("ApplyResolvedRowBaseline: Processing struct '%s'"), *StructType->GetName());

	// DEBUG: List all properties on this struct
	LogAllPropertiesOnStruct(StructType);
	// Stats effect (optional)
	{
		TSubclassOf<UGameplayEffect> StatsGE = ExtractEffectClass(StructType, StructData, NAME_Stats);
		if (StatsGE)
		{
			UE_LOG(LogTemp, Warning, TEXT("  - Found Stats effect: %s"), *StatsGE->GetName());
			const FGameplayEffectContextHandle Ctx = ASC->MakeEffectContext();
			const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(StatsGE, 1.0f, Ctx);
			if (Spec.IsValid())
			{
				AppliedEffectHandles.Add(ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()));
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("  - No Stats effect found (property '%s' not found or null)"), *NAME_Stats.ToString());
		}
	}

	// Primary weapon (only expected on Ship rows; safe to attempt)
	if (!CurrentPrimaryWeapon)
	{
		UARWeaponDefinition* WeaponDef = ExtractWeaponDef(StructType, StructData, NAME_PrimaryWeapon);
		if (WeaponDef)
		{
			UE_LOG(LogTemp, Warning, TEXT("  - Found Primary Weapon: %s"), *WeaponDef->GetName());
			CurrentPrimaryWeapon = WeaponDef;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("  - No Primary Weapon found (property '%s' not found or null)"), *NAME_PrimaryWeapon.ToString());
		}
	}

	// Startup abilities/effects
	{
		// Debug: Check if property exists (using prefix matching now)
		FProperty* P = FindPropertyByNamePrefix(StructType, NAME_StartupAbilities.ToString());
		if (P)
		{
			FArrayProperty* ArrayProp = CastField<FArrayProperty>(P);
			if (ArrayProp)
			{
				FScriptArrayHelper Helper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(StructData));
				int32 NumAbilities = Helper.Num();
				UE_LOG(LogTemp, Warning, TEXT("  - Found StartupAbilities array with %d elements"), NumAbilities);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("  - StartupAbilities property exists but is NOT an array (type: %s)"), *P->GetClass()->GetName());
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("  - StartupAbilities property NOT FOUND on struct"));
		}

		GrantAbilityArrayFromStruct(ASC, StructType, StructData, NAME_StartupAbilities, GrantedAbilityHandles);
	}

	{
		// Debug: Check if property exists (using prefix matching now)
		FProperty* P = FindPropertyByNamePrefix(StructType, NAME_StartupEffects.ToString());
		if (P)
		{
			FArrayProperty* ArrayProp = CastField<FArrayProperty>(P);
			if (ArrayProp)
			{
				FScriptArrayHelper Helper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(StructData));
				int32 NumEffects = Helper.Num();
				UE_LOG(LogTemp, Warning, TEXT("  - Found StartupEffects array with %d elements"), NumEffects);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("  - StartupEffects property exists but is NOT an array"));
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("  - StartupEffects property NOT FOUND on struct"));
		}

		ApplyEffectArrayFromStruct(ASC, StructType, StructData, NAME_StartupEffects, AppliedEffectHandles);
	}

	// Loose tags (ShipTags field)
	{
		FGameplayTagContainer LooseTags;
		AppendTagContainerFromStruct(StructType, StructData, NAME_ShipTags, LooseTags);

		if (!LooseTags.IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("  - Found %d ShipTags"), LooseTags.Num());
			ASC->AddLooseGameplayTags(LooseTags);
			AppliedLooseTags.AppendTags(LooseTags);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("  - ShipTags property is empty or not found"));
		}
	}

	// MovementType tag (optional)
	{
		FGameplayTag MovementTag;
		if (ExtractGameplayTagFromStruct(StructType, StructData, NAME_MovementType, MovementTag))
		{
			UE_LOG(LogTemp, Warning, TEXT("  - Found MovementType: %s"), *MovementTag.ToString());
			FGameplayTagContainer MoveTags;
			MoveTags.AddTag(MovementTag);
			ASC->AddLooseGameplayTags(MoveTags);
			AppliedLooseTags.AppendTags(MoveTags);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("  - No MovementType found"));
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("ApplyResolvedRowBaseline: COMPLETE"));
}

// --------------------
// Cleanup
// --------------------

void AARShipCharacterBase::ClearAppliedLoadout()
{
	if (!HasAuthority()) return;

	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC) return;

	for (const FGameplayAbilitySpecHandle& Handle : GrantedAbilityHandles)
	{
		if (Handle.IsValid())
		{
			ASC->ClearAbility(Handle);
		}
	}
	GrantedAbilityHandles.Reset();

	for (const FActiveGameplayEffectHandle& Handle : AppliedEffectHandles)
	{
		if (Handle.IsValid())
		{
			ASC->RemoveActiveGameplayEffect(Handle);
		}
	}
	AppliedEffectHandles.Reset();

	if (!AppliedLooseTags.IsEmpty())
	{
		ASC->RemoveLooseGameplayTags(AppliedLooseTags);
		AppliedLooseTags.Reset();
	}

	CurrentPrimaryWeapon = nullptr;
}

// --------------------
// PlayerState loadout tags
// --------------------

bool AARShipCharacterBase::GetPlayerLoadoutTags(FGameplayTagContainer& OutLoadoutTags) const
{
	OutLoadoutTags.Reset();

	APlayerState* PS = GetPlayerState();
	if (!PS) return false;

	// Look for a property named "LoadoutTags" on the concrete (BP) PlayerState class.
	FProperty* P = PS->GetClass()->FindPropertyByName(NAME_LoadoutTags);
	FStructProperty* SP = CastField<FStructProperty>(P);

	if (!SP || SP->Struct != FGameplayTagContainer::StaticStruct())
	{
		return false;
	}

	const void* TagsPtr = SP->ContainerPtrToValuePtr<void>(PS);
	const FGameplayTagContainer* LoadoutTagContainer = reinterpret_cast<const FGameplayTagContainer*>(TagsPtr);
	if (!LoadoutTagContainer) return false;

	OutLoadoutTags = *LoadoutTagContainer;
	return true;
}

bool AARShipCharacterBase::FindFirstTagUnderRoot(const FGameplayTagContainer& InTags, const FGameplayTag& Root, FGameplayTag& OutTag) const
{
	OutTag = FGameplayTag();
	if (!Root.IsValid()) return false;

	for (const FGameplayTag& Tag : InTags)
	{
		if (Tag.MatchesTag(Root))
		{
			OutTag = Tag;
			return true;
		}
	}
	return false;
}

bool AARShipCharacterBase::ResolveRowFromTag(FGameplayTag Tag, FInstancedStruct& OutRow, FString& OutError) const
{
	OutRow.Reset();
	OutError.Reset();

	UWorld* World = GetWorld();
	if (!World) { OutError = TEXT("No World."); return false; }

	UGameInstance* GI = World->GetGameInstance();
	if (!GI) { OutError = TEXT("No GameInstance."); return false; }

	UContentLookupSubsystem* Lookup = GI->GetSubsystem<UContentLookupSubsystem>();
	if (!Lookup) { OutError = TEXT("No ContentLookupSubsystem."); return false; }

	if (!Lookup->LookupWithGameplayTag(Tag, OutRow, OutError))
	{
		return false;
	}

	return OutRow.IsValid();
}

// --------------------
// Activation: pick ONE ability deterministically
// --------------------

bool AARShipCharacterBase::SpecMatchesAnyTag(const FGameplayAbilitySpec& Spec, const FGameplayTagContainer& InTagsToMatch, int32& OutBestScore)
{
	OutBestScore = 0;

	UGameplayAbility* AbilityCDO = Spec.Ability;
	if (!AbilityCDO) return false;

	// Gather tags from:
	// 1) Ability's declared AbilityTags
	// 2) Spec's dynamic source tags (grant-time injected tags)
	const FGameplayTagContainer& AbilityTags = AbilityCDO->AbilityTags;
	const FGameplayTagContainer& SpecTags = Spec.GetDynamicSpecSourceTags();

	// DEBUG
	UE_LOG(LogTemp, Warning, TEXT("SpecMatchesAnyTag: Ability=%s"), *AbilityCDO->GetName());
	UE_LOG(LogTemp, Warning, TEXT("  AbilityTags (%d): %s"), AbilityTags.Num(), *AbilityTags.ToStringSimple());
	UE_LOG(LogTemp, Warning, TEXT("  SpecTags (%d): %s"), SpecTags.Num(), *SpecTags.ToStringSimple());
	UE_LOG(LogTemp, Warning, TEXT("  Looking for tags (%d): %s"), InTagsToMatch.Num(), *InTagsToMatch.ToStringSimple());

	// Scoring:
	// - exact match => +100
	// - parent match => +10
	// (we take the best score across all provided tags)
	int32 Best = 0;
	bool bAny = false;

	for (const FGameplayTag& Query : InTagsToMatch)
	{
		if (!Query.IsValid()) continue;

		if (AbilityTags.HasTagExact(Query) || SpecTags.HasTagExact(Query))
		{
			UE_LOG(LogTemp, Warning, TEXT("    MATCH (exact): %s"), *Query.ToString());
			Best = FMath::Max(Best, 100);
			bAny = true;
			continue;
		}

		// Parent match: query is a parent of some ability tag
		if (AbilityTags.HasTag(Query) || SpecTags.HasTag(Query))
		{
			UE_LOG(LogTemp, Warning, TEXT("    MATCH (parent): %s"), *Query.ToString());
			Best = FMath::Max(Best, 10);
			bAny = true;
			continue;
		}

		UE_LOG(LogTemp, Warning, TEXT("    NO MATCH: %s"), *Query.ToString());
	}

	OutBestScore = Best;
	UE_LOG(LogTemp, Warning, TEXT("  Result: bAny=%d, Score=%d"), bAny, Best);
	return bAny;
}

bool AARShipCharacterBase::PickBestMatchingAbilityHandle(
	UAbilitySystemComponent* ASC,
	const FGameplayTagContainer& InTagsToMatch,
	FGameplayAbilitySpecHandle& OutHandle
)
{
	OutHandle = FGameplayAbilitySpecHandle();

	if (!ASC || InTagsToMatch.IsEmpty())
	{
		return false;
	}

	int32 BestScore = -1;
	int32 BestLevel = -1;
	// Use array index for deterministic tie-breaking instead of private Handle value
	int32 BestIndex = -1;

	const TArray<FGameplayAbilitySpec>& ActivatableAbilities = ASC->GetActivatableAbilities();

	for (int32 i = 0; i < ActivatableAbilities.Num(); ++i)
	{
		const FGameplayAbilitySpec& Spec = ActivatableAbilities[i];

		int32 Score = 0;
		if (!SpecMatchesAnyTag(Spec, InTagsToMatch, Score))
		{
			continue;
		}

		// Prefer higher score, then higher level, then lower index for deterministic tie-break.
		const int32 Level = Spec.Level;

		const bool bBetter =
			(Score > BestScore) ||
			(Score == BestScore && Level > BestLevel) ||
			(Score == BestScore && Level == BestLevel && (BestIndex < 0 || i < BestIndex));

		if (bBetter)
		{
			BestScore = Score;
			BestLevel = Level;
			BestIndex = i;
			OutHandle = Spec.Handle;
		}
	}

	return OutHandle.IsValid();
}

bool AARShipCharacterBase::ActivateAbilityByTag(FGameplayTag Tag, bool bAllowRemoteActivation)
{
	FGameplayTagContainer TagsToActivate;
	if (Tag.IsValid())
	{
		TagsToActivate.AddTag(Tag);
	}
	return ActivateAbilityByTags(TagsToActivate, bAllowRemoteActivation);
}

bool AARShipCharacterBase::ActivateAbilityByTags(const FGameplayTagContainer& InTagsToActivate, bool bAllowRemoteActivation)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC) 
	{
		UE_LOG(LogTemp, Error, TEXT("ActivateAbilityByTags: No ASC found"));
		return false;
	}
	if (InTagsToActivate.IsEmpty()) 
	{
		UE_LOG(LogTemp, Error, TEXT("ActivateAbilityByTags: Empty tag container"));
		return false;
	}

	UE_LOG(LogTemp, Warning, TEXT("ActivateAbilityByTags: Looking for tags: %s"), *InTagsToActivate.ToStringSimple());

	FGameplayAbilitySpecHandle Handle;
	if (!PickBestMatchingAbilityHandle(ASC, InTagsToActivate, Handle))
	{
		UE_LOG(LogTemp, Error, TEXT("ActivateAbilityByTags: No matching ability found"));
		return false;
	}

	UE_LOG(LogTemp, Warning, TEXT("ActivateAbilityByTags: Found matching ability, attempting to activate"));

	bool bResult = ASC->TryActivateAbility(Handle, bAllowRemoteActivation);
	UE_LOG(LogTemp, Warning, TEXT("ActivateAbilityByTags: TryActivateAbility returned %d"), bResult);
	return bResult;
}

int32 AARShipCharacterBase::ActivateAllAbilitiesByTag(FGameplayTag Tag, bool bAllowRemoteActivation)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC) return 0;
	if (!Tag.IsValid()) return 0;

	FGameplayTagContainer TagsToMatch;
	TagsToMatch.AddTag(Tag);

	int32 ActivatedCount = 0;

	// Activate every matching ability (explicit opt-in; your default should be ActivateAbilityByTag)
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		int32 Score = 0;
		if (!SpecMatchesAnyTag(Spec, TagsToMatch, Score))
		{
			continue;
		}

		if (ASC->TryActivateAbility(Spec.Handle, bAllowRemoteActivation))
		{
			ActivatedCount++;
		}
	}

	return ActivatedCount;
}

void AARShipCharacterBase::CancelAbilityByTag(FGameplayTag Tag)
{
	FGameplayTagContainer TagsToCancel;
	if (Tag.IsValid())
	{
		TagsToCancel.AddTag(Tag);
	}
	CancelAbilityByTags(TagsToCancel);
}

void AARShipCharacterBase::CancelAbilityByTags(const FGameplayTagContainer& InTagsToCancel)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC) return;
	if (InTagsToCancel.IsEmpty()) return;

	// Cancels any active abilities that have any of these tags
	FGameplayTagContainer CopyTags = InTagsToCancel;
	ASC->CancelAbilities(&CopyTags);
}

// Debug helper: logs all properties on a struct
void AARShipCharacterBase::LogAllPropertiesOnStruct(const UScriptStruct* StructType)
{
	if (!StructType) return;

	UE_LOG(LogTemp, Warning, TEXT("=== Properties on struct '%s' ==="), *StructType->GetName());
	for (TFieldIterator<FProperty> It(StructType); It; ++It)
	{
		FProperty* Prop = *It;
		if (Prop)
		{
			UE_LOG(LogTemp, Warning, TEXT("  - %s (%s)"), *Prop->GetName(), *Prop->GetClass()->GetName());
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("=== End Properties ==="));
}

void AARShipCharacterBase::ApplyLoadoutTagsToASC(const FGameplayTagContainer& InLoadoutTags)
{
	if (!HasAuthority()) return;
	if (InLoadoutTags.IsEmpty()) return;

	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC)
	{
		UE_LOG(LogTemp, Error, TEXT("ApplyLoadoutTagsToASC: No ASC found"));
		return;
	}

	ASC->AddLooseGameplayTags(InLoadoutTags);
	AppliedLooseTags.AppendTags(InLoadoutTags);

	UE_LOG(LogTemp, Warning, TEXT("ApplyLoadoutTagsToASC: Mirrored %d loadout tags into ASC"), InLoadoutTags.Num());
	for (const FGameplayTag& Tag : InLoadoutTags)
	{
		UE_LOG(LogTemp, Warning, TEXT("  - Mirrored tag: %s"), *Tag.ToString());
	}
}
