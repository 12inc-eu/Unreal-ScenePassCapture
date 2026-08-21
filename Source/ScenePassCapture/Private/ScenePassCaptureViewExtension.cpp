// Copyright Exiin Game Studio. All Rights Reserved.

#include "ScenePassCaptureViewExtension.h"

#include "ScenePassCaptureLumen.h"
#include "ScenePassCaptureProfile.h"
#include "ScenePassCaptureShaders.h"

#include "EngineModule.h"
#include "PixelShaderUtils.h"
#include "PostProcess/PostProcessMaterialInputs.h"
#include "RHIStaticStates.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RendererInterface.h"
#include "SceneTexturesConfig.h"
#include "SceneView.h"
#include "ScreenPass.h"
#include "ProfilingDebugging/RealtimeGPUProfiler.h"
#include "TextureResource.h"
#include "UnrealClient.h"
#include "Engine/TextureRenderTarget2D.h"

// Real measured GPU cost, visible via "stat GPU" and in Unreal Insights. The Cost and Validation
// pane only estimates from bandwidth; this is the number actually spent on the hardware.
DECLARE_GPU_STAT_NAMED(ScenePassCapture, TEXT("Scene Pass Capture"));

static TAutoConsoleVariable<int32> CVarScenePassCaptureEnabled(
	TEXT("r.ScenePassCapture.Enabled"),
	1,
	TEXT("Master switch for the Scene Pass Capture plugin. 0 disables every capture without touching the profile."),
	ECVF_RenderThreadSafe);

// ------------------------------------------------------------------------------------------------
// Source classification

bool ScenePassCapture_IsPostOpaqueSource(EScenePassCaptureSource Source)
{
	switch (Source)
	{
	case EScenePassCaptureSource::SceneColorBeforeBloom:
	case EScenePassCaptureSource::SceneColorAfterTonemap:
	case EScenePassCaptureSource::SeparateTranslucency:
		return false;
	default:
		// Lumen is captured in PreRenderViewFamily, not at the post-opaque hook. See the note there.
		return !ScenePassCapture_IsLumenSource(Source);
	}
}

bool FScenePassCaptureFrameTargets::Wants(EScenePassCaptureSource Source) const
{
	for (const FScenePassCaptureResolvedEntry& Entry : Entries)
	{
		if (Entry.Source == Source)
		{
			return true;
		}
	}
	return false;
}

bool FScenePassCaptureFrameTargets::WantsPostOpaque() const
{
	for (const FScenePassCaptureResolvedEntry& Entry : Entries)
	{
		if (ScenePassCapture_IsPostOpaqueSource(Entry.Source))
		{
			return true;
		}
	}
	return false;
}

// ------------------------------------------------------------------------------------------------
// Blit helpers

