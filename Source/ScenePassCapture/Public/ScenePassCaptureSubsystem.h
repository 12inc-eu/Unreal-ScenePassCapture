// Copyright Exiin Game Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ScenePassCaptureSubsystem.generated.h"

class UScenePassCaptureProfile;
class FScenePassCaptureViewExtension;

/**
 * Owns the render graph hook for one world. Give it a profile, turn it on, and the passes listed
 * in that profile are mirrored into their render targets every frame with no scene re-render.
 */
UCLASS()
class SCENEPASSCAPTURE_API UScenePassCaptureSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	// UWorldSubsystem
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void OnWorldComponentsUpdated(UWorld& World) override;

	/** Sets the profile that describes which passes go into which render targets. Pass null to clear. */
	UFUNCTION(BlueprintCallable, Category = "Scene Pass Capture")
	void SetProfile(UScenePassCaptureProfile* InProfile);

	UFUNCTION(BlueprintPure, Category = "Scene Pass Capture")
	UScenePassCaptureProfile* GetProfile() const;

	/** Starts or stops continuous per-frame capture. Nothing happens until a profile is set. */
	UFUNCTION(BlueprintCallable, Category = "Scene Pass Capture")
	void SetCaptureEnabled(bool bInEnabled);

	UFUNCTION(BlueprintPure, Category = "Scene Pass Capture")
	bool IsCaptureEnabled() const;

	/** Convenience: sets the profile and turns capture on in one call. */
	UFUNCTION(BlueprintCallable, Category = "Scene Pass Capture")
	void StartCapture(UScenePassCaptureProfile* InProfile);

	UFUNCTION(BlueprintCallable, Category = "Scene Pass Capture")
	void StopCapture();

	/** Captures the next rendered frame only, then goes back to idle. Useful for one-off grabs. */
	UFUNCTION(BlueprintCallable, Category = "Scene Pass Capture")
	void CaptureSingleFrame();

private:
	/** Applies the Project Settings auto start options. bEditorContext picks which of the two flags to honour. */
	void TryAutoStart(bool bEditorContext);

	UPROPERTY(Transient)
	TObjectPtr<UScenePassCaptureProfile> Profile;

	/** Auto start fires once per world, so reopening the same editor map re-arms it but a component update does not re-trigger. */
	bool bAutoStartAttempted = false;

	TSharedPtr<FScenePassCaptureViewExtension, ESPMode::ThreadSafe> ViewExtension;
};
