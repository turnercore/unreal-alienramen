// ARPlayerCharacterInvader.cpp

#include "ARPlayerCharacterInvader.h"
#include "ARLog.h"

#include "ARPlayerStateBase.h"
#include "ARPlayerController.h"
#include "ARAbilitySet.h"
#include "AREnemyBase.h"
#include "AREnemyIncomingDamageEffect.h"
#include "ARAttributeSetCore.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Abilities/GameplayAbility.h"

#include "ContentLookupSubsystem.h"
#include "ARWeaponDefinition.h"

#include "UObject/UnrealType.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffectExtension.h"

static UARWeaponDefinition* ExtractWeaponDef(const UScriptStruct* StructType, const void* StructData, FName PropName);

// --------------------
// Static names (row struct fields)
// --------------------

const FName AARPlayerCharacterInvader::NAME_PrimaryWeapon(TEXT("PrimaryWeapon"));
const FName AARPlayerCharacterInvader::NAME_StartupAbilities(TEXT("StartupAbilities"));
const FName AARPlayerCharacterInvader::NAME_StartupEffects(TEXT("StartupEffects"));
const FName AARPlayerCharacterInvader::NAME_ShipTags(TEXT("ShipTags"));
const FName AARPlayerCharacterInvader::NAME_Stats(TEXT("Stats"));
const FName AARPlayerCharacterInvader::NAME_MovementType(TEXT("MovementType"));
const FName AARPlayerCharacterInvader::NAME_LoadoutTags(TEXT("LoadoutTags"));
static const FName NAME_LegacyASC(TEXT("ASC"));
static const FName NAME_LegacyBasePrimaryFireRateEffect(TEXT("BasePrimaryFireRateEffect"));

FGameplayTag AARPlayerCharacterInvader::GetTagRootShips()
{
	return FGameplayTag::RequestGameplayTag(TEXT("Unlock.Ship"));
}

FGameplayTag AARPlayerCharacterInvader::GetTagRootSecondaries()
{
	return FGameplayTag::RequestGameplayTag(TEXT("Unlock.Secondary"));
}

FGameplayTag AARPlayerCharacterInvader::GetTagRootHats()
{
	return FGameplayTag::RequestGameplayTag(TEXT("Unlock.Hat"));
}

AARPlayerCharacterInvader::AARPlayerCharacterInvader()
{
	bReplicates = true;

	static ConstructorHelpers::FClassFinder<UGameplayEffect> FireRateGEClass(
		TEXT("/Game/CodeAlong/Blueprints/GAS/GameplayEffects/GE_WeaponFireRate"));
	if (FireRateGEClass.Succeeded())
	{
		PrimaryWeaponFireRateEffectClass = FireRateGEClass.Class;
	}
}

UAbilitySystemComponent* AARPlayerCharacterInvader::GetAbilitySystemComponent() const
{
	if (CachedASC) return CachedASC;

	const AARPlayerStateBase* PS = GetPlayerState<AARPlayerStateBase>();
	return PS ? PS->GetAbilitySystemComponent() : nullptr;
}

const UARWeaponDefinition* AARPlayerCharacterInvader::GetPrimaryWeaponDefinition() const
{
	if (CurrentPrimaryWeapon)
	{
		return CurrentPrimaryWeapon;
	}

	// Fallback for client/server timing races: derive weapon from current loadout tags.
	FGameplayTagContainer LoadoutTags;
	if (!GetPlayerLoadoutTags(LoadoutTags) || LoadoutTags.IsEmpty())
	{
		return nullptr;
	}

	FGameplayTag ShipTag;
	if (!FindFirstTagUnderRoot(LoadoutTags, GetTagRootShips(), ShipTag))
	{
		return nullptr;
	}

	FInstancedStruct ShipRow;
	FString Error;
	if (!ResolveRowFromTag(ShipTag, ShipRow, Error))
	{
		return nullptr;
	}

	const UScriptStruct* StructType = ShipRow.GetScriptStruct();
	const void* StructData = ShipRow.GetMemory();
	if (!StructType || !StructData)
	{
		return nullptr;
	}

	if (UARWeaponDefinition* ResolvedWeapon = ExtractWeaponDef(StructType, StructData, NAME_PrimaryWeapon))
	{
		// Cache for subsequent calls.
		const_cast<AARPlayerCharacterInvader*>(this)->CurrentPrimaryWeapon = ResolvedWeapon;
		return ResolvedWeapon;
	}

	return nullptr;
}

