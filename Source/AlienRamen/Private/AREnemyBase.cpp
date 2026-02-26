// Fill out your copyright notice in the Description page of Project Settings.


#include "AREnemyBase.h"
#include "AREnemyAIController.h"
#include "AREnemyAttributeSet.h"
#include "AREnemyIncomingDamageEffect.h"
#include "ARShipCharacterBase.h"
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
	const FGameplayTag DataDamageTag = FGameplayTag::RequestGameplayTag(TEXT("Data.Damage"), false);

	static bool ApplyDamageToActorViaGAS(AActor* Target, float Damage, AActor* Offender)
	{
		if (!Target || Damage <= 0.f)
		{
			return false;
		}

		if (AAREnemyBase* EnemyTarget = Cast<AAREnemyBase>(Target))
		{
			return EnemyTarget->ApplyDamageViaGAS(Damage, Offender);
		}

		if (AARShipCharacterBase* ShipTarget = Cast<AARShipCharacterBase>(Target))
		{
			return ShipTarget->ApplyDamageViaGAS(Damage, Offender);
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
	AIControllerClass = AAREnemyAIController::StaticClass();

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
}

void AAREnemyBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindHealthChangeDelegate();
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

	AbilitySystemComponent->AddLooseGameplayTags(InTags);
	RuntimeAppliedLooseTags.AppendTags(InTags);
}

void AAREnemyBase::ClearRuntimeEnemyTags()
{
	if (!AbilitySystemComponent || RuntimeAppliedLooseTags.IsEmpty())
	{
		return;
	}

	AbilitySystemComponent->RemoveLooseGameplayTags(RuntimeAppliedLooseTags);
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
			DamageInstigator = ChangeData.GEModData->EffectSpec.GetContext().GetOriginalInstigator();
		}

		HandleDeath(DamageInstigator);
	}
}

void AAREnemyBase::HandleDeath_Implementation(AActor* InstigatorActor)
{
	if (!HasAuthority() || bIsDead)
	{
		return;
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

	UE_LOG(ARLog, Log, TEXT("[EnemyBase] Death handled for '%s'. Instigator='%s'."),
		*GetNameSafe(this), *GetNameSafe(InstigatorActor));

	BP_OnEnemyDied(InstigatorActor);
}

void AAREnemyBase::OnRep_IsDead()
{
	if (bIsDead)
	{
		BP_OnEnemyDied(nullptr);
	}
}

void AAREnemyBase::SetEnemyColor(EAREnemyColor InColor)
{
	if (!HasAuthority())
	{
		return;
	}

	if (EnemyColor == InColor)
	{
		return;
	}

	EnemyColor = InColor;
	ForceNetUpdate();
	UE_LOG(ARLog, Verbose, TEXT("[EnemyBase] Set enemy color for '%s' -> %d."), *GetNameSafe(this), static_cast<int32>(EnemyColor));
	BP_OnEnemyColorChanged(EnemyColor);
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

bool AAREnemyBase::ApplyDamageViaGAS(float Damage, AActor* Offender)
{
	if (!HasAuthority() || bIsDead || Damage <= 0.f || !AbilitySystemComponent)
	{
		return false;
	}

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

	Spec.Data->SetSetByCallerMagnitude(DataDamageTag, Damage);
	AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	return true;
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
	}

	return ApplyDamageViaGAS(DamageAmount, EffectiveOffender) ? DamageAmount : 0.f;
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
	float InPhaseStartServerTime)
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
	bHasEnteredGameplayScreen = false;
	bReachedFormationSlot = false;
	bHasDispatchedEnteredScreenEvent = false;
	bHasDispatchedInFormationEvent = false;
	LastDispatchedWavePhaseWaveId = INDEX_NONE;
	ForceNetUpdate();

	if (PreviousPhase != WavePhase || PreviousWaveInstanceId != WaveInstanceId)
	{
		TryDispatchWavePhaseEvent();
	}
	TryDispatchEnteredScreenEvent();
	TryDispatchInFormationEvent();
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
