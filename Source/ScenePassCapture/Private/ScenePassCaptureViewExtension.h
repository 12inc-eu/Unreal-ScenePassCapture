// Copyright Exiin Game Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SceneViewExtension.h"
#include "ScenePassCaptureTypes.h"
#include "RHIResources.h"
#include "UObject/WeakObjectPtr.h"

class UScenePassCaptureProfile;
class FPostOpaqueRenderParameters;
struct FPostProcessMaterialInputs;
struct FScreenPassTexture;

/** One resolved capture instruction. Built on the game thread, consumed on the render thread. Deliberately holds no UObjects. */
struct FScenePassCaptureResolvedEntry
{
	EScenePassCaptureSource Source = EScenePassCaptureSource::SceneDepthWorldUnits;
	FTextureRHIRef TargetRHI;
	float DepthNormalizeRange = 0.0f;
	float StencilNormalizeRange = 255.0f;
	bool bDecodeNormals = false;
};

/** An immutable per-frame snapshot handed across the thread boundary. */
struct FScenePassCaptureFrameTargets
{
	TArray<FScenePassCaptureResolvedEntry> Entries;

	bool Wants(EScenePassCaptureSource Source) const;
	bool WantsPostOpaque() const;
};

/** True for every source read from the renderer's post-opaque hook, false for the post-processing chain sources. */
bool ScenePassCapture_IsPostOpaqueSource(EScenePassCaptureSource Source);

/**
 * Taps the live render graph and blits the requested passes into user render targets.
 *
 * Two hook points are used:
 *  - the renderer's post-opaque delegate, which hands us the GBuffer, depth and pre-translucency scene color
 *  - post-processing pass callbacks, for the before-bloom HDR color and the final tonemapped color
 *
 * Nothing here re-renders the scene. The cost is one blit per requested pass.
 */
class FScenePassCaptureViewExtension : public FSceneViewExtensionBase
{
public:
	FScenePassCaptureViewExtension(const FAutoRegister& AutoRegister);
	virtual ~FScenePassCaptureViewExtension();

	/** Game thread. Unhooks from the renderer and flushes, so it must be called before the last reference is dropped. */
	void Shutdown();

	/** Game thread. The profile is re-read once per frame in BeginRenderViewFamily. */
	void SetProfile(UScenePassCaptureProfile* InProfile);
	UScenePassCaptureProfile* GetProfile() const { return Profile.Get(); }

	/** Game thread. */
	void SetEnabled(bool bInEnabled) { bEnabled = bInEnabled; }
	bool IsEnabled() const { return bEnabled; }

	/** Game thread. Captures the next rendered frame even if continuous capture is off. */
	void RequestSingleFrame() { bSingleFrameRequested = true; }

	// ISceneViewExtension
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override {}
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {}
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;
	virtual void PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) override;
	virtual void PostRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) override;
	virtual void SubscribeToPostProcessingPass(EPostProcessingPass Pass, const FSceneView& InView, FPostProcessingPassDelegateArray& InOutPassCallbacks, bool bIsPassEnabled) override;
	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override;

private:
	/** Renderer post-opaque callback. Runs on the render thread, and only inside a family we opted into. */
	void OnPostOpaqueRender(FPostOpaqueRenderParameters& Parameters);

	/** Post-processing chain callback. Taps the incoming scene color and passes it through untouched. */
	FScreenPassTexture OnPostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& Inputs, EScenePassCaptureSource Source);

	// --- game thread state ---
	TWeakObjectPtr<UScenePassCaptureProfile> Profile;
	bool bEnabled = false;
	bool bSingleFrameRequested = false;
	bool bShutdown = false;
	FDelegateHandle PostOpaqueHandle;

	// --- render thread state ---
	TSharedPtr<const FScenePassCaptureFrameTargets, ESPMode::ThreadSafe> RenderThreadTargets;
	bool bInsideTrackedFamily_RT = false;
	bool bCapturedPostOpaqueThisFamily_RT = false;
};
