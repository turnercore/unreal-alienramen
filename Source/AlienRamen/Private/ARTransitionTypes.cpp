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
	}

	FString BuildTransitionTravelURL(const FString& TransitionMapURL, const FARTransitionContext& Context)
	{
		if (TransitionMapURL.IsEmpty())
		{
			return Context.DestinationURL;
		}

		const FString EncodedDestinationValue = EncodeOptionValue(Context.DestinationURL);

		FString TravelURL = TransitionMapURL;
		AppendTravelOption(TravelURL, OptionSourceMode, FString::FromInt(static_cast<int32>(Context.SourceMode)));
		AppendTravelOption(TravelURL, OptionReason, FString::FromInt(static_cast<int32>(Context.Reason)));
		AppendTravelOption(TravelURL, OptionDestinationURL, EncodedDestinationValue);
		// Always append this option so any transport-level suffixing (for example "&listen")
		// is absorbed by a non-critical value instead of destination URL.
		AppendTravelOption(TravelURL, OptionFreshLoad, Context.bFreshLoadEntry ? TEXT("1") : TEXT("0"));

		return TravelURL;
	}

	void ApplyTransitionContextFromTravelOptions(const FString& OptionsString, FARTransitionContext& InOutContext)
	{
		uint8 EnumValue = 0;
		if (TryParseEnumOption(OptionsString, OptionSourceMode, EnumValue))
		{
			InOutContext.SourceMode = static_cast<EARTransitionSourceMode>(EnumValue);
		}

		if (TryParseEnumOption(OptionsString, OptionReason, EnumValue))
		{
			InOutContext.Reason = static_cast<EARTransitionReason>(EnumValue);
		}

		const FString DestinationURL = DecodeOptionValue(NormalizeParsedOptionValue(UGameplayStatics::ParseOption(OptionsString, OptionDestinationURL)));
		if (!DestinationURL.IsEmpty())
		{
			InOutContext.DestinationURL = DestinationURL;
		}

		const FString FreshLoadString = NormalizeParsedOptionValue(UGameplayStatics::ParseOption(OptionsString, OptionFreshLoad));
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
