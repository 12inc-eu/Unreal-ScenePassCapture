// Copyright Exiin Game Studio. All Rights Reserved.

#include "ScenePassCaptureSubsystem.h"

#include "ScenePassCaptureCustomPassRunner.h"
#include "ScenePassCaptureProfile.h"
#include "ScenePassCaptureProjectSettings.h"
#include "ScenePassCaptureTypes.h"
#include "ScenePassCaptureViewExtension.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/GameViewportClient.h"
#include "UnrealClient.h"
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

	if (CustomPassRunner.IsValid())
	{
		CustomPassRunner->Shutdown();
		CustomPassRunner.Reset();
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

// ------------------------------------------------------------------------------------------------
// Custom passes

void UScenePassCaptureSubsystem::SetExternalViewPoint(const FVector& Location, const FRotator& Rotation, float FOVDegrees, FIntPoint ViewSize)
{
	ExternalViewLocation = Location;
	ExternalViewRotation = Rotation;
	ExternalViewFOV = FOVDegrees;
	ExternalViewSize = ViewSize;
	bHasExternalViewPoint = true;
}

void UScenePassCaptureSubsystem::CaptureCustomPassNow(FName PassName)
{
	if (CustomPassRunner.IsValid())
	{
		CustomPassRunner->RequestCapture(PassName);
	}
}

bool UScenePassCaptureSubsystem::IsTickable() const
{
	// Only tick when there is custom pass work. The blit sources need no tick at all.
	return Profile != nullptr && Profile->HasAnyCustomPassWork() && IsCaptureEnabled();
}

TStatId UScenePassCaptureSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UScenePassCaptureSubsystem, STATGROUP_Tickables);
}

void UScenePassCaptureSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World || !Profile)
	{
		return;
	}

	// Mirror whatever camera is actually rendering. The player view point wins when there is one,
	// which covers game and PIE; the editor module pushes the viewport camera in otherwise.
	FScenePassCaptureViewPoint ViewPoint;

	if (APlayerController* PlayerController = World->GetFirstPlayerController())
	{
		FVector Location;
		FRotator Rotation;
		PlayerController->GetPlayerViewPoint(Location, Rotation);

		ViewPoint.Location = Location;
		ViewPoint.Rotation = Rotation;
		ViewPoint.FOVDegrees = PlayerController->PlayerCameraManager ? PlayerController->PlayerCameraManager->GetFOVAngle() : 90.0f;
		ViewPoint.bValid = true;
	}
	else if (bHasExternalViewPoint)
	{
		ViewPoint.Location = ExternalViewLocation;
		ViewPoint.Rotation = ExternalViewRotation;
		ViewPoint.FOVDegrees = ExternalViewFOV;
		ViewPoint.bValid = true;
	}

	if (!ViewPoint.bValid)
	{
		return;
	}

	// A scene capture frames by its target's aspect ratio, so the target has to be sized from the view
	// it is mirroring or the result is reframed rather than matched.
	if (UGameViewportClient* GameViewport = World->GetGameViewport())
	{
		if (FViewport* Viewport = GameViewport->Viewport)
		{
			ViewPoint.ViewSize = Viewport->GetSizeXY();
		}
	}

	if (ViewPoint.ViewSize.X <= 0 || ViewPoint.ViewSize.Y <= 0)
	{
		ViewPoint.ViewSize = ExternalViewSize;
	}

	EnsureCustomPassRunner();

	if (CustomPassRunner.IsValid())
	{
		CustomPassRunner->Tick(Profile, ViewPoint);
	}
}

void UScenePassCaptureSubsystem::AddShowOnlyActorForPass(FName PassName, AActor* Actor)
{
	EnsureCustomPassRunner();

	if (CustomPassRunner.IsValid())
	{
		CustomPassRunner->AddRuntimeShowOnlyActor(PassName, Actor);
	}
}

void UScenePassCaptureSubsystem::AddHiddenActorForPass(FName PassName, AActor* Actor)
{
	EnsureCustomPassRunner();

	if (CustomPassRunner.IsValid())
	{
		CustomPassRunner->AddRuntimeHiddenActor(PassName, Actor);
	}
}

void UScenePassCaptureSubsystem::ClearRuntimeActorsForPass(FName PassName)
{
	if (CustomPassRunner.IsValid())
	{
		CustomPassRunner->ClearRuntimeActors(PassName);
	}
}

void UScenePassCaptureSubsystem::EnsureCustomPassRunner()
{
	// Blueprint can register actors before the first tick creates the runner, so build it on demand.
	if (!CustomPassRunner.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			CustomPassRunner = MakeShared<FScenePassCaptureCustomPassRunner>(World);
		}
	}
}
