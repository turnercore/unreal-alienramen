#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "ARCustomerComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FARShopCustomerMatchScoringTest,
	"AlienRamen.Shop.Customer.MatchScoring",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FARShopCustomerMatchScoringTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FARRamenOrderRequest SingleRedOrder;
	SingleRedOrder.RequestedColors = { EARAffinityColor::Red };

	FARRamenBowlSpec RedBlueWhiteBowl;
	RedBlueWhiteBowl.Noodles.Color = EARAffinityColor::Red;
	RedBlueWhiteBowl.Broth.Color = EARAffinityColor::Blue;
	RedBlueWhiteBowl.Toppings.Color = EARAffinityColor::White;

	const FARRamenServeResult SingleColorResult = UARCustomerComponent::EvaluateServeResult(
		SingleRedOrder,
		RedBlueWhiteBowl,
		/*bUsePickyExactRule=*/false,
		0,
		1,
		3,
		5);

	TestEqual(TEXT("Single requested color match yields Ok"), SingleColorResult.Reaction, EARRamenTasteReaction::Ok);
	TestEqual(TEXT("Single requested color match count is 1"), SingleColorResult.MatchedColorCount, 1);
	TestEqual(TEXT("Single requested color relationship delta is 1"), SingleColorResult.RelationshipDeltaPoints, 1);

	FARRamenOrderRequest FullOrder;
	FullOrder.RequestedColors = { EARAffinityColor::Red, EARAffinityColor::Blue, EARAffinityColor::White };
	const FARRamenServeResult FullMatchResult = UARCustomerComponent::EvaluateServeResult(
		FullOrder,
		RedBlueWhiteBowl,
		/*bUsePickyExactRule=*/false,
		0,
		1,
		3,
		5);

	TestEqual(TEXT("Three-color full match yields Love"), FullMatchResult.Reaction, EARRamenTasteReaction::Love);
	TestEqual(TEXT("Three-color full match count is 3"), FullMatchResult.MatchedColorCount, 3);
	TestEqual(TEXT("Three-color full match relationship delta is 5"), FullMatchResult.RelationshipDeltaPoints, 5);

	FARRamenOrderRequest DuplicateOrder;
	DuplicateOrder.RequestedColors = { EARAffinityColor::Red, EARAffinityColor::Red };
	FARRamenBowlSpec SingleRedBowl;
	SingleRedBowl.Noodles.Color = EARAffinityColor::Red;
	SingleRedBowl.Broth.Color = EARAffinityColor::None;
	SingleRedBowl.Toppings.Color = EARAffinityColor::None;

	const FARRamenServeResult DuplicateResult = UARCustomerComponent::EvaluateServeResult(
		DuplicateOrder,
		SingleRedBowl,
		/*bUsePickyExactRule=*/false,
		0,
		1,
		3,
		5);

	TestEqual(TEXT("Duplicate-color request counts min(requested,served)"), DuplicateResult.MatchedColorCount, 1);
	TestEqual(TEXT("Duplicate-color partial match yields Ok"), DuplicateResult.Reaction, EARRamenTasteReaction::Ok);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FARShopCustomerPickyExactTest,
	"AlienRamen.Shop.Customer.PickyExact",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FARShopCustomerPickyExactTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FARRamenOrderRequest SingleRedOrder;
	SingleRedOrder.RequestedColors = { EARAffinityColor::Red };

	FARRamenBowlSpec ExactSingleRed;
	ExactSingleRed.Noodles.Color = EARAffinityColor::Red;
	ExactSingleRed.Broth.Color = EARAffinityColor::None;
	ExactSingleRed.Toppings.Color = EARAffinityColor::None;

	FARRamenBowlSpec NonExactSingleRed;
	NonExactSingleRed.Noodles.Color = EARAffinityColor::Red;
	NonExactSingleRed.Broth.Color = EARAffinityColor::Blue;
	NonExactSingleRed.Toppings.Color = EARAffinityColor::None;

	const FARRamenServeResult ExactResult = UARCustomerComponent::EvaluateServeResult(
		SingleRedOrder,
		ExactSingleRed,
		/*bUsePickyExactRule=*/true,
		0,
		1,
		3,
		5);

	TestTrue(TEXT("Picky exact composition marks exact"), ExactResult.bExactCompositionMatch);
	TestEqual(TEXT("Picky exact single-color still maps to Ok"), ExactResult.Reaction, EARRamenTasteReaction::Ok);

	const FARRamenServeResult NonExactResult = UARCustomerComponent::EvaluateServeResult(
		SingleRedOrder,
		NonExactSingleRed,
		/*bUsePickyExactRule=*/true,
		0,
		1,
		3,
		5);

	TestFalse(TEXT("Picky non-exact composition is not exact"), NonExactResult.bExactCompositionMatch);
	TestEqual(TEXT("Picky non-exact composition yields Hate"), NonExactResult.Reaction, EARRamenTasteReaction::Hate);
	TestEqual(TEXT("Picky non-exact relationship delta is zero"), NonExactResult.RelationshipDeltaPoints, 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FARShopCustomerColorlessAndNoneMatchingTest,
	"AlienRamen.Shop.Customer.ColorlessAndNoneMatching",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FARShopCustomerColorlessAndNoneMatchingTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FARRamenOrderRequest ColorlessOrder;
	ColorlessOrder.RequestedColors = { EARAffinityColor::Colorless };

	FARRamenBowlSpec RedBowl;
	RedBowl.Noodles.Color = EARAffinityColor::Red;
	RedBowl.Broth.Color = EARAffinityColor::None;
	RedBowl.Toppings.Color = EARAffinityColor::None;

	const FARRamenServeResult ColorlessResult = UARCustomerComponent::EvaluateServeResult(
		ColorlessOrder,
		RedBowl,
		/*bUsePickyExactRule=*/false,
		0,
		1,
		3,
		5);

	TestEqual(TEXT("Colorless request matches any non-none served color"), ColorlessResult.MatchedColorCount, 1);
	TestEqual(TEXT("Colorless non-picky match yields Ok"), ColorlessResult.Reaction, EARRamenTasteReaction::Ok);

	FARRamenOrderRequest NoneOrder;
	NoneOrder.RequestedColors = { EARAffinityColor::None };

	FARRamenBowlSpec NoneBowl;
	NoneBowl.Noodles.Color = EARAffinityColor::None;
	NoneBowl.Broth.Color = EARAffinityColor::None;
	NoneBowl.Toppings.Color = EARAffinityColor::None;

	const FARRamenServeResult NoneExactResult = UARCustomerComponent::EvaluateServeResult(
		NoneOrder,
		NoneBowl,
		/*bUsePickyExactRule=*/false,
		0,
		1,
		3,
		5);

	TestEqual(TEXT("None request matches only none served color"), NoneExactResult.MatchedColorCount, 1);
	TestEqual(TEXT("None match maps to Ok"), NoneExactResult.Reaction, EARRamenTasteReaction::Ok);

	FARRamenBowlSpec NonNoneBowl;
	NonNoneBowl.Noodles.Color = EARAffinityColor::Red;
	NonNoneBowl.Broth.Color = EARAffinityColor::Blue;
	NonNoneBowl.Toppings.Color = EARAffinityColor::White;
	const FARRamenServeResult NoneMismatchResult = UARCustomerComponent::EvaluateServeResult(
		NoneOrder,
		NonNoneBowl,
		/*bUsePickyExactRule=*/false,
		0,
		1,
		3,
		5);

	TestEqual(TEXT("None request does not match non-none served color"), NoneMismatchResult.MatchedColorCount, 0);
	TestEqual(TEXT("None mismatch yields Hate"), NoneMismatchResult.Reaction, EARRamenTasteReaction::Hate);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
