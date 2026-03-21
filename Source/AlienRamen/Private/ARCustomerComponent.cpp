#include "ARCustomerComponent.h"

#include "ARCustomerOrderWidgetBase.h"
#include "ARCustomerSettings.h"
#include "ARLog.h"
#include "ARShopGameMode.h"
#include "ARShopGameState.h"
#include "ParleyDialogueSubsystem.h"
#include "EmoComponent.h"
#include "EmoSettings.h"
#include "ARNPCCharacterBase.h"
#include "ParleySpeakerComponent.h"
#include "ARPlayerController.h"
#include "ARShopAIController.h"
#include "ARShopCarryComponent.h"
#include "ARRamenBowlActor.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "GameplayTagsManager.h"
#include "Net/UnrealNetwork.h"
#include "StructUtils/InstancedStruct.h"
#include "TagKeySubsystem.h"
#include "Blueprint/UserWidget.h"

namespace
{
	static const FName OrderingStateEmotionSourceId(TEXT("OrderingState"));
	static const FName OrderingReactionEmotionSourceId(TEXT("OrderingReaction"));

	static EARAffinityColor SanitizeColor(const EARAffinityColor InColor)
	{
		return InColor == EARAffinityColor::Unknown ? EARAffinityColor::None : InColor;
	}

	static bool DoesRequestedColorMatchServedColor(const EARAffinityColor RequestedColor, const EARAffinityColor ServedColor)
	{
		const EARAffinityColor SanitizedRequested = SanitizeColor(RequestedColor);
		const EARAffinityColor SanitizedServed = SanitizeColor(ServedColor);
		if (SanitizedRequested == EARAffinityColor::None)
		{
			return SanitizedServed == EARAffinityColor::None;
		}

		if (SanitizedRequested == EARAffinityColor::Colorless)
		{
			return SanitizedServed != EARAffinityColor::None;
		}

		return SanitizedRequested == SanitizedServed;
	}

	static int32 CountRequestedScoringSlots(const TArray<EARAffinityColor>& RequestedColors)
	{
		int32 Count = 0;
		for (const EARAffinityColor Color : RequestedColors)
		{
			if (SanitizeColor(Color) != EARAffinityColor::None)
			{
				++Count;
			}
		}

		return Count;
	}

	static void GetBowlColors(const FARRamenBowlSpec& BowlSpec, TArray<EARAffinityColor>& OutColors)
	{
		OutColors.Reset();
		OutColors.Add(SanitizeColor(BowlSpec.Noodles.Color));
		OutColors.Add(SanitizeColor(BowlSpec.Broth.Color));
		OutColors.Add(SanitizeColor(BowlSpec.Toppings.Color));
	}

	static FGameplayTag BuildTagFromRootAndLeaf(const FGameplayTag& RootTag, const FGameplayTag& SourceTag)
	{
		if (!RootTag.IsValid() || !SourceTag.IsValid())
		{
			return FGameplayTag();
		}

		FString SourcePath = SourceTag.ToString();
		FString Leaf;
		if (SourcePath.Split(TEXT("."), nullptr, &Leaf, ESearchCase::CaseSensitive, ESearchDir::FromEnd) && !Leaf.IsEmpty())
		{
			const FString CandidatePath = FString::Printf(TEXT("%s.%s"), *RootTag.ToString(), *Leaf);
			return UGameplayTagsManager::Get().RequestGameplayTag(FName(*CandidatePath), false);
		}

		return FGameplayTag();
	}
}

UARCustomerComponent::UARCustomerComponent()
{
	SetIsReplicatedByDefault(true);
}

