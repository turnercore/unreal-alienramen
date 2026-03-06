// Fill out your copyright notice in the Description page of Project Settings.


#include "AREnemyBase.h"
#include "AREnemyAIController.h"
#include "AREnemyAttributeSet.h"
#include "AREnemyIncomingDamageEffect.h"
#include "ARInvaderAIController.h"
#include "ARInvaderGameState.h"
#include "ARPlayerCharacterInvader.h"
#include "ARLog.h"

#include "AbilitySystemComponent.h"
#include "ARAbilitySet.h"
#include "ARAttributeSetCore.h"
#include "ARInvaderDirectorSettings.h"
#include "ContentLookupSubsystem.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/UnrealType.h"

namespace
{
	const FName DataDamageName(TEXT("Data.Damage"));
	constexpr float KillCreditInstigatorFallbackWindowSeconds = 3.0f;

	static bool ApplyDamageToActorViaGAS(AActor* Target, float Damage, AActor* Offender)
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

namespace AREnemyBaseInternal
{
	struct FAbilityEntryKey
	{
		TSubclassOf<UGameplayAbility> AbilityClass;
		FGameplayTag ActivationTag;
		int32 Level = 0;

		friend uint32 GetTypeHash(const FAbilityEntryKey& Key)
		{
			return HashCombine(HashCombine(GetTypeHash(Key.AbilityClass), GetTypeHash(Key.ActivationTag)), GetTypeHash(Key.Level));
		}

		bool operator==(const FAbilityEntryKey& Other) const
		{
			return AbilityClass == Other.AbilityClass
				&& ActivationTag == Other.ActivationTag
				&& Level == Other.Level;
		}
	};

	struct FEffectEntryKey
	{
		TSubclassOf<UGameplayEffect> EffectClass;
		float Level = 0.f;

		friend uint32 GetTypeHash(const FEffectEntryKey& Key)
		{
			return HashCombine(GetTypeHash(Key.EffectClass), GetTypeHash(Key.Level));
		}

		bool operator==(const FEffectEntryKey& Other) const
		{
			return EffectClass == Other.EffectClass && FMath::IsNearlyEqual(Level, Other.Level);
		}
	};

	static void AppendUniqueAbilityEntries(
		const TArray<FARAbilitySet_AbilityEntry>& SourceEntries,
		TArray<FARAbilitySet_AbilityEntry>& OutEntries,
		TSet<FAbilityEntryKey>& InOutSeen)
	{
		for (const FARAbilitySet_AbilityEntry& Entry : SourceEntries)
		{
			if (!Entry.Ability)
			{
				continue;
			}

			const FAbilityEntryKey Key{ Entry.Ability, Entry.ActivationTag, Entry.Level };
			if (InOutSeen.Contains(Key))
			{
				continue;
			}

			InOutSeen.Add(Key);
			OutEntries.Add(Entry);
		}
	}

	static void AppendUniqueEffectEntries(
		const TArray<FARAbilitySet_EffectEntry>& SourceEntries,
		TArray<FARAbilitySet_EffectEntry>& OutEntries,
		TSet<FEffectEntryKey>& InOutSeen)
	{
		for (const FARAbilitySet_EffectEntry& Entry : SourceEntries)
		{
			if (!Entry.Effect)
			{
				continue;
			}

			const FEffectEntryKey Key{ Entry.Effect, Entry.Level };
			if (InOutSeen.Contains(Key))
			{
				continue;
			}

			InOutSeen.Add(Key);
			OutEntries.Add(Entry);
		}
	}

	static const FAREnemyArchetypeAbilitySetEntry* ResolveBestArchetypeEntry(
		const TArray<FAREnemyArchetypeAbilitySetEntry>& Entries,
		const FGameplayTag& EnemyArchetypeTag)
	{
		if (!EnemyArchetypeTag.IsValid())
		{
			return nullptr;
		}

		const FAREnemyArchetypeAbilitySetEntry* Best = nullptr;
		int32 BestDepth = INDEX_NONE;

		for (const FAREnemyArchetypeAbilitySetEntry& Entry : Entries)
		{
			if (!Entry.EnemyArchetypeTag.IsValid() || Entry.AbilitySet.IsNull())
			{
				continue;
			}

			if (EnemyArchetypeTag.MatchesTagExact(Entry.EnemyArchetypeTag))
			{
				return &Entry;
			}

			if (!EnemyArchetypeTag.MatchesTag(Entry.EnemyArchetypeTag))
			{
				continue;
			}

			TArray<FString> TagSegments;
			Entry.EnemyArchetypeTag.ToString().ParseIntoArray(TagSegments, TEXT("."), true);
			const int32 Depth = TagSegments.Num();
			if (!Best || Depth > BestDepth)
			{
				Best = &Entry;
				BestDepth = Depth;
			}
		}

		return Best;
	}

	static void GrantAbilityEntries(
		UAbilitySystemComponent* ASC,
		const TArray<FARAbilitySet_AbilityEntry>& Entries,
		TArray<FGameplayAbilitySpecHandle>& OutGranted,
		TArray<FActiveGameplayEffectHandle>& OutApplied
	)
	{
		(void)OutApplied;
		if (!ASC)
		{
			return;
		}

		for (const FARAbilitySet_AbilityEntry& Entry : Entries)
		{
			if (!Entry.Ability)
			{
				continue;
			}

			FGameplayAbilitySpec Spec(Entry.Ability, Entry.Level);
			if (Entry.ActivationTag.IsValid())
			{
				Spec.GetDynamicSpecSourceTags().AddTag(Entry.ActivationTag);
			}

			OutGranted.Add(ASC->GiveAbility(Spec));
		}
	}

	static void ApplyEffectEntries(
		UAbilitySystemComponent* ASC,
		const TArray<FARAbilitySet_EffectEntry>& Entries,
		TArray<FActiveGameplayEffectHandle>& OutApplied)
	{
		if (!ASC)
		{
			return;
		}

		for (const FARAbilitySet_EffectEntry& Entry : Entries)
		{
			if (!Entry.Effect)
			{
				continue;
			}

			const FGameplayEffectContextHandle Ctx = ASC->MakeEffectContext();
			const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(Entry.Effect, Entry.Level, Ctx);
			if (Spec.IsValid())
			{
				OutApplied.Add(ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()));
			}
		}
	}
}

AAREnemyBase::AAREnemyBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bUseControllerRotationYaw = false;
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	AIControllerClass = AARInvaderAIController::StaticClass();

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->bOrientRotationToMovement = false;
		MoveComp->RotationRate = FRotator(0.f, 640.f, 0.f);
		MoveComp->GravityScale = 0.f;
	}

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	StateTreeASC = AbilitySystemComponent;

	AttributeSetCore = CreateDefaultSubobject<UARAttributeSetCore>(TEXT("AttributeSetCore"));
	EnemyAttributeSet = CreateDefaultSubobject<UAREnemyAttributeSet>(TEXT("EnemyAttributeSet"));
}

UAbilitySystemComponent* AAREnemyBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AAREnemyBase::BeginPlay()
{
	Super::BeginPlay();
	BindHealthChangeDelegate();
	BindMoveSpeedChangeDelegate();
	BindEnemyColorTagDelegates();
	RefreshCharacterMovementSpeedFromAttributes();
}

void AAREnemyBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindHealthChangeDelegate();
	UnbindMoveSpeedChangeDelegate();
	UnbindEnemyColorTagDelegates();
	Super::EndPlay(EndPlayReason);
}

void AAREnemyBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AAREnemyBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	InitAbilityActorInfo();

	if (HasAuthority())
	{
		InitializeFromEnemyDefinitionTag();
		ApplyStartupAbilitySet();
	}

	TryDispatchWavePhaseEvent();
	TryDispatchEnteredScreenEvent();
	TryDispatchInFormationEvent();
	BP_OnEnemyInitialized();
	UE_LOG(ARLog, Log, TEXT("[EnemyBase] Possessed '%s' by '%s'."),
		*GetNameSafe(this), *GetNameSafe(NewController));
}

void AAREnemyBase::OnRep_Controller()
{
	Super::OnRep_Controller();
	InitAbilityActorInfo();
}

void AAREnemyBase::UnPossessed()
{
	if (HasAuthority())
	{
		ClearASCStateTags();
		ClearStartupAbilitySet();
		ClearRuntimeEnemyEffects();
		ClearRuntimeEnemyTags();
	}

	Super::UnPossessed();
}

void AAREnemyBase::InitAbilityActorInfo()
{
	if (!AbilitySystemComponent)
	{
		UE_LOG(ARLog, Error, TEXT("[EnemyBase] InitAbilityActorInfo failed: no AbilitySystemComponent on '%s'."), *GetNameSafe(this));
		return;
	}

	// Enemy-owned ASC: owner and avatar are this pawn.
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	BindHealthChangeDelegate();
	BindMoveSpeedChangeDelegate();
	BindEnemyColorTagDelegates();
	if (HasAuthority())
	{
		ApplyEnemyColorGameplayTags(EnemyColor);
	}
	RefreshCharacterMovementSpeedFromAttributes();
	UE_LOG(ARLog, Log, TEXT("[EnemyBase] ASC initialized for '%s'."), *GetNameSafe(this));
}

void AAREnemyBase::ApplyStartupAbilitySet()
{
	if (!HasAuthority() || !AbilitySystemComponent || bStartupSetApplied)
	{
		return;
	}

	TArray<FARAbilitySet_AbilityEntry> AggregatedAbilities;
	TArray<FARAbilitySet_EffectEntry> AggregatedEffects;
	TSet<AREnemyBaseInternal::FAbilityEntryKey> SeenAbilityEntries;
	TSet<AREnemyBaseInternal::FEffectEntryKey> SeenEffectEntries;

	auto AppendAbilitySetData = [&AggregatedAbilities, &AggregatedEffects, &SeenAbilityEntries, &SeenEffectEntries](const UARAbilitySet* Set)
	{
		if (!Set)
		{
			return;
		}

		AREnemyBaseInternal::AppendUniqueAbilityEntries(Set->Abilities, AggregatedAbilities, SeenAbilityEntries);
		AREnemyBaseInternal::AppendUniqueEffectEntries(Set->StartupEffects, AggregatedEffects, SeenEffectEntries);
	};

	const UARInvaderDirectorSettings* DirectorSettings = GetDefault<UARInvaderDirectorSettings>();
	if (DirectorSettings)
	{
		AppendAbilitySetData(DirectorSettings->EnemyCommonAbilitySet.LoadSynchronous());

		if (const FAREnemyArchetypeAbilitySetEntry* ArchetypeEntry =
			AREnemyBaseInternal::ResolveBestArchetypeEntry(DirectorSettings->EnemyArchetypeAbilitySets, EnemyArchetypeTag))
		{
			AppendAbilitySetData(ArchetypeEntry->AbilitySet.LoadSynchronous());
		}
	}

	AREnemyBaseInternal::AppendUniqueAbilityEntries(RuntimeSpecificAbilities, AggregatedAbilities, SeenAbilityEntries);

	if (AggregatedAbilities.IsEmpty() && AggregatedEffects.IsEmpty())
	{
		return;
	}

	AREnemyBaseInternal::GrantAbilityEntries(AbilitySystemComponent, AggregatedAbilities, StartupGrantedAbilityHandles, StartupAppliedEffectHandles);
	AREnemyBaseInternal::ApplyEffectEntries(AbilitySystemComponent, AggregatedEffects, StartupAppliedEffectHandles);
	bStartupSetApplied = true;

	UE_LOG(ARLog, Log, TEXT("[EnemyBase] Applied startup enemy abilities to '%s' (Abilities=%d Effects=%d)."),
		*GetNameSafe(this), StartupGrantedAbilityHandles.Num(), StartupAppliedEffectHandles.Num());
}

void AAREnemyBase::ApplyRuntimeEnemyEffects(const TArray<TSubclassOf<UGameplayEffect>>& Effects)
{
	if (!HasAuthority() || !AbilitySystemComponent)
	{
		return;
	}

	for (const TSubclassOf<UGameplayEffect>& EffectClass : Effects)
	{
		if (!EffectClass)
		{
			continue;
		}

		const FGameplayEffectContextHandle Ctx = AbilitySystemComponent->MakeEffectContext();
		const FGameplayEffectSpecHandle Spec = AbilitySystemComponent->MakeOutgoingSpec(EffectClass, 1.f, Ctx);
		if (Spec.IsValid())
		{
			RuntimeAppliedEffectHandles.Add(AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()));
		}
	}
}

void AAREnemyBase::ClearRuntimeEnemyEffects()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	for (const FActiveGameplayEffectHandle& Handle : RuntimeAppliedEffectHandles)
	{
		if (Handle.IsValid())
		{
			AbilitySystemComponent->RemoveActiveGameplayEffect(Handle);
		}
	}

	RuntimeAppliedEffectHandles.Reset();
}

void AAREnemyBase::ApplyRuntimeEnemyTags(const FGameplayTagContainer& InTags)
{
	if (!HasAuthority() || !AbilitySystemComponent || InTags.IsEmpty())
	{
		return;
	}

	AbilitySystemComponent->AddLooseGameplayTags(InTags, 1, EGameplayTagReplicationState::TagOnly);
	RuntimeAppliedLooseTags.AppendTags(InTags);
}

void AAREnemyBase::ClearRuntimeEnemyTags()
{
	if (!AbilitySystemComponent || RuntimeAppliedLooseTags.IsEmpty())
	{
		return;
	}

	AbilitySystemComponent->RemoveLooseGameplayTags(RuntimeAppliedLooseTags, 1, EGameplayTagReplicationState::TagOnly);
	RuntimeAppliedLooseTags.Reset();
}

