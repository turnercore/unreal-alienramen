#include "ARTransitionTypes.h"

#include "Kismet/GameplayStatics.h"

namespace ARTransition
{
	const TCHAR* OptionSourceMode = TEXT("ARTrSource");
	const TCHAR* OptionReason = TEXT("ARTrReason");
	const TCHAR* OptionDestinationURL = TEXT("ARTrDest");
	const TCHAR* OptionFreshLoad = TEXT("ARTrFresh");

	namespace
	{
		static FString NormalizeOptionSeparators(const FString& Input)
		{
			FString Normalized = Input;
			Normalized.ReplaceInline(TEXT("&"), TEXT("?"), ESearchCase::CaseSensitive);
			return Normalized;
		}

		static void SplitURLAndOptions(const FString& URLOrOptions, FString& OutMapPath, FString& OutOptions)
		{
			OutMapPath = URLOrOptions;
			OutOptions.Reset();

			int32 QueryStart = INDEX_NONE;
			if (URLOrOptions.FindChar(TEXT('?'), QueryStart))
			{
				OutMapPath = URLOrOptions.Left(QueryStart);
				OutOptions = URLOrOptions.Mid(QueryStart + 1);
			}
		}

		static bool DoesTokenMatchOption(const FString& Token, const FString& OptionToken)
		{
			FString TrimmedToken = Token.TrimStartAndEnd();
			if (TrimmedToken.IsEmpty())
			{
				return false;
			}

			const int32 EqualsIndex = TrimmedToken.Find(TEXT("="));
			const FString TokenName = EqualsIndex == INDEX_NONE ? TrimmedToken : TrimmedToken.Left(EqualsIndex);
			return TokenName.TrimStartAndEnd().Equals(OptionToken, ESearchCase::IgnoreCase);
		}

		static void AppendTravelOption(FString& InOutURL, const TCHAR* Key, const FString& Value)
		{
			if (!Key || Value.IsEmpty())
			{
				return;
			}

			InOutURL += TEXT("?");
			InOutURL += Key;
			InOutURL += TEXT("=");
			InOutURL += Value;
		}

		static FString NormalizeParsedOptionValue(const FString& OptionValue)
		{
			FString Normalized = OptionValue;
			int32 AmpersandIndex = INDEX_NONE;
			if (Normalized.FindChar(TEXT('&'), AmpersandIndex))
			{
				Normalized = Normalized.Left(AmpersandIndex);
			}

			return Normalized.TrimStartAndEnd();
		}

		static FString EncodeOptionValue(const FString& Value)
		{
			FString Encoded = Value;
			Encoded.ReplaceInline(TEXT("%"), TEXT("%25"), ESearchCase::CaseSensitive);
			Encoded.ReplaceInline(TEXT("?"), TEXT("%3F"), ESearchCase::CaseSensitive);
			Encoded.ReplaceInline(TEXT("&"), TEXT("%26"), ESearchCase::CaseSensitive);
			Encoded.ReplaceInline(TEXT("="), TEXT("%3D"), ESearchCase::CaseSensitive);
			return Encoded;
		}

		static FString DecodeOptionValue(const FString& Value)
		{
			FString Decoded = Value;
			Decoded.ReplaceInline(TEXT("%3D"), TEXT("="), ESearchCase::IgnoreCase);
			Decoded.ReplaceInline(TEXT("%26"), TEXT("&"), ESearchCase::IgnoreCase);
			Decoded.ReplaceInline(TEXT("%3F"), TEXT("?"), ESearchCase::IgnoreCase);
			Decoded.ReplaceInline(TEXT("%25"), TEXT("%"), ESearchCase::IgnoreCase);
			return Decoded;
		}

		static bool TryParseEnumOption(const FString& OptionsString, const TCHAR* OptionKey, uint8& OutValue)
		{
			OutValue = 0;
			const FString OptionString = NormalizeParsedOptionValue(UGameplayStatics::ParseOption(OptionsString, OptionKey));
			if (OptionString.IsEmpty())
			{
				return false;
			}

			int32 ParsedValue = 0;
			if (!LexTryParseString(ParsedValue, *OptionString))
			{
				return false;
			}

			OutValue = static_cast<uint8>(FMath::Clamp(ParsedValue, 0, 255));
			return true;
		}

		static FString NormalizeTravelOptionsInput(const FString& OptionsOrURL)
		{
			int32 OptionsStart = INDEX_NONE;
			if (OptionsOrURL.FindChar(TEXT('?'), OptionsStart))
			{
				return OptionsOrURL.Mid(OptionsStart);
			}

			return OptionsOrURL;
		}
	}

	FString AppendTravelOptions(const FString& BaseURL, const FString& Options)
	{
		if (Options.IsEmpty())
		{
			return BaseURL;
		}

		FString TrimmedOptions = Options.TrimStartAndEnd();
		if (TrimmedOptions.IsEmpty())
		{
			return BaseURL;
		}

		TrimmedOptions = NormalizeOptionSeparators(TrimmedOptions);
		if (TrimmedOptions.StartsWith(TEXT("?")) || TrimmedOptions.StartsWith(TEXT("&")))
		{
			// UE travel options always chain via '?' (Map?A=1?B=2).
			TrimmedOptions[0] = TEXT('?');
			return BaseURL + TrimmedOptions;
		}

		return FString::Printf(TEXT("%s?%s"), *BaseURL, *TrimmedOptions);
	}

