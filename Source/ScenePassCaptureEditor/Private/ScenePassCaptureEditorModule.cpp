// Copyright Exiin Game Studio. All Rights Reserved.

#include "ScenePassCaptureEntryCustomization.h"

#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"

/**
 * The asset definition and the factory are UCLASSes, so the engine discovers those on its own.
 * The details panel customization is the one thing that has to be registered by hand.
 */
class FScenePassCaptureEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));

		PropertyEditorModule.RegisterCustomPropertyTypeLayout(EntryStructName, FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FScenePassCaptureEntryCustomization::MakeInstance));

		PropertyEditorModule.NotifyCustomizationModuleChanged();
	}

	virtual void ShutdownModule() override
	{
		if (FModuleManager::Get().IsModuleLoaded(TEXT("PropertyEditor")))
		{
			FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));

			PropertyEditorModule.UnregisterCustomPropertyTypeLayout(EntryStructName);

			PropertyEditorModule.NotifyCustomizationModuleChanged();
		}
	}

private:
	// Struct name without the F prefix, which is what the property editor keys on.
	static const FName EntryStructName;
};

const FName FScenePassCaptureEditorModule::EntryStructName = TEXT("ScenePassCaptureEntry");

IMPLEMENT_MODULE(FScenePassCaptureEditorModule, ScenePassCaptureEditor)
