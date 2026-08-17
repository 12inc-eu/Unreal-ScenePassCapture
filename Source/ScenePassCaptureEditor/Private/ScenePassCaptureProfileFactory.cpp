// Copyright Exiin Game Studio. All Rights Reserved.

#include "ScenePassCaptureProfileFactory.h"

#include "ScenePassCaptureProfile.h"

UScenePassCaptureProfileFactory::UScenePassCaptureProfileFactory()
{
	SupportedClass = UScenePassCaptureProfile::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UScenePassCaptureProfileFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UScenePassCaptureProfile>(InParent, InClass, InName, Flags);
}
