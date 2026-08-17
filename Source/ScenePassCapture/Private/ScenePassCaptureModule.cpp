// Copyright Exiin Game Studio. All Rights Reserved.

#include "ScenePassCaptureTypes.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "ShaderCore.h"

DEFINE_LOG_CATEGORY(LogScenePassCapture);

/**
 * The module exists mostly to map the plugin's Shaders folder to a virtual shader path.
 * That has to happen before shader compilation starts, which is why the module loads at PostConfigInit.
 */
class FScenePassCaptureModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("ScenePassCapture"));
		if (!Plugin.IsValid())
		{
			return;
		}

		const FString ShaderDirectory = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Shaders"));
		AddShaderSourceDirectoryMapping(TEXT("/Plugin/ScenePassCapture"), ShaderDirectory);
	}

	virtual void ShutdownModule() override
	{
	}
};

IMPLEMENT_MODULE(FScenePassCaptureModule, ScenePassCapture)