namespace
{
	/** Resolves a post-opaque source to the renderer texture that backs it, plus how the shader should decode it. */
	bool ResolvePostOpaqueSource(EScenePassCaptureSource Source, const FSceneTextureUniformParameters& SceneTextures, FRDGTextureRef SceneColorTexture, bool bDecodeNormals, FRDGTextureRef& OutTexture, EScenePassCaptureCopyMode& OutMode, FVector4f& OutChannelMask)
	{
		OutTexture = nullptr;
		OutMode = EScenePassCaptureCopyMode::Copy;
		OutChannelMask = FVector4f(1.0f, 0.0f, 0.0f, 0.0f);

		switch (Source)
		{
		case EScenePassCaptureSource::SceneColorPreTranslucency:
			OutTexture = SceneColorTexture ? SceneColorTexture : SceneTextures.SceneColorTexture;
			break;

		case EScenePassCaptureSource::SceneDepthWorldUnits:
			OutTexture = SceneTextures.SceneDepthTexture;
			OutMode = EScenePassCaptureCopyMode::LinearDepth;
			break;

		case EScenePassCaptureSource::SceneDepthDeviceZ:
			OutTexture = SceneTextures.SceneDepthTexture;
			OutMode = EScenePassCaptureCopyMode::DeviceDepth;
			break;

		case EScenePassCaptureSource::WorldNormal:
			OutTexture = SceneTextures.GBufferATexture;
			OutMode = bDecodeNormals ? EScenePassCaptureCopyMode::DecodeNormal : EScenePassCaptureCopyMode::Copy;
			break;

		case EScenePassCaptureSource::BaseColor:
			OutTexture = SceneTextures.GBufferCTexture;
			break;

		case EScenePassCaptureSource::Metallic:
			OutTexture = SceneTextures.GBufferBTexture;
			OutMode = EScenePassCaptureCopyMode::Channel;
			OutChannelMask = FVector4f(1.0f, 0.0f, 0.0f, 0.0f);
			break;

		case EScenePassCaptureSource::Specular:
			OutTexture = SceneTextures.GBufferBTexture;
			OutMode = EScenePassCaptureCopyMode::Channel;
			OutChannelMask = FVector4f(0.0f, 1.0f, 0.0f, 0.0f);
			break;

		case EScenePassCaptureSource::Roughness:
			OutTexture = SceneTextures.GBufferBTexture;
			OutMode = EScenePassCaptureCopyMode::Channel;
			OutChannelMask = FVector4f(0.0f, 0.0f, 1.0f, 0.0f);
			break;

		case EScenePassCaptureSource::Velocity:
			OutTexture = SceneTextures.GBufferVelocityTexture;
			break;

		case EScenePassCaptureSource::AmbientOcclusion:
			OutTexture = SceneTextures.ScreenSpaceAOTexture;
			OutMode = EScenePassCaptureCopyMode::Channel;
			OutChannelMask = FVector4f(1.0f, 0.0f, 0.0f, 0.0f);
			break;

		case EScenePassCaptureSource::CustomDepthWorldUnits:
			OutTexture = SceneTextures.CustomDepthTexture;
			OutMode = EScenePassCaptureCopyMode::LinearDepth;
			break;

		case EScenePassCaptureSource::GBufferCustomData:
			OutTexture = SceneTextures.GBufferDTexture;
			break;

		case EScenePassCaptureSource::PrecomputedShadowFactors:
			OutTexture = SceneTextures.GBufferETexture;
			break;

		case EScenePassCaptureSource::Anisotropy:
			OutTexture = SceneTextures.GBufferFTexture;
			break;

		case EScenePassCaptureSource::ScenePartialDepth:
			OutTexture = SceneTextures.ScenePartialDepthTexture;
			OutMode = EScenePassCaptureCopyMode::DeviceDepth;
			break;

		default:
			return false;
		}

		return OutTexture != nullptr;
	}