void UARCustomerComponent::BeginPlay()
{
	Super::BeginPlay();

	CachedSpeakerTag = GetSpeakerTag();

	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	FARCustomerDefinitionRow DefinitionRow;
	if (ResolveDefinitionRow(DefinitionRow))
	{
		bPickyExactMatch = DefinitionRow.bPickyExactMatch;
		HateEmotionOverride = DefinitionRow.HateEmotionTagOverride;
		OkEmotionOverride = DefinitionRow.OkEmotionTagOverride;
		LikeEmotionOverride = DefinitionRow.LikeEmotionTagOverride;
		LoveEmotionOverride = DefinitionRow.LoveEmotionTagOverride;
	}

	if (bGenerateOrderOnBeginPlay)
	{
		GenerateNextOrder();
	}

	UpdateDialogueGateFromOrderState();
	RefreshOrderingEmotionState();
}

FGameplayTag UARCustomerComponent::GetSpeakerTag() const
{
	if (SpeakerTagOverride.IsValid())
	{
		return SpeakerTagOverride;
	}

	const AActor* OwnerActor = GetOwner();
	const UParleySpeakerComponent* TalkComponent = OwnerActor ? OwnerActor->FindComponentByClass<UParleySpeakerComponent>() : nullptr;
	return TalkComponent ? TalkComponent->GetSpeakerTag() : FGameplayTag();
}

UARCustomerOrderWidgetBase* UARCustomerComponent::CreateAndInitializeOrderWidget(APlayerController* OwningPlayer) const
{
	if (!OwningPlayer || !OrderWidgetClass)
	{
		return nullptr;
	}

	UARCustomerOrderWidgetBase* Widget = CreateWidget<UARCustomerOrderWidgetBase>(OwningPlayer, OrderWidgetClass);
	if (Widget)
	{
		Widget->InitializeFromCustomer(const_cast<UARCustomerComponent*>(this));
	}

	return Widget;
}

void UARCustomerComponent::InitializeOrderWidget(UARCustomerOrderWidgetBase* WidgetInstance) const
{
	if (WidgetInstance)
	{
		WidgetInstance->InitializeFromCustomer(const_cast<UARCustomerComponent*>(this));
	}
}

int32 UARCustomerComponent::GetRemainingOrdersToGenerate() const
{
	if (MaxOrdersToGenerate <= 0)
	{
		return -1;
	}

	return FMath::Max(0, MaxOrdersToGenerate - OrdersGeneratedCount);
}

bool UARCustomerComponent::GenerateNextOrder()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return false;
	}

	if (!CanGenerateAdditionalOrders())
	{
		ClearActiveOrder();
		SetDoneOrdering(true);
		return false;
	}

	if (!CachedSpeakerTag.IsValid())
	{
		CachedSpeakerTag = GetSpeakerTag();
	}

	int32 RelationshipLevel = 0;
	if (UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		if (UParleyDialogueSubsystem* DialogueSubsystem = GI->GetSubsystem<UParleyDialogueSubsystem>())
		{
			const FGameplayTag RequesterSpeakerTag = UGameplayTagsManager::Get().RequestGameplayTag(FName(TEXT("Parley.Speaker.Requester")), false);
			const FGameplayTag OwnerSpeakerTag = UGameplayTagsManager::Get().RequestGameplayTag(FName(TEXT("Parley.Speaker.Owner")), false);
			const FGameplayTag BrotherTag = UGameplayTagsManager::Get().RequestGameplayTag(FName(TEXT("Parley.Speaker.Brother")), false);
			const FGameplayTag SisterTag = UGameplayTagsManager::Get().RequestGameplayTag(FName(TEXT("Parley.Speaker.Sister")), false);

			auto AccumulateRelationshipLevel = [&RelationshipLevel, DialogueSubsystem, this](const FGameplayTag& SourceSpeakerTag)
			{
				if (!SourceSpeakerTag.IsValid() || !CachedSpeakerTag.IsValid())
				{
					return;
				}

				RelationshipLevel = FMath::Max(RelationshipLevel, DialogueSubsystem->GetRelationshipLevelForSpeakerPair(SourceSpeakerTag, CachedSpeakerTag));
			};

			AccumulateRelationshipLevel(RequesterSpeakerTag);
			AccumulateRelationshipLevel(OwnerSpeakerTag);
			AccumulateRelationshipLevel(BrotherTag);
			AccumulateRelationshipLevel(SisterTag);
		}
	}

	FARRamenOrderRequest ChosenOrder;
	bool bFoundOrder = false;

	FARCustomerDefinitionRow DefinitionRow;
	if (ResolveDefinitionRow(DefinitionRow))
	{
		bFoundOrder = SelectOrderForRelationshipLevel(DefinitionRow, RelationshipLevel, ChosenOrder);
		bPickyExactMatch = DefinitionRow.bPickyExactMatch;
	}

	if (!bFoundOrder)
	{
		bFoundOrder = BuildProceduralFallbackOrder(ChosenOrder);
	}

	if (!bFoundOrder)
	{
		ClearActiveOrder();
		return false;
	}

	NormalizeOrderRequest(ChosenOrder, false);
	ActiveOrder = ChosenOrder;
	bHasActiveOrder = ActiveOrder.RequestedColors.Num() > 0;
	if (bHasActiveOrder)
	{
		OrdersGeneratedCount = FMath::Max(0, OrdersGeneratedCount + 1);
	}
	SetDoneOrdering(false);
	OnRep_ActiveOrder();
	UpdateDialogueGateFromOrderState();
	RefreshOrderingEmotionState();

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (AARShopAIController* ShopAI = Cast<AARShopAIController>(OwnerPawn ? OwnerPawn->GetController() : nullptr))
	{
		const FGameplayTag OrderGeneratedEvent = UGameplayTagsManager::Get().RequestGameplayTag(FName(TEXT("Event.ShopNPC.OrderGenerated")), false);
		if (OrderGeneratedEvent.IsValid())
		{
			ShopAI->SendShopStateTreeEventByTag(OrderGeneratedEvent, TEXT("CustomerOrderGenerated"));
		}
	}

	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->ForceNetUpdate();
	}

	return bHasActiveOrder;
}