namespace ARPlayerCharacterInvaderLocal
{
	static void AddRuntimeTags(UAbilitySystemComponent* ASC, const FGameplayTagContainer& Tags, bool bAuthority)
	{
		if (!ASC || Tags.IsEmpty())
		{
			return;
		}

		if (bAuthority)
		{
			ASC->AddLooseGameplayTags(Tags, 1, EGameplayTagReplicationState::TagOnly);
		}
		else
		{
			ASC->AddLooseGameplayTags(Tags);
		}
	}

	static void RemoveRuntimeTags(UAbilitySystemComponent* ASC, const FGameplayTagContainer& Tags, bool bAuthority)
	{
		if (!ASC || Tags.IsEmpty())
		{
			return;
		}

		if (bAuthority)
		{
			ASC->RemoveLooseGameplayTags(Tags, 1, EGameplayTagReplicationState::TagOnly);
		}
		else
		{
			ASC->RemoveLooseGameplayTags(Tags);
		}
	}

	static void SyncLegacyASCProperty(AARPlayerCharacterInvader* Ship, UAbilitySystemComponent* InASC)
	{
		if (!Ship)
		{
			return;
		}

		FProperty* P = Ship->GetClass()->FindPropertyByName(NAME_LegacyASC);
		FObjectProperty* OP = CastField<FObjectProperty>(P);
		if (!OP || !OP->PropertyClass || !OP->PropertyClass->IsChildOf(UAbilitySystemComponent::StaticClass()))
		{
			return;
		}

		OP->SetObjectPropertyValue_InContainer(Ship, InASC);
	}

	static void SyncLegacyBasePrimaryFireRateHandle(
		AARPlayerCharacterInvader* Ship,
		const FActiveGameplayEffectHandle& InHandle)
	{
		if (!Ship)
		{
			return;
		}

		FProperty* P = Ship->GetClass()->FindPropertyByName(NAME_LegacyBasePrimaryFireRateEffect);
		FStructProperty* SP = CastField<FStructProperty>(P);
		if (!SP || SP->Struct != FActiveGameplayEffectHandle::StaticStruct())
		{
			return;
		}

		void* HandlePtr = SP->ContainerPtrToValuePtr<void>(Ship);
		if (!HandlePtr)
		{
			return;
		}

		*reinterpret_cast<FActiveGameplayEffectHandle*>(HandlePtr) = InHandle;
	}

	static bool ApplyDamageToActorViaGAS_Local(AActor* Target, float Damage, AActor* Offender)
	{
		if (!Target || Damage <= 0.f)
		{
			return false;
		}

		if (AAREnemyBase* EnemyTarget = Cast<AAREnemyBase>(Target))
		{
			float IgnoredCurrentHealth = 0.f;
			return EnemyTarget->ApplyDamageViaGAS(Damage, Offender, IgnoredCurrentHealth);
		}

		if (AARPlayerCharacterInvader* ShipTarget = Cast<AARPlayerCharacterInvader>(Target))
		{
			float IgnoredCurrentHealth = 0.f;
			return ShipTarget->ApplyDamageViaGAS(Damage, Offender, IgnoredCurrentHealth);
		}

		return false;
	}
}

bool AARPlayerCharacterInvader::ApplyDamageViaGAS(float Damage, AActor* Offender, float& OutCurrentHealth)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	OutCurrentHealth = ASC ? ASC->GetNumericAttribute(UARAttributeSetCore::GetHealthAttribute()) : 0.f;

	if (!HasAuthority() || Damage <= 0.f)
	{
		return false;
	}

	ASC = GetAbilitySystemComponent();
	if (!ASC)
	{
		return false;
	}

	static const FName ShipDataDamageName(TEXT("Data.Damage"));
	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	if (Offender)
	{
		Context.AddInstigator(Offender, Offender);
	}

	const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UAREnemyIncomingDamageEffect::StaticClass(), 1.f, Context);
	if (!Spec.IsValid())
	{
		return false;
	}

	Spec.Data->SetSetByCallerMagnitude(ShipDataDamageName, Damage);
	ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	OutCurrentHealth = ASC->GetNumericAttribute(UARAttributeSetCore::GetHealthAttribute());
	return true;
}

