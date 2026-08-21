// Copyright Exiin Game Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ScenePassCaptureTypes.h"
#include "UObject/GCObject.h"

class AActor;
class USceneCaptureComponent2D;
class UScenePassCaptureProfile;
class UWorld;

/** Where the mirrored cameras should look from. */
struct FScenePassCaptureViewPoint
{
	FVector Location = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;
	float FOVDegrees = 90.0f;

	/** Size of the view being mirrored. Custom pass targets are sized from this so the aspect matches. */
	FIntPoint ViewSize = FIntPoint::ZeroValue;

	bool bValid = false;
};

/**
 * Owns the scene capture components behind the custom passes.
 *
 * Each enabled custom pass gets one USceneCaptureComponent2D whose transform and FOV are copied from
 * the active view every frame, so it renders the same shot with different settings rather than
 * disturbing the real one. Everything lives on a single transient actor that is destroyed with the
 * runner.
 */
class FScenePassCaptureCustomPassRunner : public FGCObject
{
public:
	explicit FScenePassCaptureCustomPassRunner(UWorld* InWorld);
	virtual ~FScenePassCaptureCustomPassRunner();

	/** Rebuilds or updates the capture components and issues captures according to each pass's timing. */
	void Tick(const UScenePassCaptureProfile* Profile, const FScenePassCaptureViewPoint& ViewPoint);

	/** Forces every On Demand pass to capture on the next tick. Pass NAME_None for all passes. */
	void RequestCapture(FName PassName);

	/** Runtime actor selection, for actors that only exist while playing. */
	void AddRuntimeShowOnlyActor(FName PassName, AActor* Actor);
	void AddRuntimeHiddenActor(FName PassName, AActor* Actor);
	void ClearRuntimeActors(FName PassName);

	/** Destroys the capture actor and releases everything. */
	void Shutdown();

	// FGCObject
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override;

private:
	void EnsureComponents(const UScenePassCaptureProfile* Profile);
	void ApplyPassSettings(USceneCaptureComponent2D& Component, const FScenePassCaptureCustomPass& Pass, const FScenePassCaptureViewPoint& ViewPoint);

	TWeakObjectPtr<UWorld> World;

	/** Transient holder so the components have an owner and get cleaned up in one go. */
	TObjectPtr<AActor> CaptureActor;

	TArray<TObjectPtr<USceneCaptureComponent2D>> Components;

	/** Per pass frame counter, used by the Every N Frames timing. */
	TArray<int32> FrameCounters;

	/** Passes waiting on an On Demand trigger. */
	TSet<FName> PendingRequests;
	bool bRequestAll = false;

	/** Actors added from Blueprint at runtime, keyed by pass name. */
	TMap<FName, TArray<TWeakObjectPtr<AActor>>> RuntimeShowOnlyActors;
	TMap<FName, TArray<TWeakObjectPtr<AActor>>> RuntimeHiddenActors;
};