void UARCustomerComponent::ClearActiveOrder()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	ActiveOrder = FARRamenOrderRequest();
	bHasActiveOrder = false;
	OnRep_ActiveOrder();
	UpdateDialogueGateFromOrderState();
	RefreshOrderingEmotionState();
	if (!CanGenerateAdditionalOrders())
	{
		SetDoneOrdering(true);
	}

	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->ForceNetUpdate();
	}
}

bool UARCustomerComponent::TryServeBowl(AARPlayerController* InteractingController, const FARRamenBowlSpec& ServedBowl, FARRamenServeResult& OutResult)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !InteractingController || !bHasActiveOrder)
	{
		return false;
	}

	const UARCustomerSettings* Settings = GetDefault<UARCustomerSettings>();
	const int32 HatePoints = Settings ? Settings->HateRelationshipDeltaPoints : 0;
	const int32 OkPoints = Settings ? Settings->OkRelationshipDeltaPoints : 1;
	const int32 LikePoints = Settings ? Settings->LikeRelationshipDeltaPoints : 3;
	const int32 LovePoints = Settings ? Settings->LoveRelationshipDeltaPoints : 5;

	OutResult = EvaluateServeResult(ActiveOrder, ServedBowl, bPickyExactMatch, HatePoints, OkPoints, LikePoints, LovePoints);
	OutResult.AppliedReactionEmotionTag = ResolveReactionEmotionTag(OutResult.Reaction);
	OutResult.RelationshipDeltaPoints = ResolveReactionRelationshipDelta(OutResult.Reaction);
	OrdersServedCount = FMath::Max(0, OrdersServedCount + 1);

	if (AARShopGameMode* ShopGameMode = GetWorld() ? Cast<AARShopGameMode>(GetWorld()->GetAuthGameMode()) : nullptr)
	{
		AARShopGameState* ShopGameState = GetWorld() ? GetWorld()->GetGameState<AARShopGameState>() : nullptr;
		if (ShopGameState)
		{
			float TipMultiplier = 0.0f;
			int32 CombinedMeatValue = 0;
			int32 BasePayout = 0;
			int32 TipPayout = 0;
			const int32 TotalPayout = ShopGameMode->CalculateServePayout(
				ServedBowl,
				OutResult.Reaction,
				TipMultiplier,
				CombinedMeatValue,
				BasePayout,
				TipPayout);
			if (TotalPayout > 0)
			{
				ShopGameState->SetMoneyFromSave(ShopGameState->GetMoney() + TotalPayout);
			}

			UE_LOG(
				ARLog,
				Verbose,
				TEXT("[Shop|Serve] Customer='%s' reaction=%d payout=%d base=%d tip=%d combinedMeatValue=%d tipMultiplier=%.3f."),
				*GetNameSafe(GetOwner()),
				static_cast<int32>(OutResult.Reaction),
				TotalPayout,
				BasePayout,
				TipPayout,
				CombinedMeatValue,
				TipMultiplier);
		}
	}

	ApplyServeOutcomeToDialogue(OutResult);
	ApplyOrderingReactionEmotion(OutResult.AppliedReactionEmotionTag);
	OnCustomerOrderResolved.Broadcast(OutResult);
	OnCustomerOrderServedDetailed.Broadcast(OutResult, OrdersGeneratedCount, OrdersServedCount, GetRemainingOrdersToGenerate());

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (AARShopAIController* ShopAI = Cast<AARShopAIController>(OwnerPawn ? OwnerPawn->GetController() : nullptr))
	{
		const FGameplayTag OrderServedEvent = UGameplayTagsManager::Get().RequestGameplayTag(FName(TEXT("Event.ShopNPC.OrderServed")), false);
		if (OrderServedEvent.IsValid())
		{
			ShopAI->SendShopStateTreeEventByTag(OrderServedEvent, TEXT("CustomerOrderServed"));
		}
	}

	if (bGenerateNextOrderAfterServe)
	{
		GenerateNextOrder();
	}
	else
	{
		ClearActiveOrder();
	}

	return true;
}