bool AAREnemyBase::ResolveEnemyDefinition(FARInvaderEnemyDefRow& OutRow, FString& OutError) const
{
	OutRow = FARInvaderEnemyDefRow();
	OutError.Reset();

	if (!EnemyIdentifierTag.IsValid())
	{
		OutError = TEXT("EnemyIdentifierTag is invalid.");
		return false;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		OutError = TEXT("No world.");
		return false;
	}

	UGameInstance* GI = World->GetGameInstance();
	if (!GI)
	{
		OutError = TEXT("No game instance.");
		return false;
	}

	UContentLookupSubsystem* Lookup = GI->GetSubsystem<UContentLookupSubsystem>();
	if (!Lookup)
	{
		OutError = TEXT("No content lookup subsystem.");
		return false;
	}

	FInstancedStruct ResolvedRow;
	if (!Lookup->LookupWithGameplayTag(EnemyIdentifierTag, ResolvedRow, OutError))
	{
		return false;
	}

	if (!ResolvedRow.IsValid())
	{
		OutError = TEXT("Resolved row struct is invalid.");
		return false;
	}

	const UScriptStruct* RowType = ResolvedRow.GetScriptStruct();
	const void* RowData = ResolvedRow.GetMemory();
	if (!RowType || !RowData)
	{
		OutError = TEXT("Resolved row has no struct data.");
		return false;
	}

	if (RowType == FARInvaderEnemyDefRow::StaticStruct())
	{
		OutRow = *static_cast<const FARInvaderEnemyDefRow*>(RowData);
	}
	else
	{
		// Backward-compatible extraction for legacy BP row structs (maxHp + Blueprint).
		const FProperty* MaxHealthProp = RowType->FindPropertyByName(TEXT("maxHp"));
		if (const FNumericProperty* NumProp = CastField<FNumericProperty>(MaxHealthProp))
		{
			const void* ValuePtr = NumProp->ContainerPtrToValuePtr<void>(RowData);
			OutRow.RuntimeInit.MaxHealth = static_cast<float>(NumProp->GetFloatingPointPropertyValue(ValuePtr));
		}

		const FProperty* BPProp = RowType->FindPropertyByName(TEXT("Blueprint"));
		if (const FSoftClassProperty* SoftClassProp = CastField<FSoftClassProperty>(BPProp))
		{
			const FSoftObjectPtr SoftClassObj = SoftClassProp->GetPropertyValue_InContainer(RowData);
			OutRow.EnemyClass = TSoftClassPtr<AAREnemyBase>(SoftClassObj.ToSoftObjectPath());
		}
		else if (const FClassProperty* ClassProp = CastField<FClassProperty>(BPProp))
		{
			OutRow.EnemyClass = Cast<UClass>(ClassProp->GetPropertyValue_InContainer(RowData));
		}

		OutRow.EnemyIdentifierTag = EnemyIdentifierTag;
		OutRow.bEnabled = true;
	}

	if (!OutRow.EnemyIdentifierTag.IsValid())
	{
		OutRow.EnemyIdentifierTag = EnemyIdentifierTag;
	}
	if (OutRow.RuntimeInit.MaxHealth <= 0.f)
	{
		OutRow.RuntimeInit.MaxHealth = 100.f;
	}

	return true;
}

void AAREnemyBase::ApplyEnemyRuntimeInitData(const FARInvaderEnemyRuntimeInitData& RuntimeInit)
{
	if (!HasAuthority() || !AbilitySystemComponent)
	{
		return;
	}

	AbilitySystemComponent->SetNumericAttributeBase(UARAttributeSetCore::GetMaxHealthAttribute(), RuntimeInit.MaxHealth);
	AbilitySystemComponent->SetNumericAttributeBase(UARAttributeSetCore::GetHealthAttribute(), RuntimeInit.MaxHealth);
	AbilitySystemComponent->SetNumericAttributeBase(UARAttributeSetCore::GetDamageAttribute(), RuntimeInit.Damage);
	AbilitySystemComponent->SetNumericAttributeBase(UARAttributeSetCore::GetMoveSpeedAttribute(), RuntimeInit.MoveSpeed);
	AbilitySystemComponent->SetNumericAttributeBase(UARAttributeSetCore::GetFireRateAttribute(), RuntimeInit.FireRate);
	AbilitySystemComponent->SetNumericAttributeBase(UARAttributeSetCore::GetDamageTakenMultiplierAttribute(), RuntimeInit.DamageTakenMultiplier);
	AbilitySystemComponent->SetNumericAttributeBase(UAREnemyAttributeSet::GetCollisionDamageAttribute(), RuntimeInit.CollisionDamage);
	RefreshCharacterMovementSpeedFromAttributes();

	EnemyArchetypeTag = RuntimeInit.EnemyArchetypeTag;
	RuntimeSpecificAbilities = RuntimeInit.EnemySpecificAbilities;

	ClearRuntimeEnemyEffects();
	ClearRuntimeEnemyTags();
	ApplyRuntimeEnemyEffects(RuntimeInit.StartupGameplayEffects);
	ApplyRuntimeEnemyTags(RuntimeInit.StartupLooseTags);
}

bool AAREnemyBase::InitializeFromEnemyDefinitionTag()
{
	if (!HasAuthority())
	{
		return false;
	}

	FARInvaderEnemyDefRow EnemyDef;
	FString Error;
	if (!ResolveEnemyDefinition(EnemyDef, Error))
	{
		UE_LOG(ARLog, Warning, TEXT("[EnemyBase] Failed to resolve enemy definition for '%s': %s"),
			*GetNameSafe(this), *Error);
		return false;
	}

	if (!EnemyDef.bEnabled)
	{
		UE_LOG(ARLog, Warning, TEXT("[EnemyBase] Enemy definition '%s' is disabled for '%s'."),
			*EnemyDef.EnemyIdentifierTag.ToString(), *GetNameSafe(this));
	}

	ApplyEnemyRuntimeInitData(EnemyDef.RuntimeInit);
	ForceNetUpdate();
	BP_OnEnemyDefinitionApplied();
	return true;
}

void AAREnemyBase::ClearStartupAbilitySet()
{
	if (!AbilitySystemComponent || !bStartupSetApplied)
	{
		return;
	}

	for (const FGameplayAbilitySpecHandle& Handle : StartupGrantedAbilityHandles)
	{
		if (Handle.IsValid())
		{
			AbilitySystemComponent->ClearAbility(Handle);
		}
	}

	for (const FActiveGameplayEffectHandle& Handle : StartupAppliedEffectHandles)
	{
		if (Handle.IsValid())
		{
			AbilitySystemComponent->RemoveActiveGameplayEffect(Handle);
		}
	}

	StartupGrantedAbilityHandles.Reset();
	StartupAppliedEffectHandles.Reset();
	bStartupSetApplied = false;
}

void AAREnemyBase::BindHealthChangeDelegate()
{
	if (!AbilitySystemComponent || HealthChangedDelegateHandle.IsValid())
	{
		return;
	}

	HealthChangedDelegateHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UARAttributeSetCore::GetHealthAttribute())
		.AddUObject(this, &AAREnemyBase::OnHealthChanged);
}

void AAREnemyBase::UnbindHealthChangeDelegate()
{
	if (!AbilitySystemComponent || !HealthChangedDelegateHandle.IsValid())
	{
		return;
	}

	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UARAttributeSetCore::GetHealthAttribute())
		.Remove(HealthChangedDelegateHandle);
	HealthChangedDelegateHandle.Reset();
}

