#include "ARCustomerComponent.h"

#include "ARCustomerOrderWidgetBase.h"
#include "ARCustomerSettings.h"
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

	static int32 ColorIndex(const EARAffinityColor InColor)
	{
		switch (SanitizeColor(InColor))
		{
		case EARAffinityColor::Red:
			return 1;
		case EARAffinityColor::White:
			return 2;
		case EARAffinityColor::Blue:
			return 3;
		default:
			return 0;
		}
	}

	static void GetBowlColors(const FARRamenBowlSpec& BowlSpec, TArray<EARAffinityColor>& OutColors)
	{
		OutColors.Reset();
		OutColors.Add(SanitizeColor(BowlSpec.NoodlesColor));
		OutColors.Add(SanitizeColor(BowlSpec.BrothColor));
		OutColors.Add(SanitizeColor(BowlSpec.ToppingsColor));
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
			RelationshipLevel = DialogueSubsystem->GetRelationshipLevelForSpeaker(CachedSpeakerTag);
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
		TArray<int32> OrderCounts;
		TArray<int32> BowlCounts;
		OrderCounts.Init(0, 4);
		BowlCounts.Init(0, 4);

		for (const EARAffinityColor Color : NormalizedOrder.RequestedColors)
		{
			OrderCounts[ColorIndex(Color)] += 1;
		}
		for (const EARAffinityColor Color : BowlColors)
		{
			BowlCounts[ColorIndex(Color)] += 1;
		}

		Result.bExactCompositionMatch = OrderCounts == BowlCounts;
		if (Result.bExactCompositionMatch)
		{
			for (const EARAffinityColor Color : NormalizedOrder.RequestedColors)
			{
				if (SanitizeColor(Color) != EARAffinityColor::None)
				{
					Result.MatchedColorCount += 1;
				}
			}
		}
		else
		{
			Result.MatchedColorCount = 0;
		}
	}
	else
	{
		int32 RequestedCounts[4] = { 0, 0, 0, 0 };
		int32 ServedCounts[4] = { 0, 0, 0, 0 };

		for (const EARAffinityColor Color : NormalizedOrder.RequestedColors)
		{
			const EARAffinityColor Sanitized = SanitizeColor(Color);
			if (Sanitized != EARAffinityColor::None)
			{
				RequestedCounts[ColorIndex(Sanitized)] += 1;
			}
		}
		for (const EARAffinityColor Color : BowlColors)
		{
			const EARAffinityColor Sanitized = SanitizeColor(Color);
			if (Sanitized != EARAffinityColor::None)
			{
				ServedCounts[ColorIndex(Sanitized)] += 1;
			}
		}

		Result.MatchedColorCount =
			FMath::Min(RequestedCounts[1], ServedCounts[1]) +
			FMath::Min(RequestedCounts[2], ServedCounts[2]) +
			FMath::Min(RequestedCounts[3], ServedCounts[3]);
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
		EARAffinityColor::White
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