bool UARCustomerComponent::TryServeHeldBowlFromController(AARPlayerController* InteractingController, FARRamenServeResult& OutResult)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !InteractingController || !bHasActiveOrder)
	{
		return false;
	}

	APawn* Pawn = InteractingController->GetPawn();
	UARShopCarryComponent* CarryComponent = Pawn ? Pawn->FindComponentByClass<UARShopCarryComponent>() : nullptr;
	AARRamenBowlActor* HeldBowl = CarryComponent ? CarryComponent->GetHeldBowlActor() : nullptr;
	if (!CarryComponent || !HeldBowl || !HeldBowl->IsComplete())
	{
		return false;
	}

	if (!TryServeBowl(InteractingController, HeldBowl->GetBowlSpec(), OutResult))
	{
		return false;
	}

	CarryComponent->ClearHeldActor(false);
	HeldBowl->ReleaseCarryItem();
	return true;
}

FGameplayTag UARCustomerComponent::ResolveReactionEmotionTag(const EARRamenTasteReaction Reaction) const
{
	const UARCustomerSettings* Settings = GetDefault<UARCustomerSettings>();

	switch (Reaction)
	{
	case EARRamenTasteReaction::Hate:
		return HateEmotionOverride.IsValid() ? HateEmotionOverride : (Settings ? Settings->HateEmotionTag : FGameplayTag());
	case EARRamenTasteReaction::Ok:
		return OkEmotionOverride.IsValid() ? OkEmotionOverride : (Settings ? Settings->OkEmotionTag : FGameplayTag());
	case EARRamenTasteReaction::Like:
		return LikeEmotionOverride.IsValid() ? LikeEmotionOverride : (Settings ? Settings->LikeEmotionTag : FGameplayTag());
	case EARRamenTasteReaction::Love:
		return LoveEmotionOverride.IsValid() ? LoveEmotionOverride : (Settings ? Settings->LoveEmotionTag : FGameplayTag());
	default:
		return FGameplayTag();
	}
}