void AAREnemyBase::OnHealthChanged(const FOnAttributeChangeData& ChangeData)
{
	if (!HasAuthority() || bIsDead)
	{
		return;
	}

	if (ChangeData.NewValue <= 0.f)
	{
		AActor* DamageInstigator = nullptr;
		if (ChangeData.GEModData)
		{
			const FGameplayEffectContextHandle EffectContext = ChangeData.GEModData->EffectSpec.GetContext();
			DamageInstigator = EffectContext.GetOriginalInstigator();
			if (!DamageInstigator)
			{
				DamageInstigator = EffectContext.GetEffectCauser();
			}
		}
		if (!DamageInstigator)
		{
			DamageInstigator = ResolveRecentDamageInstigatorForKillCredit(KillCreditInstigatorFallbackWindowSeconds);
		}

		UE_LOG(ARLog, Verbose, TEXT("[EnemyBase] Health reached zero for '%s'. Resolved damage instigator '%s' (recentFallback=%d)."),
			*GetNameSafe(this), *GetNameSafe(DamageInstigator), DamageInstigator ? 1 : 0);
		HandleDeath(DamageInstigator);
	}
}

void AAREnemyBase::BindMoveSpeedChangeDelegate()
{
	if (!AbilitySystemComponent || MoveSpeedChangedDelegateHandle.IsValid())
	{
		return;
	}

	MoveSpeedChangedDelegateHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UARAttributeSetCore::GetMoveSpeedAttribute())
		.AddUObject(this, &AAREnemyBase::OnMoveSpeedChanged);
}

void AAREnemyBase::UnbindMoveSpeedChangeDelegate()
{
	if (!AbilitySystemComponent || !MoveSpeedChangedDelegateHandle.IsValid())
	{
		return;
	}

	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UARAttributeSetCore::GetMoveSpeedAttribute())
		.Remove(MoveSpeedChangedDelegateHandle);
	MoveSpeedChangedDelegateHandle.Reset();
}

void AAREnemyBase::OnMoveSpeedChanged(const FOnAttributeChangeData& ChangeData)
{
	(void)ChangeData;
	RefreshCharacterMovementSpeedFromAttributes();
}

void AAREnemyBase::BindEnemyColorTagDelegates()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	if (!ColorNoneTagChangedDelegateHandle.IsValid())
	{
		const FGameplayTag ColorNoneTag = FGameplayTag::RequestGameplayTag(TEXT("Color.None"), false);
		if (ColorNoneTag.IsValid())
		{
			ColorNoneTagChangedDelegateHandle = AbilitySystemComponent->RegisterGameplayTagEvent(ColorNoneTag, EGameplayTagEventType::NewOrRemoved)
				.AddUObject(this, &AAREnemyBase::HandleEnemyColorOverrideTagChanged);
		}
	}

	if (!ColorRedTagChangedDelegateHandle.IsValid())
	{
		const FGameplayTag ColorRedTag = FGameplayTag::RequestGameplayTag(TEXT("Color.Red"), false);
		if (ColorRedTag.IsValid())
		{
			ColorRedTagChangedDelegateHandle = AbilitySystemComponent->RegisterGameplayTagEvent(ColorRedTag, EGameplayTagEventType::NewOrRemoved)
				.AddUObject(this, &AAREnemyBase::HandleEnemyColorOverrideTagChanged);
		}
	}

	if (!ColorWhiteTagChangedDelegateHandle.IsValid())
	{
		const FGameplayTag ColorWhiteTag = FGameplayTag::RequestGameplayTag(TEXT("Color.White"), false);
		if (ColorWhiteTag.IsValid())
		{
			ColorWhiteTagChangedDelegateHandle = AbilitySystemComponent->RegisterGameplayTagEvent(ColorWhiteTag, EGameplayTagEventType::NewOrRemoved)
				.AddUObject(this, &AAREnemyBase::HandleEnemyColorOverrideTagChanged);
		}
	}

	if (!ColorBlueTagChangedDelegateHandle.IsValid())
	{
		const FGameplayTag ColorBlueTag = FGameplayTag::RequestGameplayTag(TEXT("Color.Blue"), false);
		if (ColorBlueTag.IsValid())
		{
			ColorBlueTagChangedDelegateHandle = AbilitySystemComponent->RegisterGameplayTagEvent(ColorBlueTag, EGameplayTagEventType::NewOrRemoved)
				.AddUObject(this, &AAREnemyBase::HandleEnemyColorOverrideTagChanged);
		}
	}

	EvaluateEnemyColorFromASCOverrideTags();
}

void AAREnemyBase::UnbindEnemyColorTagDelegates()
{
	if (!AbilitySystemComponent)
	{
		ColorNoneTagChangedDelegateHandle.Reset();
		ColorRedTagChangedDelegateHandle.Reset();
		ColorWhiteTagChangedDelegateHandle.Reset();
		ColorBlueTagChangedDelegateHandle.Reset();
		return;
	}

	const FGameplayTag ColorNoneTag = FGameplayTag::RequestGameplayTag(TEXT("Color.None"), false);
	if (ColorNoneTag.IsValid() && ColorNoneTagChangedDelegateHandle.IsValid())
	{
		AbilitySystemComponent->RegisterGameplayTagEvent(ColorNoneTag, EGameplayTagEventType::NewOrRemoved).Remove(ColorNoneTagChangedDelegateHandle);
		ColorNoneTagChangedDelegateHandle.Reset();
	}

	const FGameplayTag ColorRedTag = FGameplayTag::RequestGameplayTag(TEXT("Color.Red"), false);
	if (ColorRedTag.IsValid() && ColorRedTagChangedDelegateHandle.IsValid())
	{
		AbilitySystemComponent->RegisterGameplayTagEvent(ColorRedTag, EGameplayTagEventType::NewOrRemoved).Remove(ColorRedTagChangedDelegateHandle);
		ColorRedTagChangedDelegateHandle.Reset();
	}

	const FGameplayTag ColorWhiteTag = FGameplayTag::RequestGameplayTag(TEXT("Color.White"), false);
	if (ColorWhiteTag.IsValid() && ColorWhiteTagChangedDelegateHandle.IsValid())
	{
		AbilitySystemComponent->RegisterGameplayTagEvent(ColorWhiteTag, EGameplayTagEventType::NewOrRemoved).Remove(ColorWhiteTagChangedDelegateHandle);
		ColorWhiteTagChangedDelegateHandle.Reset();
	}

	const FGameplayTag ColorBlueTag = FGameplayTag::RequestGameplayTag(TEXT("Color.Blue"), false);
	if (ColorBlueTag.IsValid() && ColorBlueTagChangedDelegateHandle.IsValid())
	{
		AbilitySystemComponent->RegisterGameplayTagEvent(ColorBlueTag, EGameplayTagEventType::NewOrRemoved).Remove(ColorBlueTagChangedDelegateHandle);
		ColorBlueTagChangedDelegateHandle.Reset();
	}
}

void AAREnemyBase::HandleEnemyColorOverrideTagChanged(const FGameplayTag /*Tag*/, const int32 /*NewCount*/)
{
	if (bApplyingEnemyColorTags)
	{
		return;
	}

	EvaluateEnemyColorFromASCOverrideTags();
}