	/** One blit from a renderer texture into a user render target. Uses a straight GPU copy when the formats line up exactly. */
	void AddCapturePass(FRDGBuilder& GraphBuilder, FRDGTextureRef SourceTexture, FIntRect SourceRect, FRDGTextureRef DestTexture, EScenePassCaptureCopyMode Mode, const FVector4f& ChannelMask, const FVector4f& InvDeviceZToWorldZ, float DepthNormalizeScale)
	{
		if (!SourceTexture || !DestTexture)
		{
			return;
		}

		const FIntPoint SourceExtent = SourceTexture->Desc.Extent;
		const FIntPoint DestExtent = DestTexture->Desc.Extent;

		if (SourceExtent.X <= 0 || SourceExtent.Y <= 0 || DestExtent.X <= 0 || DestExtent.Y <= 0)
		{
			return;
		}

		if (SourceRect.IsEmpty())
		{
			SourceRect = FIntRect(FIntPoint::ZeroValue, SourceExtent);
		}

		// Fast path: identical layout and format, so the GPU can just copy the bytes with no shader at all.
		const bool bExactMatch = Mode == EScenePassCaptureCopyMode::Copy && SourceTexture->Desc.Format == DestTexture->Desc.Format && SourceExtent == DestExtent && SourceRect.Min == FIntPoint::ZeroValue && SourceRect.Size() == SourceExtent;

		if (bExactMatch)
		{
			AddCopyTexturePass(GraphBuilder, SourceTexture, DestTexture);
			return;
		}

		const bool bPointSample = Mode == EScenePassCaptureCopyMode::LinearDepth || Mode == EScenePassCaptureCopyMode::DeviceDepth;
		FRHISamplerState* Sampler = bPointSample ? TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI() : TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

		const FGlobalShaderMap* ShaderMapForDimension = GetGlobalShaderMap(GMaxRHIFeatureLevel);
		const ETextureDimension Dimension = SourceTexture->Desc.Dimension;

		// Several Lumen history buffers are Texture2DArray, one slice per Substrate closure. Binding one
		// of those to a Texture2D parameter trips an RDG assert and takes the process down, so array
		// sources go through their own shader and anything else is refused outright.
		if (Dimension == ETextureDimension::Texture2DArray)
		{
			FScenePassCaptureCopyArrayPS::FParameters* ArrayParameters = GraphBuilder.AllocParameters<FScenePassCaptureCopyArrayPS::FParameters>();
			ArrayParameters->InputTextureArray = SourceTexture;
			ArrayParameters->InputSampler = Sampler;
			ArrayParameters->InputUVScale = FVector2f(float(SourceRect.Width()) / float(SourceExtent.X), float(SourceRect.Height()) / float(SourceExtent.Y));
			ArrayParameters->InputUVBias = FVector2f(float(SourceRect.Min.X) / float(SourceExtent.X), float(SourceRect.Min.Y) / float(SourceExtent.Y));
			ArrayParameters->OutputInvExtent = FVector2f(1.0f / float(DestExtent.X), 1.0f / float(DestExtent.Y));
			ArrayParameters->InvDeviceZToWorldZ = InvDeviceZToWorldZ;
			ArrayParameters->ChannelMask = ChannelMask;
			ArrayParameters->DepthNormalizeScale = DepthNormalizeScale;
			ArrayParameters->CopyMode = static_cast<uint32>(Mode);
			ArrayParameters->RenderTargets[0] = FRenderTargetBinding(DestTexture, ERenderTargetLoadAction::ENoAction);

			TShaderMapRef<FScenePassCaptureCopyArrayPS> ArrayPixelShader(ShaderMapForDimension);
			FPixelShaderUtils::AddFullscreenPass(GraphBuilder, ShaderMapForDimension, RDG_EVENT_NAME("ScenePassCaptureBlitArray"), ArrayPixelShader, ArrayParameters, FIntRect(FIntPoint::ZeroValue, DestExtent));
			return;
		}

		if (Dimension != ETextureDimension::Texture2D)
		{
			static bool bWarnedOnce = false;
			if (!bWarnedOnce)
			{
				bWarnedOnce = true;
				UE_LOG(LogScenePassCapture, Warning, TEXT("Skipping a capture whose source texture is not 2D (dimension %d). Copying it would crash."), int32(Dimension));
			}
			return;
		}

		FScenePassCaptureCopyPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FScenePassCaptureCopyPS::FParameters>();
		PassParameters->InputTexture = SourceTexture;
		PassParameters->InputSampler = Sampler;
		PassParameters->InputUVScale = FVector2f(float(SourceRect.Width()) / float(SourceExtent.X), float(SourceRect.Height()) / float(SourceExtent.Y));
		PassParameters->InputUVBias = FVector2f(float(SourceRect.Min.X) / float(SourceExtent.X), float(SourceRect.Min.Y) / float(SourceExtent.Y));
		PassParameters->OutputInvExtent = FVector2f(1.0f / float(DestExtent.X), 1.0f / float(DestExtent.Y));
		PassParameters->InvDeviceZToWorldZ = InvDeviceZToWorldZ;
		PassParameters->ChannelMask = ChannelMask;
		PassParameters->DepthNormalizeScale = DepthNormalizeScale;
		PassParameters->CopyMode = static_cast<uint32>(Mode);
		PassParameters->RenderTargets[0] = FRenderTargetBinding(DestTexture, ERenderTargetLoadAction::ENoAction);

		const FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
		TShaderMapRef<FScenePassCaptureCopyPS> PixelShader(ShaderMap);

		FPixelShaderUtils::AddFullscreenPass(GraphBuilder, ShaderMap, RDG_EVENT_NAME("ScenePassCaptureBlit"), PixelShader, PassParameters, FIntRect(FIntPoint::ZeroValue, DestExtent));
	}

	/** Copies one screen-pass texture into every target that asked for this source. */
	void CaptureScreenTextureIntoTargets(FRDGBuilder& GraphBuilder, const FScenePassCaptureFrameTargets& Targets, EScenePassCaptureSource Source, const FScreenPassTexture& ScreenTexture);