int32 UARCustomerComponent::ResolveReactionRelationshipDelta(const EARRamenTasteReaction Reaction) const
{
	const UARCustomerSettings* Settings = GetDefault<UARCustomerSettings>();
	if (!Settings)
	{
		return 0;
	}

	switch (Reaction)
	{
	case EARRamenTasteReaction::Hate:
		return Settings->HateRelationshipDeltaPoints;
	case EARRamenTasteReaction::Ok:
		return Settings->OkRelationshipDeltaPoints;
	case EARRamenTasteReaction::Like:
		return Settings->LikeRelationshipDeltaPoints;
	case EARRamenTasteReaction::Love:
		return Settings->LoveRelationshipDeltaPoints;
	default:
		return 0;
	}
}

FARRamenServeResult UARCustomerComponent::EvaluateServeResult(
	const FARRamenOrderRequest& RequestedOrder,
	const FARRamenBowlSpec& ServedBowl,
	const bool bUsePickyExactRule,
	const int32 HatePoints,
	const int32 OkPoints,
	const int32 LikePoints,
	const int32 LovePoints)
{
	FARRamenServeResult Result;
	Result.bUsedPickyRule = bUsePickyExactRule;

	FARRamenOrderRequest NormalizedOrder = RequestedOrder;
	NormalizeOrderRequest(NormalizedOrder, bUsePickyExactRule);

	TArray<EARAffinityColor> BowlColors;
	GetBowlColors(ServedBowl, BowlColors);

	if (bUsePickyExactRule)
	{
		static const int32 Permutations[6][3] =
		{
			{ 0, 1, 2 },
			{ 0, 2, 1 },
			{ 1, 0, 2 },
			{ 1, 2, 0 },
			{ 2, 0, 1 },
			{ 2, 1, 0 }
		};

		Result.bExactCompositionMatch = false;
		for (int32 PermutationIndex = 0; PermutationIndex < UE_ARRAY_COUNT(Permutations); ++PermutationIndex)
		{
			bool bAllSlotsMatch = true;
			for (int32 SlotIndex = 0; SlotIndex < 3; ++SlotIndex)
			{
				const EARAffinityColor RequestedColor = NormalizedOrder.RequestedColors[SlotIndex];
				const EARAffinityColor ServedColor = BowlColors[Permutations[PermutationIndex][SlotIndex]];
				if (!DoesRequestedColorMatchServedColor(RequestedColor, ServedColor))
				{
					bAllSlotsMatch = false;
					break;
				}
			}

			if (bAllSlotsMatch)
			{
				Result.bExactCompositionMatch = true;
				break;
			}
		}

		if (Result.bExactCompositionMatch)
		{
			Result.MatchedColorCount = CountRequestedScoringSlots(NormalizedOrder.RequestedColors);
		}
		else
		{
			Result.MatchedColorCount = 0;
		}
	}
	else
	{
		int32 RequestedNoneCount = 0;
		int32 RequestedColorlessCount = 0;
		int32 RequestedRedCount = 0;
		int32 RequestedWhiteCount = 0;
		int32 RequestedBlueCount = 0;

		for (const EARAffinityColor Color : NormalizedOrder.RequestedColors)
		{
			switch (SanitizeColor(Color))
			{
			case EARAffinityColor::None:
				++RequestedNoneCount;
				break;
			case EARAffinityColor::Colorless:
				++RequestedColorlessCount;
				break;
			case EARAffinityColor::Red:
				++RequestedRedCount;
				break;
			case EARAffinityColor::White:
				++RequestedWhiteCount;
				break;
			case EARAffinityColor::Blue:
				++RequestedBlueCount;
				break;
			default:
				break;
			}
		}

		int32 ServedNoneCount = 0;
		int32 ServedColorlessCount = 0;
		int32 ServedRedCount = 0;
		int32 ServedWhiteCount = 0;
		int32 ServedBlueCount = 0;
		for (const EARAffinityColor Color : BowlColors)
		{
			switch (SanitizeColor(Color))
			{
			case EARAffinityColor::None:
				++ServedNoneCount;
				break;
			case EARAffinityColor::Colorless:
				++ServedColorlessCount;
				break;
			case EARAffinityColor::Red:
				++ServedRedCount;
				break;
			case EARAffinityColor::White:
				++ServedWhiteCount;
				break;
			case EARAffinityColor::Blue:
				++ServedBlueCount;
				break;
			default:
				break;
			}
		}

		Result.MatchedColorCount += FMath::Min(RequestedNoneCount, ServedNoneCount);
		ServedNoneCount -= FMath::Min(RequestedNoneCount, ServedNoneCount);
		Result.MatchedColorCount += FMath::Min(RequestedRedCount, ServedRedCount);
		ServedRedCount -= FMath::Min(RequestedRedCount, ServedRedCount);
		Result.MatchedColorCount += FMath::Min(RequestedWhiteCount, ServedWhiteCount);
		ServedWhiteCount -= FMath::Min(RequestedWhiteCount, ServedWhiteCount);
		Result.MatchedColorCount += FMath::Min(RequestedBlueCount, ServedBlueCount);
		ServedBlueCount -= FMath::Min(RequestedBlueCount, ServedBlueCount);

		const int32 RemainingServedNonNoneCount = ServedColorlessCount + ServedRedCount + ServedWhiteCount + ServedBlueCount;
		Result.MatchedColorCount += FMath::Min(RequestedColorlessCount, RemainingServedNonNoneCount);
	}

	if (Result.MatchedColorCount >= 3)
	{
		Result.Reaction = EARRamenTasteReaction::Love;
		Result.RelationshipDeltaPoints = LovePoints;
	}
	else if (Result.MatchedColorCount == 2)
	{
		Result.Reaction = EARRamenTasteReaction::Like;
		Result.RelationshipDeltaPoints = LikePoints;
	}
	else if (Result.MatchedColorCount == 1)
	{
		Result.Reaction = EARRamenTasteReaction::Ok;
		Result.RelationshipDeltaPoints = OkPoints;
	}
	else
	{
		Result.Reaction = EARRamenTasteReaction::Hate;
		Result.RelationshipDeltaPoints = HatePoints;
	}

	return Result;
}