void AAREnemyBase::EvaluateEnemyColorFromASCOverrideTags()
{
	if (!HasAuthority() || !AbilitySystemComponent)
	{
		return;
	}

	const EARAffinityColor ResolvedColor = ResolveEnemyColorFromASCOverrideTags();
	bUpdatingEnemyColorFromTags = true;
	SetEnemyColor(ResolvedColor);
	bUpdatingEnemyColorFromTags = false;
}

EARAffinityColor AAREnemyBase::ResolveEnemyColorFromASCOverrideTags() const
{
	if (!AbilitySystemComponent)
	{
		return EnemyColor;
	}

	const FGameplayTag ColorNoneTag = FGameplayTag::RequestGameplayTag(TEXT("Color.None"), false);
	const FGameplayTag ColorWhiteTag = FGameplayTag::RequestGameplayTag(TEXT("Color.White"), false);
	const FGameplayTag ColorRedTag = FGameplayTag::RequestGameplayTag(TEXT("Color.Red"), false);
	const FGameplayTag ColorBlueTag = FGameplayTag::RequestGameplayTag(TEXT("Color.Blue"), false);

	// Override precedence matches player logic.
	if (ColorNoneTag.IsValid() && AbilitySystemComponent->HasMatchingGameplayTag(ColorNoneTag))
	{
		return EARAffinityColor::None;
	}

	if (ColorWhiteTag.IsValid() && AbilitySystemComponent->HasMatchingGameplayTag(ColorWhiteTag))
	{
		return EARAffinityColor::White;
	}

	if (ColorRedTag.IsValid() && AbilitySystemComponent->HasMatchingGameplayTag(ColorRedTag))
	{
		return EARAffinityColor::Red;
	}

	if (ColorBlueTag.IsValid() && AbilitySystemComponent->HasMatchingGameplayTag(ColorBlueTag))
	{
		return EARAffinityColor::Blue;
	}

	return EnemyColor;
}

void AAREnemyBase::ApplyEnemyColorGameplayTags(const EARAffinityColor NewColor)
{
	if (!HasAuthority() || !AbilitySystemComponent || bApplyingEnemyColorTags)
	{
		return;
	}

	const FGameplayTag ColorNoneTag = FGameplayTag::RequestGameplayTag(TEXT("Color.None"), false);
	const FGameplayTag ColorWhiteTag = FGameplayTag::RequestGameplayTag(TEXT("Color.White"), false);
	const FGameplayTag ColorRedTag = FGameplayTag::RequestGameplayTag(TEXT("Color.Red"), false);
	const FGameplayTag ColorBlueTag = FGameplayTag::RequestGameplayTag(TEXT("Color.Blue"), false);

	FGameplayTagContainer AllColorTags;
	if (ColorNoneTag.IsValid()) { AllColorTags.AddTag(ColorNoneTag); }
	if (ColorWhiteTag.IsValid()) { AllColorTags.AddTag(ColorWhiteTag); }
	if (ColorRedTag.IsValid()) { AllColorTags.AddTag(ColorRedTag); }
	if (ColorBlueTag.IsValid()) { AllColorTags.AddTag(ColorBlueTag); }

	if (AllColorTags.IsEmpty())
	{
		return;
	}

	bApplyingEnemyColorTags = true;
	AbilitySystemComponent->RemoveLooseGameplayTags(AllColorTags, 1, EGameplayTagReplicationState::TagOnly);

	FGameplayTagContainer ActiveColorTag;
	switch (NewColor)
	{
	case EARAffinityColor::None:
		if (ColorNoneTag.IsValid()) { ActiveColorTag.AddTag(ColorNoneTag); }
		break;
	case EARAffinityColor::White:
		if (ColorWhiteTag.IsValid()) { ActiveColorTag.AddTag(ColorWhiteTag); }
		break;
	case EARAffinityColor::Red:
		if (ColorRedTag.IsValid()) { ActiveColorTag.AddTag(ColorRedTag); }
		break;
	case EARAffinityColor::Blue:
		if (ColorBlueTag.IsValid()) { ActiveColorTag.AddTag(ColorBlueTag); }
		break;
	default:
		break;
	}

	if (!ActiveColorTag.IsEmpty())
	{
		AbilitySystemComponent->AddLooseGameplayTags(ActiveColorTag, 1, EGameplayTagReplicationState::TagOnly);
	}
	bApplyingEnemyColorTags = false;
}

void AAREnemyBase::RefreshCharacterMovementSpeedFromAttributes()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	if (!MoveComp)
	{
		return;
	}

	const float MoveSpeed = FMath::Max(0.f, AbilitySystemComponent->GetNumericAttribute(UARAttributeSetCore::GetMoveSpeedAttribute()));
	MoveComp->MaxWalkSpeed = MoveSpeed;
	MoveComp->MaxFlySpeed = MoveSpeed;
}

void AAREnemyBase::HandleDeath_Implementation(AActor* InstigatorActor)
{
	if (!HasAuthority() || bIsDead)
	{
		return;
	}

	if (!InstigatorActor)
	{
		InstigatorActor = ResolveRecentDamageInstigatorForKillCredit(KillCreditInstigatorFallbackWindowSeconds);
	}

	bIsDead = true;
	ForceNetUpdate();

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->CancelAllAbilities();
	}

	if (AController* PawnController = GetController())
	{
		PawnController->StopMovement();
		if (AAIController* AIController = Cast<AAIController>(PawnController))
		{
			if (UBrainComponent* Brain = AIController->GetBrainComponent())
			{
				Brain->StopLogic(TEXT("Enemy died"));
			}
		}
	}

	if (InstigatorActor)
	{
		UE_LOG(ARLog, Log, TEXT("[EnemyBase] Death handled for '%s'. Instigator='%s'."),
			*GetNameSafe(this), *GetNameSafe(InstigatorActor));
	}
	else
	{
		UE_LOG(ARLog, Warning, TEXT("[EnemyBase] Death handled for '%s' with missing instigator."),
			*GetNameSafe(this));
	}

	if (AARInvaderGameState* InvaderGameState = GetWorld() ? GetWorld()->GetGameState<AARInvaderGameState>() : nullptr)
	{
		InvaderGameState->NotifyEnemyKilled(this, InstigatorActor);
	}

	BP_OnEnemyDied(InstigatorActor);
	BP_OnEnemyPreRelease(InstigatorActor);
	ReleaseEnemyActor();
}

void AAREnemyBase::ReleaseEnemyActor_Implementation()
{
	Destroy();
}

void AAREnemyBase::OnRep_IsDead()
{
	if (bIsDead)
	{
		BP_OnEnemyDied(nullptr);
	}
}