	/** Custom stencil needs its own pass because the SRV is a uint2 that cannot be sampled with a filtering sampler. */
	void AddStencilCapturePass(FRDGBuilder& GraphBuilder, FRDGTextureSRVRef StencilSRV, FIntRect SourceRect, FRDGTextureRef DestTexture, float StencilNormalizeRange)
	{
		if (!StencilSRV || !DestTexture)
		{
			return;
		}

		const FIntPoint DestExtent = DestTexture->Desc.Extent;
		if (DestExtent.X <= 0 || DestExtent.Y <= 0 || SourceRect.IsEmpty())
		{
			return;
		}

		FScenePassCaptureStencilPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FScenePassCaptureStencilPS::FParameters>();
		PassParameters->StencilTexture = StencilSRV;
		PassParameters->StencilPixelScale = FVector2f(float(SourceRect.Width()) / float(DestExtent.X), float(SourceRect.Height()) / float(DestExtent.Y));
		PassParameters->StencilPixelOffset = SourceRect.Min;
		PassParameters->StencilNormalizeScale = StencilNormalizeRange > 0.0f ? 1.0f / StencilNormalizeRange : 1.0f;
		PassParameters->RenderTargets[0] = FRenderTargetBinding(DestTexture, ERenderTargetLoadAction::ENoAction);

		const FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
		TShaderMapRef<FScenePassCaptureStencilPS> PixelShader(ShaderMap);

		FPixelShaderUtils::AddFullscreenPass(GraphBuilder, ShaderMap, RDG_EVENT_NAME("ScenePassCaptureStencil"), PixelShader, PassParameters, FIntRect(FIntPoint::ZeroValue, DestExtent));
	}

	void CaptureScreenTextureIntoTargets(FRDGBuilder& GraphBuilder, const FScenePassCaptureFrameTargets& Targets, EScenePassCaptureSource Source, const FScreenPassTexture& ScreenTexture)
	{
		for (const FScenePassCaptureResolvedEntry& Entry : Targets.Entries)
		{
			if (Entry.Source != Source)
			{
				continue;
			}

			FRDGTextureRef DestTexture = RegisterExternalTexture(GraphBuilder, Entry.TargetRHI, TEXT("ScenePassCaptureTarget"));
			AddCapturePass(GraphBuilder, ScreenTexture.Texture, ScreenTexture.ViewRect, DestTexture, EScenePassCaptureCopyMode::Copy, FVector4f(1.0f, 0.0f, 0.0f, 0.0f), FVector4f(0.0f, 0.0f, 0.0f, 0.0f), 0.0f);
		}
	}
}

// ------------------------------------------------------------------------------------------------
// FScenePassCaptureViewExtension

FScenePassCaptureViewExtension::FScenePassCaptureViewExtension(const FAutoRegister& AutoRegister)
	: FSceneViewExtensionBase(AutoRegister)
{
	PostOpaqueHandle = GetRendererModule().RegisterPostOpaqueRenderDelegate(FPostOpaqueRenderDelegate::CreateRaw(this, &FScenePassCaptureViewExtension::OnPostOpaqueRender));
}

FScenePassCaptureViewExtension::~FScenePassCaptureViewExtension()
{
	// Shutdown() unhooks us from the renderer and flushes. Skipping it leaves the render thread holding a dangling this.
	ensureMsgf(bShutdown, TEXT("FScenePassCaptureViewExtension destroyed without Shutdown() being called first."));
}

void FScenePassCaptureViewExtension::Shutdown()
{
	check(IsInGameThread());

	if (bShutdown)
	{
		return;
	}

	bShutdown = true;
	bEnabled = false;
	bSingleFrameRequested = false;
	Profile.Reset();

	if (PostOpaqueHandle.IsValid())
	{
		GetRendererModule().RemovePostOpaqueRenderDelegate(PostOpaqueHandle);
		PostOpaqueHandle.Reset();
	}

	// Drain anything already in flight that still points at this object.
	FlushRenderingCommands();

	RenderThreadTargets.Reset();
}

void FScenePassCaptureViewExtension::SetProfile(UScenePassCaptureProfile* InProfile)
{
	check(IsInGameThread());
	Profile = InProfile;
}

bool FScenePassCaptureViewExtension::IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const
{
	// Context.Viewport is null for scene captures, thumbnail renders and other offscreen renders.
	// Restricting to real viewports keeps us off every SceneCaptureComponent in the level.
	return !bShutdown && Context.Viewport != nullptr && (bEnabled || bSingleFrameRequested) && Profile.IsValid();
}

