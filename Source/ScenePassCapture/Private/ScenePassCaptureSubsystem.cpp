// Copyright Exiin Game Studio. All Rights Reserved.

#include "ScenePassCaptureSubsystem.h"

#include "ScenePassCaptureProfile.h"
#include "ScenePassCaptureProjectSettings.h"
#include "ScenePassCaptureTypes.h"
#include "ScenePassCaptureViewExtension.h"

#include "Engine/World.h"
#include "SceneViewExtension.h"

void UScenePassCaptureSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	ViewExtension = FSceneViewExtensions::NewExtension<FScenePassCaptureViewExtension>();
}

void UScenePassCaptureSubsystem::Deinitialize()
{
	if (ViewExtension.IsValid())
	{
		// Must happen before we drop our reference: it unhooks the renderer delegate and flushes anything in flight.
		ViewExtension->Shutdown();
		ViewExtension.Reset();
	}

	Profile = nullptr;

	Super::Deinitialize();
}

bool UScenePassCaptureSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE || WorldType == EWorldType::Editor;
}

void UScenePassCaptureSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (InWorld.WorldType == EWorldType::Game || InWorld.WorldType == EWorldType::PIE)
	{
		TryAutoStart(false);
	}
}

void UScenePassCaptureSubsystem::OnWorldComponentsUpdated(UWorld& World)
{
	Super::OnWorldComponentsUpdated(World);

	// Editor worlds never begin play, so this is the equivalent "the map is ready" moment for them.
	if (World.WorldType == EWorldType::Editor)
	{
		TryAutoStart(true);
	}
}

void UScenePassCaptureSubsystem::TryAutoStart(bool bEditorContext)
{
	if (bAutoStartAttempted)
	{
		return;
	}

	const UScenePassCaptureProjectSettings* Settings = GetDefault<UScenePassCaptureProjectSettings>();
	if (!Settings)
	{
		return;
	}

	const bool bWanted = bEditorContext ? Settings->bAutoStartInEditor : Settings->bAutoStartInGame;
	if (!bWanted)
	{
		return;
	}

	bAutoStartAttempted = true;

	if (Settings->DefaultProfile.IsNull())
	{
		UE_LOG(LogScenePassCapture, Warning, TEXT("Auto start is enabled but no Default Profile is set. Set one in Project Settings, Plugins, Scene Pass Capture."));
		return;
	}

	UScenePassCaptureProfile* LoadedProfile = Settings->DefaultProfile.LoadSynchronous();
	if (!LoadedProfile)
	{
		UE_LOG(LogScenePassCapture, Warning, TEXT("Auto start could not load the Default Profile '%s'."), *Settings->DefaultProfile.ToString());
		return;
	}

	UE_LOG(LogScenePassCapture, Log, TEXT("Auto starting capture with profile '%s'."), *LoadedProfile->GetName());

	StartCapture(LoadedProfile);
}

void UScenePassCaptureSubsystem::SetProfile(UScenePassCaptureProfile* InProfile)
{
	Profile = InProfile;

	if (ViewExtension.IsValid())
	{
		ViewExtension->SetProfile(InProfile);
	}
}

UScenePassCaptureProfile* UScenePassCaptureSubsystem::GetProfile() const
{
	return Profile;
}

void UScenePassCaptureSubsystem::SetCaptureEnabled(bool bInEnabled)
{
	if (ViewExtension.IsValid())
	{
		ViewExtension->SetEnabled(bInEnabled);
	}
}

bool UScenePassCaptureSubsystem::IsCaptureEnabled() const
{
	return ViewExtension.IsValid() && ViewExtension->IsEnabled();
}

void UScenePassCaptureSubsystem::StartCapture(UScenePassCaptureProfile* InProfile)
{
	SetProfile(InProfile);
	SetCaptureEnabled(true);
}

void UScenePassCaptureSubsystem::StopCapture()
{
	SetCaptureEnabled(false);
}

void UScenePassCaptureSubsystem::CaptureSingleFrame()
{
	if (ViewExtension.IsValid())
	{
		ViewExtension->RequestSingleFrame();
	}
}