bool AARPlayerCharacterInvader::ApplyDamageViaGAS(float Damage, AActor* Offender)
{
	float IgnoredCurrentHealth = 0.f;
	return ApplyDamageViaGAS(Damage, Offender, IgnoredCurrentHealth);
}

float AARPlayerCharacterInvader::GetCurrentDamageFromGAS() const
{
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC)
	{
		return 0.f;
	}

	return ASC->GetNumericAttribute(UARAttributeSetCore::GetDamageAttribute());
}

bool AARPlayerCharacterInvader::ApplyDamageToTargetViaGAS(AActor* Target, float DamageOverride)
{
	if (!HasAuthority())
	{
		return false;
	}

	const float DamageToApply = (DamageOverride >= 0.f) ? DamageOverride : GetCurrentDamageFromGAS();
	return ARPlayerCharacterInvaderLocal::ApplyDamageToActorViaGAS_Local(Target, DamageToApply, this);
}

// --------------------
// Small reflection helpers for row structs
// --------------------

// Helper: Find a property by name prefix (handles Unreal's auto-generated suffixes like Stats_39_xxxx)
FProperty* AARPlayerCharacterInvader::FindPropertyByNamePrefix(const UScriptStruct* StructType, const FString& Prefix)
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

	FProperty* P = AARPlayerCharacterInvader::FindPropertyByNamePrefix(StructType, PropName.ToString());
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

	FProperty* P = AARPlayerCharacterInvader::FindPropertyByNamePrefix(StructType, PropName.ToString());
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

	FProperty* P = AARPlayerCharacterInvader::FindPropertyByNamePrefix(StructType, ArrayPropName.ToString());
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

	FProperty* P = AARPlayerCharacterInvader::FindPropertyByNamePrefix(StructType, ArrayPropName.ToString());
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

	FProperty* P = AARPlayerCharacterInvader::FindPropertyByNamePrefix(StructType, PropName.ToString());
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

	FProperty* P = AARPlayerCharacterInvader::FindPropertyByNamePrefix(StructType, PropName.ToString());
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

void AARPlayerCharacterInvader::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	InitAbilityActorInfo();

	if (!HasAuthority())
	{
		return;
	}

	// Only gameplay player controllers should trigger ship loadout init.
	AARPlayerController* ARPC = Cast<AARPlayerController>(NewController);
	if (!ARPC)
	{
		UE_LOG(ARLog, Error, TEXT("[ShipGAS] Possess by non-gameplay controller '%s' (class=%s); ship loadout init skipped. Expect missing abilities/stats until possessed by AARPlayerController."),
			*GetNameSafe(NewController),
			*GetNameSafe(NewController ? NewController->GetClass() : nullptr));
		return;
	}

	bServerLoadoutApplied = false;
	LoadoutInitRetryCount = 0;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LoadoutInitRetryTimer);
	}

	ClearAppliedLoadout();
	GrantCommonAbilitySetFromController(ARPC);
	if (!TryApplyServerLoadoutFromPlayerState(true))
	{
		RetryServerLoadoutInit();
	}
}

void AARPlayerCharacterInvader::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	InitAbilityActorInfo();
}

void AARPlayerCharacterInvader::InitAbilityActorInfo()
{
	AARPlayerStateBase* PS = GetPlayerState<AARPlayerStateBase>();
	if (!PS)
	{
		UnbindMoveSpeedChangeDelegate(CachedASC);
		CachedASC = nullptr;
		ARPlayerCharacterInvaderLocal::SyncLegacyASCProperty(this, nullptr);
		return;
	}

	UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent();
	if (!ASC)
	{
		UnbindMoveSpeedChangeDelegate(CachedASC);
		CachedASC = nullptr;
		ARPlayerCharacterInvaderLocal::SyncLegacyASCProperty(this, nullptr);
		return;
	}

	if (CachedASC != ASC)
	{
		UnbindMoveSpeedChangeDelegate(CachedASC);
	}

	CachedASC = ASC;

	// Owner = PlayerState, Avatar = this pawn
	ASC->InitAbilityActorInfo(PS, this);
	BindMoveSpeedChangeDelegate(ASC);
	RefreshCharacterMovementSpeedFromAttributes();
	ARPlayerCharacterInvaderLocal::SyncLegacyASCProperty(this, ASC);
	ApplyOrRefreshPrimaryWeaponRuntimeEffects();

}