void FScenePassCaptureViewExtension::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
	check(IsInGameThread());

	if (bShutdown)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(ScenePassCapture_BuildSnapshot);

	UScenePassCaptureProfile* ActiveProfile = Profile.Get();
	const bool bShouldCapture = (bEnabled || bSingleFrameRequested) && ActiveProfile && ActiveProfile->HasAnyWork() && CVarScenePassCaptureEnabled.GetValueOnGameThread() != 0;

	bSingleFrameRequested = false;

	TSharedPtr<FScenePassCaptureFrameTargets, ESPMode::ThreadSafe> Snapshot;

	if (bShouldCapture)
	{
		// Size from the view rect, not the backing render target. A camera with Constrain Aspect Ratio
		// renders into a letterboxed sub-rect of the viewport, and sizing targets to the full viewport
		// would stretch that image across the wrong aspect. UnscaledViewRect is the constrained region
		// in viewport pixels, which is exactly the pixels that actually get captured.
		FIntPoint ViewSize = FIntPoint::ZeroValue;
		if (InViewFamily.Views.Num() > 0 && InViewFamily.Views[0])
		{
			ViewSize = InViewFamily.Views[0]->UnscaledViewRect.Size();
		}
		if ((ViewSize.X <= 0 || ViewSize.Y <= 0) && InViewFamily.RenderTarget)
		{
			ViewSize = InViewFamily.RenderTarget->GetSizeXY();
		}

		Snapshot = MakeShared<FScenePassCaptureFrameTargets, ESPMode::ThreadSafe>();
		Snapshot->Entries.Reserve(ActiveProfile->Passes.Num());

		for (const FScenePassCaptureEntry& Entry : ActiveProfile->Passes)
		{
			UTextureRenderTarget2D* Target = Entry.Target;
			if (!Entry.bEnabled || !Target)
			{
				continue;
			}

			if (ActiveProfile->bResizeTargetsToViewport && ViewSize.X > 0 && ViewSize.Y > 0)
			{
				const FIntPoint Desired = ScenePassCapture_ApplyResolutionScale(ViewSize, Entry.ResolutionScale, Entry.bAlignSizeToMultipleOfTwo);
				if (Target->SizeX != Desired.X || Target->SizeY != Desired.Y)
				{
					Target->ResizeTarget(Desired.X, Desired.Y);
				}
			}

			FTextureRenderTargetResource* Resource = Target->GameThread_GetRenderTargetResource();
			if (!Resource)
			{
				continue;
			}

			FTextureRHIRef TargetRHI = Resource->GetRenderTargetTexture();
			if (!TargetRHI.IsValid())
			{
				TargetRHI = Resource->TextureRHI;
			}
			if (!TargetRHI.IsValid())
			{
				continue;
			}

			FScenePassCaptureResolvedEntry& Resolved = Snapshot->Entries.AddDefaulted_GetRef();
			Resolved.Source = Entry.Source;
			Resolved.TargetRHI = MoveTemp(TargetRHI);
			Resolved.DepthNormalizeRange = Entry.DepthNormalizeRange;
			Resolved.StencilNormalizeRange = Entry.StencilNormalizeRange;
			Resolved.bDecodeNormals = Entry.bDecodeNormals;
		}

		if (Snapshot->Entries.Num() == 0)
		{
			Snapshot.Reset();
		}
	}

	// Hand the immutable snapshot over. Render commands are ordered, so this lands before this family renders.
	TSharedPtr<const FScenePassCaptureFrameTargets, ESPMode::ThreadSafe> Immutable = Snapshot;
	FScenePassCaptureViewExtension* Self = this;

	ENQUEUE_RENDER_COMMAND(ScenePassCaptureUpdateTargets)([Self, Immutable](FRHICommandListImmediate&)
	{
		Self->RenderThreadTargets = Immutable;
	});
}