void UARCustomerComponent::OnRep_ActiveOrder()
{
	OnCustomerOrderChanged.Broadcast(ActiveOrder);
	if (ActiveOrder.RequestedColors.Num() > 0)
	{
		OnCustomerOrderGeneratedDetailed.Broadcast(ActiveOrder, OrdersGeneratedCount, OrdersServedCount, GetRemainingOrdersToGenerate());
	}
}

void UARCustomerComponent::OnRep_DoneOrdering(const bool bOldDoneOrdering)
{
	if (bDoneOrdering == bOldDoneOrdering)
	{
		return;
	}

	if (bDoneOrdering)
	{
		OnCustomerDoneOrdering.Broadcast(OrdersGeneratedCount, OrdersServedCount, GetRemainingOrdersToGenerate());
	}
}

void UARCustomerComponent::NormalizeOrderRequest(FARRamenOrderRequest& InOutOrder, const bool bPadToThreeSlots)
{
	TArray<EARAffinityColor> Normalized;
	Normalized.Reserve(3);

	for (const EARAffinityColor Color : InOutOrder.RequestedColors)
	{
		if (Normalized.Num() >= 3)
		{
			break;
		}

		Normalized.Add(SanitizeColor(Color));
	}

	while (bPadToThreeSlots && Normalized.Num() < 3)
	{
		Normalized.Add(EARAffinityColor::None);
	}

	InOutOrder.RequestedColors = MoveTemp(Normalized);
}