void AARPlayerCharacterInvader::ApplyOrRefreshPrimaryWeaponRuntimeEffects()
{
	if (!HasAuthority())
	{
		return;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	const UARWeaponDefinition* WeaponDef = GetPrimaryWeaponDefinition();
	if (!ASC || !WeaponDef || !PrimaryWeaponFireRateEffectClass)
	{
		return;
	}

	if (BasePrimaryFireRateEffectHandle.IsValid())
	{
		ASC->RemoveActiveGameplayEffect(BasePrimaryFireRateEffectHandle);
		BasePrimaryFireRateEffectHandle.Invalidate();
	}

	const FGameplayEffectContextHandle Ctx = ASC->MakeEffectContext();
	const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(PrimaryWeaponFireRateEffectClass, 1.0f, Ctx);
	if (!Spec.IsValid())
	{
		return;
	}

	static const FGameplayTag DataFireRateTag = FGameplayTag::RequestGameplayTag(TEXT("Data.FireRate"), false);
	if (DataFireRateTag.IsValid())
	{
		Spec.Data->SetSetByCallerMagnitude(DataFireRateTag, WeaponDef->FireRate);
	}

	BasePrimaryFireRateEffectHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	ARPlayerCharacterInvaderLocal::SyncLegacyBasePrimaryFireRateHandle(this, BasePrimaryFireRateEffectHandle);
}

void AARPlayerCharacterInvader::ClearPrimaryWeaponRuntimeEffects()
{
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		if (BasePrimaryFireRateEffectHandle.IsValid())
		{
			ASC->RemoveActiveGameplayEffect(BasePrimaryFireRateEffectHandle);
		}
	}

	BasePrimaryFireRateEffectHandle.Invalidate();
	ARPlayerCharacterInvaderLocal::SyncLegacyBasePrimaryFireRateHandle(this, BasePrimaryFireRateEffectHandle);
}

void AARPlayerCharacterInvader::UnPossessed()
{
	Super::UnPossessed();
	ClearPrimaryWeaponRuntimeEffects();
	UnbindMoveSpeedChangeDelegate(CachedASC);
	CachedASC = nullptr;
	ARPlayerCharacterInvaderLocal::SyncLegacyASCProperty(this, nullptr);
	bServerLoadoutApplied = false;
	LoadoutInitRetryCount = 0;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LoadoutInitRetryTimer);
	}

	if (HasAuthority())
	{
		ClearAppliedLoadout();
	}
}

void AARPlayerCharacterInvader::BindMoveSpeedChangeDelegate(UAbilitySystemComponent* ASC)
{
	if (!ASC || MoveSpeedChangedDelegateHandle.IsValid())
	{
		return;
	}

	MoveSpeedChangedDelegateHandle = ASC->GetGameplayAttributeValueChangeDelegate(UARAttributeSetCore::GetMoveSpeedAttribute())
		.AddUObject(this, &AARPlayerCharacterInvader::OnMoveSpeedChanged);
}

void AARPlayerCharacterInvader::UnbindMoveSpeedChangeDelegate(UAbilitySystemComponent* ASC)
{
	if (!ASC || !MoveSpeedChangedDelegateHandle.IsValid())
	{
		MoveSpeedChangedDelegateHandle.Reset();
		return;
	}

	ASC->GetGameplayAttributeValueChangeDelegate(UARAttributeSetCore::GetMoveSpeedAttribute())
		.Remove(MoveSpeedChangedDelegateHandle);
	MoveSpeedChangedDelegateHandle.Reset();
}

void AARPlayerCharacterInvader::OnMoveSpeedChanged(const FOnAttributeChangeData& ChangeData)
{
	(void)ChangeData;
	RefreshCharacterMovementSpeedFromAttributes();
}

void AARPlayerCharacterInvader::RefreshCharacterMovementSpeedFromAttributes()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	if (!MoveComp)
	{
		return;
	}

	const float MoveSpeed = FMath::Max(0.f, ASC->GetNumericAttribute(UARAttributeSetCore::GetMoveSpeedAttribute()));
	MoveComp->MaxWalkSpeed = MoveSpeed;
	MoveComp->MaxFlySpeed = MoveSpeed;
}