void FScenePassCaptureViewExtension::PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily)
{
	// The post-opaque delegate fires for every scene render in the engine. These brackets tell it
	// which of those renders is the family we actually opted into.
	bInsideTrackedFamily_RT = true;
	bCapturedPostOpaqueThisFamily_RT = false;

	// Lumen has to be read here rather than at post-opaque. Lumen's gather explicitly nulls its history
	// pointers during graph construction and only refills them via QueueTextureExtraction at graph
	// execution, so by post-opaque they are always null. At this point they still hold last frame's
	// textures, which is the one-frame-old history these buffers are anyway.
	if (InViewFamily.Views.Num() > 0 && InViewFamily.Views[0])
	{
		const FSceneView& View = *InViewFamily.Views[0];

		ScenePassCapture_LogLumenState(View);

		const TSharedPtr<const FScenePassCaptureFrameTargets, ESPMode::ThreadSafe> Targets = RenderThreadTargets;
		if (Targets.IsValid())
		{
			RDG_EVENT_SCOPE_STAT(GraphBuilder, ScenePassCapture, "ScenePassCaptureLumen");

			for (const FScenePassCaptureResolvedEntry& Entry : Targets->Entries)
			{
				if (!ScenePassCapture_IsLumenSource(Entry.Source))
				{
					continue;
				}

				FIntRect LumenSourceRect;
				if (FRDGTextureRef LumenTexture = ScenePassCapture_ResolveLumenTexture(GraphBuilder, View, Entry.Source, LumenSourceRect))
				{
					// The rect the buffer itself reports is the allocation, not the valid area, so the
					// view rect from post-opaque wins whenever we have one.
					if (!LastViewRect_RT.IsEmpty())
					{
						LumenSourceRect = LastViewRect_RT;
					}

					FRDGTextureRef DestTexture = RegisterExternalTexture(GraphBuilder, Entry.TargetRHI, TEXT("ScenePassCaptureTarget"));

					// LumenSourceRect is the valid region. Copying the full padded extent would drag
					// uninitialised garbage in along the bottom and right and squeeze the image.
					AddCapturePass(GraphBuilder, LumenTexture, LumenSourceRect, DestTexture, EScenePassCaptureCopyMode::Copy, FVector4f(1.0f, 0.0f, 0.0f, 0.0f), FVector4f(0.0f, 0.0f, 0.0f, 0.0f), 0.0f);
				}
			}
		}
	}
}

void FScenePassCaptureViewExtension::PostRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily)
{
	bInsideTrackedFamily_RT = false;
}

void FScenePassCaptureViewExtension::OnPostOpaqueRender(FPostOpaqueRenderParameters& Parameters)
{
	if (!bInsideTrackedFamily_RT || bCapturedPostOpaqueThisFamily_RT)
	{
		return;
	}

	// Recorded before any early-out below, so it is available even when only Lumen sources are in use
	// and this hook has no targets of its own to write.
	LastViewRect_RT = Parameters.ViewportRect;


	const TSharedPtr<const FScenePassCaptureFrameTargets, ESPMode::ThreadSafe> Targets = RenderThreadTargets;
	if (!Targets.IsValid() || !Targets->WantsPostOpaque())
	{
		return;
	}

	if (!Parameters.GraphBuilder || !Parameters.SceneTexturesUniformParams)
	{
		// No uniform params means the mobile renderer, which has a different texture set and is not supported.
		return;
	}

	const FSceneTextureUniformParameters* SceneTextures = Parameters.SceneTexturesUniformParams->GetContents();
	if (!SceneTextures)
	{
		return;
	}

	// Only the first view of the family is captured. Split screen and stereo would otherwise fight over one target.
	bCapturedPostOpaqueThisFamily_RT = true;

	FRDGBuilder& GraphBuilder = *Parameters.GraphBuilder;
	const FIntRect ViewRect = Parameters.ViewportRect;
	const FVector4f InvDeviceZToWorldZ = CreateInvDeviceZToWorldZTransform(Parameters.ProjMatrix);

	RDG_EVENT_SCOPE_STAT(GraphBuilder, ScenePassCapture, "ScenePassCapture");

	for (const FScenePassCaptureResolvedEntry& Entry : Targets->Entries)
	{
		if (!ScenePassCapture_IsPostOpaqueSource(Entry.Source))
		{
			continue;
		}

		FRDGTextureRef DestTexture = RegisterExternalTexture(GraphBuilder, Entry.TargetRHI, TEXT("ScenePassCaptureTarget"));

		if (Entry.Source == EScenePassCaptureSource::CustomStencil)
		{
			AddStencilCapturePass(GraphBuilder, SceneTextures->CustomStencilTexture, ViewRect, DestTexture, Entry.StencilNormalizeRange);
			continue;
		}

		FRDGTextureRef SourceTexture = nullptr;
		EScenePassCaptureCopyMode Mode = EScenePassCaptureCopyMode::Copy;
		FVector4f ChannelMask(1.0f, 0.0f, 0.0f, 0.0f);

		if (!ResolvePostOpaqueSource(Entry.Source, *SceneTextures, Parameters.ColorTexture, Entry.bDecodeNormals, SourceTexture, Mode, ChannelMask))
		{
			// Velocity and SSAO are legitimately absent on frames where those passes did not run.
			continue;
		}

		const float DepthNormalizeScale = Entry.DepthNormalizeRange > 0.0f ? 1.0f / Entry.DepthNormalizeRange : 0.0f;

		AddCapturePass(GraphBuilder, SourceTexture, ViewRect, DestTexture, Mode, ChannelMask, InvDeviceZToWorldZ, DepthNormalizeScale);
	}
}

