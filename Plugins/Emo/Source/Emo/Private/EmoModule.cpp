#include "EmoModule.h"
#include "EmoLog.h"
#include "GameplayTagsManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FEmoModule, Emo)

void FEmoModule::StartupModule()
{
	if (TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("Emo")))
	{
		const FString TagsSearchPath = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Config"), TEXT("Tags"));
		UGameplayTagsManager::Get().AddTagIniSearchPath(TagsSearchPath);
		UE_LOG(EmoLog, Verbose, TEXT("[Emo] Registered gameplay-tag config path: %s"), *TagsSearchPath);
	}

	static const TCHAR* RequiredTags[] =
	{
		TEXT("Parley.Emotion"),
		TEXT("Parley.Emotion.Busy"),
		TEXT("Parley.Emotion.WantsToTalk"),
		TEXT("Parley.Emotion.Preview")
	};

	for (const TCHAR* RequiredTag : RequiredTags)
	{
		if (!UGameplayTagsManager::Get().RequestGameplayTag(FName(RequiredTag), false).IsValid())
		{
			UE_LOG(
				EmoLog,
				Warning,
				TEXT("[Emo] Required gameplay tag '%s' is missing. Add it to project/plugin gameplay-tag config."),
				RequiredTag);
		}
	}
}

void FEmoModule::ShutdownModule()
{
}