void AAREnemyBase::SetEnemyColor(EARAffinityColor InColor)
{
	if (!HasAuthority())
	{
		return;
	}

	if (InColor == EARAffinityColor::Unknown)
	{
		InColor = EARAffinityColor::None;
	}

	if (!bUpdatingEnemyColorFromTags)
	{
		ApplyEnemyColorGameplayTags(InColor);
	}

	if (EnemyColor == InColor)
	{
		return;
	}

	const EARAffinityColor OldColor = EnemyColor;
	EnemyColor = InColor;
	ForceNetUpdate();
	UE_LOG(ARLog, Verbose, TEXT("[EnemyBase] Set enemy color for '%s' -> %d."), *GetNameSafe(this), static_cast<int32>(EnemyColor));
	BP_OnEnemyColorChanged(EnemyColor);

	if (EnemyColor == EARAffinityColor::None || EnemyColor == EARAffinityColor::White)
	{
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[EnemyBase|Color] Enemy '%s' entered non-baseline color %d (Old=%d Identifier=%s)."),
			*GetNameSafe(this),
			static_cast<int32>(EnemyColor),
			static_cast<int32>(OldColor),
			*EnemyIdentifierTag.ToString());
	}
}

void AAREnemyBase::SetEnemyIdentifierTag(FGameplayTag InIdentifierTag)
{
	if (!HasAuthority())
	{
		return;
	}

	EnemyIdentifierTag = InIdentifierTag;
	ForceNetUpdate();
	BP_OnEnemyIdentifierTagChanged(EnemyIdentifierTag);
}

bool AAREnemyBase::ApplyDamageViaGAS(float Damage, AActor* Offender, float& OutCurrentHealth)
{
	OutCurrentHealth = AbilitySystemComponent
		? AbilitySystemComponent->GetNumericAttribute(UARAttributeSetCore::GetHealthAttribute())
		: 0.f;

	if (!HasAuthority() || bIsDead || Damage <= 0.f || !AbilitySystemComponent)
	{
		return false;
	}

	RememberDamageInstigatorForKillCredit(Offender);

	FGameplayEffectContextHandle Context = AbilitySystemComponent->MakeEffectContext();
	if (Offender)
	{
		Context.AddInstigator(Offender, Offender);
	}

	const FGameplayEffectSpecHandle Spec = AbilitySystemComponent->MakeOutgoingSpec(UAREnemyIncomingDamageEffect::StaticClass(), 1.f, Context);
	if (!Spec.IsValid())
	{
		return false;
	}

	Spec.Data->SetSetByCallerMagnitude(DataDamageName, Damage);
	AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());

	OutCurrentHealth = AbilitySystemComponent->GetNumericAttribute(UARAttributeSetCore::GetHealthAttribute());
	if (!bIsDead && OutCurrentHealth <= 0.f)
	{
		HandleDeath(Offender);
	}
	return true;
}

bool AAREnemyBase::ApplyDamageViaGAS(float Damage, AActor* Offender)
{
	float IgnoredCurrentHealth = 0.f;
	return ApplyDamageViaGAS(Damage, Offender, IgnoredCurrentHealth);
}

float AAREnemyBase::GetCurrentDamageFromGAS() const
{
	if (!AbilitySystemComponent)
	{
		return 0.f;
	}

	return AbilitySystemComponent->GetNumericAttribute(UARAttributeSetCore::GetDamageAttribute());
}

float AAREnemyBase::GetCurrentCollisionDamageFromGAS() const
{
	if (!AbilitySystemComponent)
	{
		return 0.f;
	}

	return AbilitySystemComponent->GetNumericAttribute(UAREnemyAttributeSet::GetCollisionDamageAttribute());
}

bool AAREnemyBase::ApplyDamageToTargetViaGAS(AActor* Target, float DamageOverride)
{
	if (!HasAuthority())
	{
		return false;
	}

	const float DamageToApply = (DamageOverride >= 0.f) ? DamageOverride : GetCurrentDamageFromGAS();
	return ApplyDamageToActorViaGAS(Target, DamageToApply, this);
}

bool AAREnemyBase::ApplyCollisionDamageToTargetViaGAS(AActor* Target, float DamageOverride)
{
	if (!HasAuthority())
	{
		return false;
	}

	const float DamageToApply = (DamageOverride >= 0.f) ? DamageOverride : GetCurrentCollisionDamageFromGAS();
	return ApplyDamageToActorViaGAS(Target, DamageToApply, this);
}

float AAREnemyBase::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	(void)DamageEvent;

	AActor* EffectiveOffender = DamageCauser;
	if (!EffectiveOffender && EventInstigator)
	{
		EffectiveOffender = EventInstigator->GetPawn();
		if (!EffectiveOffender)
		{
			EffectiveOffender = EventInstigator;
		}
	}

	float IgnoredCurrentHealth = 0.f;
	return ApplyDamageViaGAS(DamageAmount, EffectiveOffender, IgnoredCurrentHealth) ? DamageAmount : 0.f;
}