void FScenePassCaptureViewExtension::SubscribeToPostProcessingPass(EPostProcessingPass Pass, const FSceneView& InView, FPostProcessingPassDelegateArray& InOutPassCallbacks, bool bIsPassEnabled)
{
	const TSharedPtr<const FScenePassCaptureFrameTargets, ESPMode::ThreadSafe> Targets = RenderThreadTargets;
	if (!Targets.IsValid())
	{
		return;
	}

	// MotionBlur is the BL_SceneColorBeforeBloom blend point, Tonemap is BL_SceneColorAfterTonemapping.
	if (Pass == EPostProcessingPass::MotionBlur && (Targets->Wants(EScenePassCaptureSource::SceneColorBeforeBloom) || Targets->Wants(EScenePassCaptureSource::SeparateTranslucency)))
	{
		InOutPassCallbacks.Add(FPostProcessingPassDelegate::CreateRaw(this, &FScenePassCaptureViewExtension::OnPostProcessPass_RenderThread, EScenePassCaptureSource::SceneColorBeforeBloom));
	}
	else if (Pass == EPostProcessingPass::Tonemap && Targets->Wants(EScenePassCaptureSource::SceneColorAfterTonemap))
	{
		InOutPassCallbacks.Add(FPostProcessingPassDelegate::CreateRaw(this, &FScenePassCaptureViewExtension::OnPostProcessPass_RenderThread, EScenePassCaptureSource::SceneColorAfterTonemap));
	}
}

FScreenPassTexture FScenePassCaptureViewExtension::OnPostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& Inputs, EScenePassCaptureSource Source)
{
	// CopyFromSlice honours OverrideOutput for us, which matters on the frames where we end up last in the chain.
	// Only the returned scene color may use OverrideOutput. Side taps below must never touch it.
	const FScreenPassTexture SceneColor = FScreenPassTexture::CopyFromSlice(GraphBuilder, Inputs.GetInput(EPostProcessMaterialInput::SceneColor), Inputs.OverrideOutput);

	const TSharedPtr<const FScenePassCaptureFrameTargets, ESPMode::ThreadSafe> Targets = RenderThreadTargets;

	if (Targets.IsValid())
	{
		RDG_EVENT_SCOPE_STAT(GraphBuilder, ScenePassCapture, "ScenePassCapture");

		if (SceneColor.IsValid())
		{
			CaptureScreenTextureIntoTargets(GraphBuilder, *Targets, Source, SceneColor);
		}

		// Separate translucency is read at the before-bloom point, where the chain has it populated.
		if (Source == EScenePassCaptureSource::SceneColorBeforeBloom && Targets->Wants(EScenePassCaptureSource::SeparateTranslucency))
		{
			const FScreenPassTexture Translucency = FScreenPassTexture::CopyFromSlice(GraphBuilder, Inputs.GetInput(EPostProcessMaterialInput::SeparateTranslucency));
			if (Translucency.IsValid())
			{
				CaptureScreenTextureIntoTargets(GraphBuilder, *Targets, EScenePassCaptureSource::SeparateTranslucency, Translucency);
			}
		}
	}

	// We are a tap, not a filter. Whatever came in goes back out untouched.
	return SceneColor;
}