bool UARCustomerComponent::ResolveDefinitionRow(FARCustomerDefinitionRow& OutRow) const
{
	OutRow = FARCustomerDefinitionRow();

	const FGameplayTag SpeakerTag = CachedSpeakerTag.IsValid() ? CachedSpeakerTag : GetSpeakerTag();
	if (!SpeakerTag.IsValid())
	{
		return false;
	}

	UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UTagKeySubsystem* Lookup = GI ? GI->GetSubsystem<UTagKeySubsystem>() : nullptr;
	if (!Lookup)
	{
		return false;
	}

	FInstancedStruct RowData;
	FString Error;
	if (Lookup->TryResolveRowStructForTag(SpeakerTag, RowData, Error))
	{
		if (const FARCustomerDefinitionRow* Row = RowData.GetPtr<FARCustomerDefinitionRow>())
		{
			OutRow = *Row;
			return true;
		}
	}

	const UARCustomerSettings* Settings = GetDefault<UARCustomerSettings>();
	if (!Settings || !Settings->CustomerDefinitionRootTag.IsValid())
	{
		return false;
	}

	const FGameplayTag CandidateTag = BuildTagFromRootAndLeaf(Settings->CustomerDefinitionRootTag, SpeakerTag);
	if (!CandidateTag.IsValid())
	{
		return false;
	}

	if (!Lookup->TryResolveRowStructForTag(CandidateTag, RowData, Error))
	{
		return false;
	}

	if (const FARCustomerDefinitionRow* Row = RowData.GetPtr<FARCustomerDefinitionRow>())
	{
		OutRow = *Row;
		return true;
	}

	return false;
}

bool UARCustomerComponent::SelectOrderForRelationshipLevel(const FARCustomerDefinitionRow& Row, const int32 RelationshipLevel, FARRamenOrderRequest& OutOrder) const
{
	TArray<const FARRamenOrderOption*> Eligible;
	int32 TotalWeight = 0;

	for (const FARRamenOrderOption& Option : Row.OrderOptions)
	{
		if (Option.Weight <= 0
			|| RelationshipLevel < Option.MinRelationshipLevel
			|| RelationshipLevel > Option.MaxRelationshipLevel
			|| Option.Order.RequestedColors.Num() <= 0)
		{
			continue;
		}

		Eligible.Add(&Option);
		TotalWeight += Option.Weight;
	}

	if (Eligible.IsEmpty() || TotalWeight <= 0)
	{
		return false;
	}

	int32 Roll = FMath::RandRange(1, TotalWeight);
	for (const FARRamenOrderOption* Option : Eligible)
	{
		Roll -= Option->Weight;
		if (Roll <= 0)
		{
			OutOrder = Option->Order;
			NormalizeOrderRequest(OutOrder, false);
			return OutOrder.RequestedColors.Num() > 0;
		}
	}

	OutOrder = Eligible.Last()->Order;
	NormalizeOrderRequest(OutOrder, false);
	return OutOrder.RequestedColors.Num() > 0;
}

bool UARCustomerComponent::BuildProceduralFallbackOrder(FARRamenOrderRequest& OutOrder) const
{
	const UARCustomerSettings* Settings = GetDefault<UARCustomerSettings>();
	if (!Settings || !Settings->bAllowProceduralFallbackOrders)
	{
		return false;
	}

	OutOrder.RequestedColors.Reset();
	const int32 ColorCount = FMath::RandRange(1, 3);
	static const EARAffinityColor CandidateColors[] =
	{
		EARAffinityColor::Red,
		EARAffinityColor::Blue,
		EARAffinityColor::White,
		EARAffinityColor::Colorless
	};

	for (int32 Index = 0; Index < ColorCount; ++Index)
	{
		OutOrder.RequestedColors.Add(CandidateColors[FMath::RandRange(0, UE_ARRAY_COUNT(CandidateColors) - 1)]);
	}

	NormalizeOrderRequest(OutOrder, false);
	return OutOrder.RequestedColors.Num() > 0;
}

bool UARCustomerComponent::CanGenerateAdditionalOrders() const
{
	return MaxOrdersToGenerate <= 0 || OrdersGeneratedCount < MaxOrdersToGenerate;
}