AActor* AAREnemyBase::ResolveRecentDamageInstigatorForKillCredit(const float MaxAgeSeconds) const
{
	if (!HasAuthority())
	{
		return nullptr;
	}

	AActor* Candidate = LastDamageInstigatorActor.Get();
	if (!Candidate)
	{
		return nullptr;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	const float AgeSeconds = World->GetTimeSeconds() - LastDamageInstigatorServerTime;
	if (AgeSeconds > FMath::Max(0.0f, MaxAgeSeconds))
	{
		return nullptr;
	}

	return Candidate;
}

void AAREnemyBase::RememberDamageInstigatorForKillCredit(AActor* Offender)
{
	if (!HasAuthority() || !Offender)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	LastDamageInstigatorActor = Offender;
	LastDamageInstigatorServerTime = World->GetTimeSeconds();
}

void AAREnemyBase::OnRep_EnemyColor()
{
	BP_OnEnemyColorChanged(EnemyColor);
}

void AAREnemyBase::OnRep_EnemyIdentifierTag()
{
	BP_OnEnemyIdentifierTagChanged(EnemyIdentifierTag);
}

void AAREnemyBase::SetWaveRuntimeContext(
	int32 InWaveInstanceId,
	int32 InFormationSlotIndex,
	EARWavePhase InWavePhase,
	float InPhaseStartServerTime,
	bool bInFormationLockEnter,
	bool bInFormationLockActive)
{
	if (!HasAuthority())
	{
		return;
	}

	const EARWavePhase PreviousPhase = WavePhase;
	const int32 PreviousWaveInstanceId = WaveInstanceId;
	WaveInstanceId = InWaveInstanceId;
	FormationSlotIndex = InFormationSlotIndex;
	WavePhase = InWavePhase;
	WavePhaseStartServerTime = InPhaseStartServerTime;
	bFormationLockEnter = bInFormationLockEnter;
	bFormationLockActive = bInFormationLockActive;
	bHasEnteredScreen = false;
	bHasEnteredGameplayScreen = false;
	bReachedFormationSlot = false;
	bHasDispatchedEnteredScreenEvent = false;
	bHasDispatchedInFormationEvent = false;
	LastDispatchedWavePhaseWaveId = INDEX_NONE;
	UE_LOG(
		ARLog,
		Log,
		TEXT("[EnemyBase|WaveCtx] Enemy='%s' WaveId=%d LockEnter=%d LockActive=%d Phase=%d"),
		*GetNameSafe(this),
		WaveInstanceId,
		bFormationLockEnter ? 1 : 0,
		bFormationLockActive ? 1 : 0,
		static_cast<int32>(WavePhase));
	ForceNetUpdate();

	if (PreviousPhase != WavePhase || PreviousWaveInstanceId != WaveInstanceId)
	{
		TryDispatchWavePhaseEvent();
	}
	TryDispatchEnteredScreenEvent();
	TryDispatchInFormationEvent();

	// Context assignment is the authoritative signal that AI can start safely.
	if (AAREnemyAIController* EnemyAI = Cast<AAREnemyAIController>(GetController()))
	{
		EnemyAI->TryStartStateTreeForCurrentPawn(TEXT("WaveRuntimeContextAssigned"));
	}
	else
	{
		UE_LOG(ARLog, Warning, TEXT("[EnemyBase|WaveCtx] Enemy='%s' has no AAREnemyAIController at context assignment; StateTree start deferred."),
			*GetNameSafe(this));
	}
}

void AAREnemyBase::SetWavePhase(EARWavePhase InWavePhase, float InPhaseStartServerTime)
{
	if (!HasAuthority())
	{
		return;
	}

	WavePhase = InWavePhase;
	WavePhaseStartServerTime = InPhaseStartServerTime;
	ForceNetUpdate();
	TryDispatchWavePhaseEvent();
}

void AAREnemyBase::SetFormationTargetWorldLocation(const FVector& InFormationTargetWorldLocation)
{
	if (!HasAuthority())
	{
		return;
	}

	FormationTargetWorldLocation = InFormationTargetWorldLocation;
	bHasFormationTargetWorldLocation = true;
	ForceNetUpdate();
}

void AAREnemyBase::NotifyEnteredGameplayScreen(float InServerTime)
{
	if (!HasAuthority() || bHasEnteredGameplayScreen)
	{
		return;
	}

	bHasEnteredGameplayScreen = true;
	bHasEnteredScreen = true;
	EnteredGameplayScreenServerTime = InServerTime;
	TryDispatchEnteredScreenEvent();
	TryDispatchInFormationEvent();
}

void AAREnemyBase::SetFormationLockRules(bool bInFormationLockEnter, bool bInFormationLockActive)
{
	if (!HasAuthority())
	{
		return;
	}

	bFormationLockEnter = bInFormationLockEnter;
	bFormationLockActive = bInFormationLockActive;
	TryDispatchEnteredScreenEvent();
	TryDispatchInFormationEvent();
	ForceNetUpdate();
}

void AAREnemyBase::SetReachedFormationSlot(bool bInReachedFormationSlot)
{
	if (!HasAuthority())
	{
		return;
	}

	bReachedFormationSlot = bInReachedFormationSlot;
	TryDispatchInFormationEvent();
}

bool AAREnemyBase::CheckAndMarkLeaked(float LeakBoundaryX)
{
	if (!HasAuthority() || bCountedAsLeak || bIsDead)
	{
		return false;
	}

	// In this coordinate layout, leak means crossing player-side low-X boundary.
	if (GetActorLocation().X >= LeakBoundaryX)
	{
		return false;
	}

	bCountedAsLeak = true;

	return true;
}

AAREnemyAIController* AAREnemyBase::GetEnemyAIController() const
{
	return Cast<AAREnemyAIController>(GetController());
}

UARStateTreeAIComponent* AAREnemyBase::GetEnemyStateTreeComponent() const
{
	if (const AAREnemyAIController* EnemyAI = GetEnemyAIController())
	{
		return EnemyAI->GetEnemyStateTreeComponent();
	}

	return nullptr;
}

bool AAREnemyBase::SendEnemyStateTreeEvent(const FStateTreeEvent& Event)
{
	if (!HasAuthority())
	{
		UE_LOG(ARLog, Warning, TEXT("[EnemyBase] SendEnemyStateTreeEvent ignored on non-authority for '%s' (Event=%s)."),
			*GetNameSafe(this), *Event.Tag.ToString());
		return false;
	}

	if (AAREnemyAIController* EnemyAI = GetEnemyAIController())
	{
		return EnemyAI->SendStateTreeEvent(Event);
	}

	UE_LOG(ARLog, Warning, TEXT("[EnemyBase] SendEnemyStateTreeEvent failed for '%s': no enemy AI controller (Event=%s)."),
		*GetNameSafe(this), *Event.Tag.ToString());
	return false;
}

bool AAREnemyBase::SendEnemyStateTreeEventByTag(FGameplayTag EventTag, FName Origin)
{
	if (!HasAuthority())
	{
		UE_LOG(ARLog, Warning, TEXT("[EnemyBase] SendEnemyStateTreeEventByTag ignored on non-authority for '%s' (Event=%s)."),
			*GetNameSafe(this), *EventTag.ToString());
		return false;
	}

	if (AAREnemyAIController* EnemyAI = GetEnemyAIController())
	{
		return EnemyAI->SendStateTreeEventByTag(EventTag, Origin);
	}

	UE_LOG(ARLog, Warning, TEXT("[EnemyBase] SendEnemyStateTreeEventByTag failed for '%s': no enemy AI controller (Event=%s)."),
		*GetNameSafe(this), *EventTag.ToString());
	return false;
}

bool AAREnemyBase::SendEnemySignalToController(
	FGameplayTag SignalTag,
	AActor* RelatedActor,
	FVector WorldLocation,
	float ScalarValue,
	bool bForwardToStateTree)
{
	if (!HasAuthority() || !SignalTag.IsValid())
	{
		if (!HasAuthority())
		{
			UE_LOG(ARLog, Warning, TEXT("[EnemyBase] SendEnemySignalToController ignored on non-authority for '%s' (Signal=%s)."),
				*GetNameSafe(this), *SignalTag.ToString());
		}
		else
		{
			UE_LOG(ARLog, Warning, TEXT("[EnemyBase] SendEnemySignalToController ignored for '%s': invalid signal tag."), *GetNameSafe(this));
		}
		return false;
	}

	if (AAREnemyAIController* EnemyAI = GetEnemyAIController())
	{
		return EnemyAI->ReceivePawnSignal(
			SignalTag,
			RelatedActor,
			WorldLocation,
			ScalarValue,
			bForwardToStateTree);
	}

	UE_LOG(ARLog, Warning, TEXT("[EnemyBase] SendEnemySignalToController failed for '%s': no enemy AI controller (Signal=%s)."),
		*GetNameSafe(this), *SignalTag.ToString());
	return false;
}

void AAREnemyBase::TryDispatchWavePhaseEvent()
{
	if (!HasAuthority())
	{
		return;
	}

	if (LastDispatchedWavePhaseWaveId == WaveInstanceId && LastDispatchedWavePhase == WavePhase)
	{
		return;
	}

	LastDispatchedWavePhaseWaveId = WaveInstanceId;
	LastDispatchedWavePhase = WavePhase;
	BP_OnWavePhaseChanged(WavePhase);
	if (AAREnemyAIController* EnemyAI = Cast<AAREnemyAIController>(GetController()))
	{
		EnemyAI->NotifyWavePhaseChanged(WaveInstanceId, WavePhase);
	}
}

void AAREnemyBase::TryDispatchEnteredScreenEvent()
{
	if (!HasAuthority() || bHasDispatchedEnteredScreenEvent)
	{
		return;
	}

	if (!bHasEnteredGameplayScreen)
	{
		return;
	}

	bHasDispatchedEnteredScreenEvent = true;
	if (AAREnemyAIController* EnemyAI = Cast<AAREnemyAIController>(GetController()))
	{
		EnemyAI->NotifyEnemyEnteredScreen(WaveInstanceId);
	}
}

void AAREnemyBase::TryDispatchInFormationEvent()
{
	if (!HasAuthority() || bHasDispatchedInFormationEvent)
	{
		return;
	}

	// InFormation means "formation slot reached while on screen".
	if (!bReachedFormationSlot || !bHasEnteredGameplayScreen)
	{
		return;
	}

	bHasDispatchedInFormationEvent = true;
	if (AAREnemyAIController* EnemyAI = Cast<AAREnemyAIController>(GetController()))
	{
		EnemyAI->NotifyEnemyInFormation(WaveInstanceId);
	}
}

bool AAREnemyBase::CanFireByWaveRules() const
{
	if (!bHasEnteredGameplayScreen)
	{
		return false;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const float Now = World->GetTimeSeconds();
	return (Now - EnteredGameplayScreenServerTime) >= 1.0f;
}

void AAREnemyBase::OnRep_WaveRuntimeContext()
{
	BP_OnWavePhaseChanged(WavePhase);
}

bool AAREnemyBase::ActivateAbilityByTag(FGameplayTag AbilityTag, bool bAllowPartialMatch)
{
	if (!HasAuthority())
	{
		UE_LOG(ARLog, Warning, TEXT("[EnemyBase] ActivateAbilityByTag ignored on non-authority for '%s'."), *GetNameSafe(this));
		return false;
	}

	if (!AbilitySystemComponent || !AbilityTag.IsValid())
	{
		return false;
	}

	if (!CanFireByWaveRules())
	{
		return false;
	}

	FGameplayTagContainer QueryTags;
	QueryTags.AddTag(AbilityTag);
	return AbilitySystemComponent->TryActivateAbilitiesByTag(QueryTags, bAllowPartialMatch);
}

void AAREnemyBase::CancelAbilitiesByTag(FGameplayTag AbilityTag, bool bAllowPartialMatch)
{
	(void)bAllowPartialMatch;

	if (!HasAuthority())
	{
		UE_LOG(ARLog, Warning, TEXT("[EnemyBase] CancelAbilitiesByTag ignored on non-authority for '%s'."), *GetNameSafe(this));
		return;
	}

	if (!AbilitySystemComponent || !AbilityTag.IsValid())
	{
		return;
	}

	FGameplayTagContainer WithTags;
	WithTags.AddTag(AbilityTag);
	AbilitySystemComponent->CancelAbilities(&WithTags, nullptr);
}

void AAREnemyBase::PushASCStateTag(FGameplayTag StateTag)
{
	if (!HasAuthority() || !AbilitySystemComponent || !StateTag.IsValid())
	{
		return;
	}

	int32& RefCount = ASCStateTagRefCounts.FindOrAdd(StateTag);
	RefCount++;
	if (RefCount > 1)
	{
		return;
	}

	FGameplayTagContainer SingleTag;
	SingleTag.AddTag(StateTag);
	AbilitySystemComponent->AddLooseGameplayTags(SingleTag, 1, EGameplayTagReplicationState::TagOnly);
}

void AAREnemyBase::PopASCStateTag(FGameplayTag StateTag)
{
	if (!HasAuthority() || !AbilitySystemComponent || !StateTag.IsValid())
	{
		return;
	}

	int32* RefCount = ASCStateTagRefCounts.Find(StateTag);
	if (!RefCount)
	{
		return;
	}

	(*RefCount)--;
	if (*RefCount > 0)
	{
		return;
	}

	ASCStateTagRefCounts.Remove(StateTag);
	FGameplayTagContainer SingleTag;
	SingleTag.AddTag(StateTag);
	AbilitySystemComponent->RemoveLooseGameplayTags(SingleTag, 1, EGameplayTagReplicationState::TagOnly);
}

void AAREnemyBase::PushASCStateTags(const FGameplayTagContainer& StateTags)
{
	if (!HasAuthority() || StateTags.IsEmpty())
	{
		return;
	}

	for (const FGameplayTag StateTag : StateTags)
	{
		PushASCStateTag(StateTag);
	}
}

void AAREnemyBase::PopASCStateTags(const FGameplayTagContainer& StateTags)
{
	if (!HasAuthority() || StateTags.IsEmpty())
	{
		return;
	}

	for (const FGameplayTag StateTag : StateTags)
	{
		PopASCStateTag(StateTag);
	}
}

int32 AAREnemyBase::GetASCStateTagRefCount(FGameplayTag StateTag) const
{
	if (!StateTag.IsValid())
	{
		return 0;
	}

	const int32* RefCount = ASCStateTagRefCounts.Find(StateTag);
	return RefCount ? *RefCount : 0;
}

bool AAREnemyBase::HasASCGameplayTag(FGameplayTag TagToCheck) const
{
	if (!AbilitySystemComponent || !TagToCheck.IsValid())
	{
		return false;
	}

	return AbilitySystemComponent->HasMatchingGameplayTag(TagToCheck);
}

bool AAREnemyBase::HasAnyASCGameplayTags(const FGameplayTagContainer& TagsToCheck) const
{
	if (!AbilitySystemComponent || TagsToCheck.IsEmpty())
	{
		return false;
	}

	return AbilitySystemComponent->HasAnyMatchingGameplayTags(TagsToCheck);
}

void AAREnemyBase::ClearASCStateTags()
{
	if (!AbilitySystemComponent || ASCStateTagRefCounts.IsEmpty())
	{
		ASCStateTagRefCounts.Reset();
		return;
	}

	FGameplayTagContainer TagsToRemove;
	for (const TPair<FGameplayTag, int32>& Entry : ASCStateTagRefCounts)
	{
		if (Entry.Key.IsValid() && Entry.Value > 0)
		{
			TagsToRemove.AddTag(Entry.Key);
		}
	}

	if (!TagsToRemove.IsEmpty())
	{
		AbilitySystemComponent->RemoveLooseGameplayTags(TagsToRemove, 1, EGameplayTagReplicationState::TagOnly);
	}

	ASCStateTagRefCounts.Reset();
}

void AAREnemyBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AAREnemyBase, EnemyColor);
	DOREPLIFETIME(AAREnemyBase, EnemyIdentifierTag);
	DOREPLIFETIME(AAREnemyBase, EnemyArchetypeTag);
	DOREPLIFETIME(AAREnemyBase, bIsDead);
	DOREPLIFETIME(AAREnemyBase, WaveInstanceId);
	DOREPLIFETIME(AAREnemyBase, FormationSlotIndex);
	DOREPLIFETIME(AAREnemyBase, WavePhase);
	DOREPLIFETIME(AAREnemyBase, WavePhaseStartServerTime);
	DOREPLIFETIME(AAREnemyBase, bFormationLockEnter);
	DOREPLIFETIME(AAREnemyBase, bFormationLockActive);
	DOREPLIFETIME(AAREnemyBase, FormationTargetWorldLocation);
	DOREPLIFETIME(AAREnemyBase, bHasFormationTargetWorldLocation);
}

void AAREnemyBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}