bool AARPlayerCharacterInvader::TryApplyServerLoadoutFromPlayerState(bool bLogErrors)
{
	if (!HasAuthority() || bServerLoadoutApplied)
	{
		return bServerLoadoutApplied;
	}

	FGameplayTagContainer LoadoutTags;
	if (!GetPlayerLoadoutTags(LoadoutTags) || LoadoutTags.IsEmpty())
	{
		if (bLogErrors)
		{
			UE_LOG(ARLog, Warning, TEXT("[ShipGAS] Deferred init: LoadoutTags unavailable/empty; retrying."));
		}
		return false;
	}

	UE_LOG(ARLog, Verbose, TEXT("[ShipGAS] Possess: applying %d loadout tags."), LoadoutTags.Num());
	ApplyLoadoutTagsToASC(LoadoutTags);

	// Ship baseline is required.
	FGameplayTag ShipTag;
	bool bFoundShipTag = FindFirstTagUnderRoot(LoadoutTags, GetTagRootShips(), ShipTag);
	if (!bFoundShipTag)
	{
		static const FGameplayTag LegacyShipRoot = FGameplayTag::RequestGameplayTag(TEXT("Unlocks.Ships"), false);
		if (LegacyShipRoot.IsValid())
		{
			bFoundShipTag = FindFirstTagUnderRoot(LoadoutTags, LegacyShipRoot, ShipTag);
		}
	}
	if (!bFoundShipTag)
	{
		if (bLogErrors)
		{
			UE_LOG(ARLog, Warning, TEXT("[ShipGAS] Deferred init: no ship tag found under root '%s'; tags=%s"), *GetTagRootShips().ToString(), *LoadoutTags.ToStringSimple());
		}
		return false;
	}

	FInstancedStruct ShipRow;
	FString Error;
	if (!ResolveRowFromTag(ShipTag, ShipRow, Error))
	{
		if (bLogErrors)
		{
			UE_LOG(ARLog, Warning, TEXT("[ShipGAS] Deferred init: could not resolve ship row for '%s'. %s"), *ShipTag.ToString(), *Error);
		}
		return false;
	}
	ApplyResolvedRowBaseline(ShipRow);

	// Secondary (optional legacy lane; never required for loadout init).
	FGameplayTag SecondaryTag;
	bool bFoundSecondaryTag = FindFirstTagUnderRoot(LoadoutTags, GetTagRootSecondaries(), SecondaryTag);
	if (!bFoundSecondaryTag)
	{
		static const FGameplayTag LegacySecondaryRoot = FGameplayTag::RequestGameplayTag(TEXT("Unlocks.Secondaries"), false);
		if (LegacySecondaryRoot.IsValid())
		{
			bFoundSecondaryTag = FindFirstTagUnderRoot(LoadoutTags, LegacySecondaryRoot, SecondaryTag);
		}
	}
	if (bFoundSecondaryTag)
	{
		FInstancedStruct SecondaryRow;
		FString SecondaryError;
		if (ResolveRowFromTag(SecondaryTag, SecondaryRow, SecondaryError))
		{
			ApplyResolvedRowBaseline(SecondaryRow);
		}
	}

	// Hat (optional).
	FGameplayTag HatTag;
	const bool bFoundHatTag = FindFirstTagUnderRoot(LoadoutTags, GetTagRootHats(), HatTag);
	if (bFoundHatTag)
	{
		FInstancedStruct HatRow;
		FString HatError;
		if (ResolveRowFromTag(HatTag, HatRow, HatError))
		{
			ApplyResolvedRowBaseline(HatRow);
		}
	}

	ApplyOrRefreshPrimaryWeaponRuntimeEffects();
	bServerLoadoutApplied = true;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LoadoutInitRetryTimer);
	}
	return true;
}

void AARPlayerCharacterInvader::RetryServerLoadoutInit()
{
	if (!HasAuthority())
	{
		return;
	}

	++LoadoutInitRetryCount;
	if (LoadoutInitRetryCount > 120)
	{
		UE_LOG(ARLog, Error, TEXT("[ShipGAS] Deferred init failed after %d retries."), LoadoutInitRetryCount - 1);
		return;
	}

	if (UWorld* World = GetWorld())
	{
		FTimerDelegate RetryDelegate;
		RetryDelegate.BindWeakLambda(this, [this]()
		{
			if (!TryApplyServerLoadoutFromPlayerState(false))
			{
				RetryServerLoadoutInit();
			}
		});

		World->GetTimerManager().SetTimer(LoadoutInitRetryTimer, RetryDelegate, 0.25f, false);
	}
}

void AARPlayerCharacterInvader::GrantCommonAbilitySetFromController(AController* NewController)
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