void UARCustomerComponent::SetDoneOrdering(const bool bNewDoneOrdering)
{
	if (bDoneOrdering == bNewDoneOrdering)
	{
		return;
	}

	const bool bOldDoneOrdering = bDoneOrdering;
	bDoneOrdering = bNewDoneOrdering;
	OnRep_DoneOrdering(bOldDoneOrdering);
}

void UARCustomerComponent::UpdateDialogueGateFromOrderState() const
{
	if (AARNPCCharacterBase* NPCOwner = Cast<AARNPCCharacterBase>(GetOwner()))
	{
		NPCOwner->SetSpeakerLocalStateAllowsDialogue(!bHasActiveOrder);
	}
}

void UARCustomerComponent::RefreshOrderingEmotionState() const
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}

	UEmoComponent* EmotionComponent = OwnerActor->FindComponentByClass<UEmoComponent>();
	if (!EmotionComponent)
	{
		return;
	}

	const UARCustomerSettings* CustomerSettings = GetDefault<UARCustomerSettings>();
	const int32 OrderingPriority = CustomerSettings ? CustomerSettings->OrderingStateEmotionPriority : 1;

	FGameplayTag ActiveOrderEmotionTag = CustomerSettings ? CustomerSettings->ActiveOrderEmotionTag : FGameplayTag();
	if (!ActiveOrderEmotionTag.IsValid())
	{
		const UEmoSettings* EmotionSettings = GetDefault<UEmoSettings>();
		ActiveOrderEmotionTag = EmotionSettings ? EmotionSettings->WantsToTalkEmotionTag : FGameplayTag();
	}

	if (bHasActiveOrder && ActiveOrderEmotionTag.IsValid())
	{
		EmotionComponent->SetSystemEmotionTag(OrderingStateEmotionSourceId, ActiveOrderEmotionTag, OrderingPriority);
	}
	else
	{
		EmotionComponent->ClearSystemEmotionTag(OrderingStateEmotionSourceId);
	}
}

void UARCustomerComponent::ApplyOrderingReactionEmotion(const FGameplayTag& ReactionEmotionTag) const
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() || !ReactionEmotionTag.IsValid())
	{
		return;
	}

	UEmoComponent* EmotionComponent = OwnerActor->FindComponentByClass<UEmoComponent>();
	if (!EmotionComponent)
	{
		return;
	}

	const UARCustomerSettings* CustomerSettings = GetDefault<UARCustomerSettings>();
	const int32 ReactionPriority = CustomerSettings ? CustomerSettings->OrderingReactionEmotionPriority : 2;
	const float ReactionDuration = CustomerSettings ? CustomerSettings->OrderingReactionEmotionDurationSeconds : -1.0f;
	EmotionComponent->SetSystemEmotionTagForDuration(OrderingReactionEmotionSourceId, ReactionEmotionTag, ReactionDuration, ReactionPriority);
}

bool UARCustomerComponent::ApplyServeOutcomeToDialogue(const FARRamenServeResult& ServeResult) const
{
	UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UParleyDialogueSubsystem* DialogueSubsystem = GI ? GI->GetSubsystem<UParleyDialogueSubsystem>() : nullptr;
	if (!DialogueSubsystem)
	{
		return false;
	}

	const FGameplayTag SpeakerTag = CachedSpeakerTag.IsValid() ? CachedSpeakerTag : GetSpeakerTag();
	return DialogueSubsystem->ApplyRamenServeOutcome(SpeakerTag, ServeResult.RelationshipDeltaPoints, ServeResult.AppliedReactionEmotionTag, GetOwner());
}

void UARCustomerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UARCustomerComponent, ActiveOrder);
	DOREPLIFETIME(UARCustomerComponent, bHasActiveOrder);
	DOREPLIFETIME(UARCustomerComponent, bPickyExactMatch);
	DOREPLIFETIME(UARCustomerComponent, OrdersGeneratedCount);
	DOREPLIFETIME(UARCustomerComponent, OrdersServedCount);
	DOREPLIFETIME(UARCustomerComponent, bDoneOrdering);
	DOREPLIFETIME(UARCustomerComponent, CachedSpeakerTag);
}
