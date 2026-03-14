#include "ParleyModule.h"
#include "ParleyLog.h"
#include "GameplayTagsManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FParleyModule, Parley)

void FParleyModule::StartupModule()
{
	if (TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("Parley")))
	{
		const FString TagsSearchPath = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Config"), TEXT("Tags"));
		UGameplayTagsManager::Get().AddTagIniSearchPath(TagsSearchPath);
		UE_LOG(ParleyLog, Verbose, TEXT("[Parley] Registered gameplay-tag config path: %s"), *TagsSearchPath);
	}

	static const TCHAR* RequiredTags[] =
	{
		TEXT("Player.Slot"),
		TEXT("Player.Slot.P1"),
		TEXT("Player.Slot.P2"),
		TEXT("Dialogue.Speaker"),
		TEXT("Dialogue.Speaker.Player"),
		TEXT("Dialogue.Speaker.Brother"),
		TEXT("Dialogue.Speaker.Sister"),
		TEXT("Dialogue.Conversation"),
		TEXT("Dialogue.Emotion"),
		TEXT("Dialogue.Emotion.Busy"),
		TEXT("Faction.Identity")
	};

	for (const TCHAR* RequiredTag : RequiredTags)
	{
		if (!UGameplayTagsManager::Get().RequestGameplayTag(FName(RequiredTag), false).IsValid())
		{
			UE_LOG(
				ParleyLog,
				Warning,
				TEXT("[Parley] Required gameplay tag '%s' is missing. Add it to project/plugin gameplay-tag config."),
				RequiredTag);
		}
	}
}

void FParleyModule::ShutdownModule()
{
}