void AARPlayerCharacterInvader::ApplyResolvedRowBaseline(const FInstancedStruct& RowStruct)
{
	if (!HasAuthority()) return;

	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC) return;

	ClearPrimaryWeaponRuntimeEffects();

	const UScriptStruct* StructType = RowStruct.GetScriptStruct();
	const void* StructData = RowStruct.GetMemory();
	if (!StructType || !StructData) return;
	UE_LOG(ARLog, Verbose, TEXT("[ShipGAS] Applying loadout row baseline from struct '%s'."), *StructType->GetName());
	// Stats effect (optional)
	{
		TSubclassOf<UGameplayEffect> StatsGE = ExtractEffectClass(StructType, StructData, NAME_Stats);
		if (StatsGE)
		{
			const FGameplayEffectContextHandle Ctx = ASC->MakeEffectContext();
			const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(StatsGE, 1.0f, Ctx);
			if (Spec.IsValid())
			{
				AppliedEffectHandles.Add(ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()));
			}
		}
	}

	// Primary weapon (only expected on Ship rows; safe to attempt)
	if (!CurrentPrimaryWeapon)
	{
		UARWeaponDefinition* WeaponDef = ExtractWeaponDef(StructType, StructData, NAME_PrimaryWeapon);
		if (WeaponDef)
		{
			CurrentPrimaryWeapon = WeaponDef;
		}
	}

	// Startup abilities/effects
	GrantAbilityArrayFromStruct(ASC, StructType, StructData, NAME_StartupAbilities, GrantedAbilityHandles);
	ApplyEffectArrayFromStruct(ASC, StructType, StructData, NAME_StartupEffects, AppliedEffectHandles);

	// Loose tags (ShipTags field)
	{
		FGameplayTagContainer LooseTags;
		AppendTagContainerFromStruct(StructType, StructData, NAME_ShipTags, LooseTags);

		if (!LooseTags.IsEmpty())
		{
			ARPlayerCharacterInvaderLocal::AddRuntimeTags(ASC, LooseTags, HasAuthority());
			AppliedLooseTags.AppendTags(LooseTags);
		}
	}

	// MovementType tag (optional)
	{
		FGameplayTag MovementTag;
		if (ExtractGameplayTagFromStruct(StructType, StructData, NAME_MovementType, MovementTag))
		{
			FGameplayTagContainer MoveTags;
			MoveTags.AddTag(MovementTag);
			ARPlayerCharacterInvaderLocal::AddRuntimeTags(ASC, MoveTags, HasAuthority());
			AppliedLooseTags.AppendTags(MoveTags);
		}
	}
}

// --------------------
// Cleanup
// --------------------

void AARPlayerCharacterInvader::ClearAppliedLoadout()
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
		ARPlayerCharacterInvaderLocal::RemoveRuntimeTags(ASC, AppliedLooseTags, HasAuthority());
		AppliedLooseTags.Reset();
	}

	CurrentPrimaryWeapon = nullptr;
}

// --------------------
// PlayerState loadout tags
// --------------------

bool AARPlayerCharacterInvader::GetPlayerLoadoutTags(FGameplayTagContainer& OutLoadoutTags) const
{
	OutLoadoutTags.Reset();

	APlayerState* PS = GetPlayerState();
	if (!PS) return false;

	// Preferred path: read native field from our canonical PlayerState class.
	// This avoids accidentally reading a BP shadow variable with the same name.
	if (const AARPlayerStateBase* ARPS = Cast<AARPlayerStateBase>(PS))
	{
		OutLoadoutTags = ARPS->LoadoutTags;
		return true;
	}

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

bool AARPlayerCharacterInvader::FindFirstTagUnderRoot(const FGameplayTagContainer& InTags, const FGameplayTag& Root, FGameplayTag& OutTag) const
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

bool AARPlayerCharacterInvader::ResolveRowFromTag(FGameplayTag Tag, FInstancedStruct& OutRow, FString& OutError) const
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

bool AARPlayerCharacterInvader::SpecMatchesAnyTag(const FGameplayAbilitySpec& Spec, const FGameplayTagContainer& InTagsToMatch, int32& OutBestScore)
{
	OutBestScore = 0;

	UGameplayAbility* AbilityCDO = Spec.Ability;
	if (!AbilityCDO) return false;

	// Gather tags from:
	// 1) Ability's declared asset tags
	// 2) Spec's dynamic source tags (grant-time injected tags)
	const FGameplayTagContainer& AbilityTags = AbilityCDO->GetAssetTags();
	const FGameplayTagContainer& SpecTags = Spec.GetDynamicSpecSourceTags();

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
			Best = FMath::Max(Best, 100);
			bAny = true;
			continue;
		}

		// Parent match: query is a parent of some ability tag
		if (AbilityTags.HasTag(Query) || SpecTags.HasTag(Query))
		{
			Best = FMath::Max(Best, 10);
			bAny = true;
			continue;
		}
	}

	OutBestScore = Best;
	return bAny;
}

