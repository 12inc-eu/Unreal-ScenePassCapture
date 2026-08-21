// Copyright Exiin Game Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "ScenePassCaptureSubsystem.generated.h"

class UScenePassCaptureProfile;
class FScenePassCaptureViewExtension;
class FScenePassCaptureCustomPassRunner;

/**
 * Owns the render graph hook for one world. Give it a profile, turn it on, and the passes listed
 * in that profile are mirrored into their render targets every frame with no scene re-render.
 */
UCLASS()
class SCENEPASSCAPTURE_API UScenePassCaptureSubsystem : public UWorldSubsystem, public FTickableGameObject
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

	/**
	 * Restricts a custom pass to these actors at runtime. This is the way to select actors that only
	 * exist while playing, since the actor lists on the profile are references to specific level
	 * actors and cannot resolve in a packaged game. Actor tags on the pass do the same thing
	 * declaratively; use this when you need to name an exact instance.
	 */
	UFUNCTION(BlueprintCallable, Category = "Scene Pass Capture")
	void AddShowOnlyActorForPass(FName PassName, AActor* Actor);

	/** Hides an actor from a custom pass at runtime. */
	UFUNCTION(BlueprintCallable, Category = "Scene Pass Capture")
	void AddHiddenActorForPass(FName PassName, AActor* Actor);

	/** Drops every runtime actor previously added for a pass. */
	UFUNCTION(BlueprintCallable, Category = "Scene Pass Capture")
	void ClearRuntimeActorsForPass(FName PassName);

	/** Triggers an On Demand custom pass. Leave the name empty to trigger every On Demand pass. */
	UFUNCTION(BlueprintCallable, Category = "Scene Pass Capture", meta = (AdvancedDisplay = "PassName"))
	void CaptureCustomPassNow(FName PassName);

	/**
	 * Supplies the camera the custom passes mirror. The editor module pushes the viewport camera
	 * through here; in game the player view point is used automatically and this is not needed.
	 */
	void SetExternalViewPoint(const FVector& Location, const FRotator& Rotation, float FOVDegrees, FIntPoint ViewSize);

	// FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual bool IsTickableInEditor() const override { return true; }
	virtual TStatId GetStatId() const override;

private:
	/** Applies the Project Settings auto start options. bEditorContext picks which of the two flags to honour. */
	void TryAutoStart(bool bEditorContext);

	/** Creates the custom pass runner if it does not exist yet. */
	void EnsureCustomPassRunner();

	UPROPERTY(Transient)
	TObjectPtr<UScenePassCaptureProfile> Profile;

	/** Auto start fires once per world, so reopening the same editor map re-arms it but a component update does not re-trigger. */
	bool bAutoStartAttempted = false;

	TSharedPtr<FScenePassCaptureViewExtension, ESPMode::ThreadSafe> ViewExtension;

	/** Owns the scene capture components behind the custom passes. Only created when one is in use. */
	TSharedPtr<FScenePassCaptureCustomPassRunner> CustomPassRunner;

	/** Camera pushed in by the editor. Ignored once a player view point is available. */
	FVector ExternalViewLocation = FVector::ZeroVector;
	FRotator ExternalViewRotation = FRotator::ZeroRotator;
	float ExternalViewFOV = 90.0f;
	FIntPoint ExternalViewSize = FIntPoint::ZeroValue;
	bool bHasExternalViewPoint = false;
};