	bool HasTravelOption(const FString& URLOrOptions, const FString& OptionToken)
	{
		const FString Token = OptionToken.TrimStartAndEnd();
		if (Token.IsEmpty() || URLOrOptions.IsEmpty())
		{
			return false;
		}

		FString OptionsString;
		FString IgnoredMapPath;
		SplitURLAndOptions(URLOrOptions, IgnoredMapPath, OptionsString);

		FString SearchSpace = !OptionsString.IsEmpty() ? OptionsString : URLOrOptions;
		SearchSpace = NormalizeOptionSeparators(SearchSpace);

		TArray<FString> OptionTokens;
		SearchSpace.ParseIntoArray(OptionTokens, TEXT("?"), true);
		for (const FString& Option : OptionTokens)
		{
			if (DoesTokenMatchOption(Option, Token))
			{
				return true;
			}
		}

		return false;
	}

	FString EnsureTravelOption(const FString& URLOrOptions, const FString& OptionToken)
	{
		const FString Token = OptionToken.TrimStartAndEnd();
		if (Token.IsEmpty())
		{
			return URLOrOptions;
		}

		FString Value = URLOrOptions.TrimStartAndEnd();
		if (Value.IsEmpty())
		{
			return Token;
		}

		Value = NormalizeOptionSeparators(Value);
		if (HasTravelOption(Value, Token))
		{
			return Value;
		}

		return FString::Printf(TEXT("%s?%s"), *Value, *Token);
	}

	FString AppendTransitionContextOptions(const FString& URL, const FARTransitionContext& Context)
	{
		if (URL.IsEmpty())
		{
			return URL;
		}

		const FString EncodedDestinationValue = EncodeOptionValue(Context.DestinationURL);
		FString TravelURL = URL;
		AppendTravelOption(TravelURL, OptionSourceMode, FString::FromInt(static_cast<int32>(Context.SourceMode)));
		AppendTravelOption(TravelURL, OptionReason, FString::FromInt(static_cast<int32>(Context.Reason)));
		AppendTravelOption(TravelURL, OptionDestinationURL, EncodedDestinationValue);
		AppendTravelOption(TravelURL, OptionFreshLoad, Context.bFreshLoadEntry ? TEXT("1") : TEXT("0"));
		return TravelURL;
	}

	FString BuildTransitionTravelURL(const FString& TransitionMapURL, const FARTransitionContext& Context)
	{
		if (TransitionMapURL.IsEmpty())
		{
			return Context.DestinationURL;
		}

		return AppendTransitionContextOptions(TransitionMapURL, Context);
	}

	void ApplyTransitionContextFromTravelOptions(const FString& OptionsString, FARTransitionContext& InOutContext)
	{
		const FString NormalizedOptions = NormalizeTravelOptionsInput(OptionsString);

		uint8 EnumValue = 0;
		if (TryParseEnumOption(NormalizedOptions, OptionSourceMode, EnumValue))
		{
			InOutContext.SourceMode = static_cast<EARTransitionSourceMode>(EnumValue);
		}

		if (TryParseEnumOption(NormalizedOptions, OptionReason, EnumValue))
		{
			InOutContext.Reason = static_cast<EARTransitionReason>(EnumValue);
		}

		const FString DestinationURL = DecodeOptionValue(NormalizeParsedOptionValue(UGameplayStatics::ParseOption(NormalizedOptions, OptionDestinationURL)));
		if (!DestinationURL.IsEmpty())
		{
			InOutContext.DestinationURL = DestinationURL;
		}

		const FString FreshLoadString = NormalizeParsedOptionValue(UGameplayStatics::ParseOption(NormalizedOptions, OptionFreshLoad));
		if (!FreshLoadString.IsEmpty())
		{
			InOutContext.bFreshLoadEntry = FreshLoadString.StartsWith(TEXT("1"), ESearchCase::IgnoreCase)
				|| FreshLoadString.Equals(TEXT("true"), ESearchCase::IgnoreCase)
				|| FreshLoadString.Equals(TEXT("yes"), ESearchCase::IgnoreCase);
		}
	}

	FString LexToString(const EARTransitionSourceMode SourceMode)
	{
		switch (SourceMode)
		{
		case EARTransitionSourceMode::Lobby:
			return TEXT("Lobby");
		case EARTransitionSourceMode::Shop:
			return TEXT("Shop");
		case EARTransitionSourceMode::Invader:
			return TEXT("Invader");
		case EARTransitionSourceMode::Scrapyard:
			return TEXT("Scrapyard");
		case EARTransitionSourceMode::SaveLoad:
			return TEXT("SaveLoad");
		default:
			return TEXT("Unknown");
		}
	}

	FString LexToString(const EARTransitionReason Reason)
	{
		switch (Reason)
		{
		case EARTransitionReason::GenericContinue:
			return TEXT("GenericContinue");
		case EARTransitionReason::ShopToInvader:
			return TEXT("ShopToInvader");
		case EARTransitionReason::InvaderToScrapyard:
			return TEXT("InvaderToScrapyard");
		case EARTransitionReason::ScrapyardToShop:
			return TEXT("ScrapyardToShop");
		case EARTransitionReason::SaveLoadEntry:
			return TEXT("SaveLoadEntry");
		default:
			return TEXT("None");
		}
	}

	FString LexToString(const EARTravelRoutePolicy RoutePolicy)
	{
		switch (RoutePolicy)
		{
		case EARTravelRoutePolicy::ModeDefault:
			return TEXT("ModeDefault");
		case EARTravelRoutePolicy::ForceTransitionMap:
			return TEXT("ForceTransitionMap");
		case EARTravelRoutePolicy::ForceDirect:
			return TEXT("ForceDirect");
		default:
			return TEXT("ModeDefault");
		}
	}
}