bool AARPlayerCharacterInvader::PickBestMatchingAbilityHandle(
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

bool AARPlayerCharacterInvader::ActivateAbilityByTag(FGameplayTag Tag, bool bAllowRemoteActivation)
{
	FGameplayTagContainer TagsToActivate;
	if (Tag.IsValid())
	{
		TagsToActivate.AddTag(Tag);
	}
	return ActivateAbilityByTags(TagsToActivate, bAllowRemoteActivation);
}

bool AARPlayerCharacterInvader::ActivateAbilityByTags(const FGameplayTagContainer& InTagsToActivate, bool bAllowRemoteActivation)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC) 
	{
		UE_LOG(ARLog, Error, TEXT("[ShipGAS] ActivateAbilityByTags failed: no ASC found."));
		return false;
	}
	if (InTagsToActivate.IsEmpty()) 
	{
		UE_LOG(ARLog, Error, TEXT("[ShipGAS] ActivateAbilityByTags failed: empty tag container."));
		return false;
	}

	FGameplayAbilitySpecHandle Handle;
	if (!PickBestMatchingAbilityHandle(ASC, InTagsToActivate, Handle))
	{
		UE_LOG(ARLog, Warning, TEXT("[ShipGAS] ActivateAbilityByTags: no matching ability for tags '%s'."), *InTagsToActivate.ToStringSimple());
		return false;
	}

	return ASC->TryActivateAbility(Handle, bAllowRemoteActivation);
}

int32 AARPlayerCharacterInvader::ActivateAllAbilitiesByTag(FGameplayTag Tag, bool bAllowRemoteActivation)
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

void AARPlayerCharacterInvader::CancelAbilityByTag(FGameplayTag Tag)
{
	FGameplayTagContainer TagsToCancel;
	if (Tag.IsValid())
	{
		TagsToCancel.AddTag(Tag);
	}
	CancelAbilityByTags(TagsToCancel);
}

void AARPlayerCharacterInvader::CancelAbilityByTags(const FGameplayTagContainer& InTagsToCancel)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC) return;
	if (InTagsToCancel.IsEmpty()) return;

	// Cancels any active abilities that have any of these tags
	FGameplayTagContainer CopyTags = InTagsToCancel;
	ASC->CancelAbilities(&CopyTags);
}

// Debug helper: logs all properties on a struct
void AARPlayerCharacterInvader::LogAllPropertiesOnStruct(const UScriptStruct* StructType)
{
	if (!StructType) return;

	UE_LOG(ARLog, VeryVerbose, TEXT("[ShipGAS] Properties on struct '%s':"), *StructType->GetName());
	for (TFieldIterator<FProperty> It(StructType); It; ++It)
	{
		FProperty* Prop = *It;
		if (Prop)
		{
			UE_LOG(ARLog, VeryVerbose, TEXT("[ShipGAS]   - %s (%s)"), *Prop->GetName(), *Prop->GetClass()->GetName());
		}
	}
}

void AARPlayerCharacterInvader::ApplyLoadoutTagsToASC(const FGameplayTagContainer& InLoadoutTags)
{
	if (!HasAuthority()) return;
	if (InLoadoutTags.IsEmpty()) return;

	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC)
	{
		UE_LOG(ARLog, Error, TEXT("[ShipGAS] ApplyLoadoutTagsToASC failed: no ASC found."));
		return;
	}

	ARPlayerCharacterInvaderLocal::AddRuntimeTags(ASC, InLoadoutTags, HasAuthority());
	AppliedLooseTags.AppendTags(InLoadoutTags);

	UE_LOG(ARLog, Verbose, TEXT("[ShipGAS] Mirrored %d loadout tags into ASC."), InLoadoutTags.Num());
}
