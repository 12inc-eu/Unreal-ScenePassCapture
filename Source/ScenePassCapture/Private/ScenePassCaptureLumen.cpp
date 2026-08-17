// Copyright Exiin Game Studio. All Rights Reserved.

#include "ScenePassCaptureLumen.h"

#include "HAL/IConsoleManager.h"
#include "SceneView.h"

#if SCENEPASSCAPTURE_LUMEN

// Renderer private headers. Not a stable API. See the note in ScenePassCapture.Build.cs, and expect
// this file to be the first casualty of an engine upgrade.
#include "ScenePrivate.h"
// ScenePrivate.h only forward-declares FSceneViewState. This is where it and FLumenViewState live.
#include "SceneViewState.h"
#include "RenderGraphBuilder.h"

#endif

static TAutoConsoleVariable<int32> CVarScenePassCaptureLumenDebug(
	TEXT("r.ScenePassCapture.LumenDebug"),
	0,
	TEXT("Log which Lumen history buffers currently exist, once per second. Use this when a Lumen source captures black."),
	ECVF_RenderThreadSafe);

#if SCENEPASSCAPTURE_LUMEN

namespace
{
	struct FLumenCandidate
	{
		const TCHAR* Name = nullptr;
		const TRefCountPtr<IPooledRenderTarget>* Buffer = nullptr;

		// Valid region inside the buffer. These are allocated at the padded scene texture extent, so
		// copying the full extent drags uninitialised padding in along the bottom and right edges.
		FIntRect ViewRect;
	};

	using FLumenCandidates = TArray<FLumenCandidate, TInlineAllocator<2>>;

	/**
	 * UE 5.8 has two Lumen final gather paths, chosen by Lumen::GetFinalGatherMethod(): the classic
	 * ScreenProbeGather and the newer ReSTIRGather. They keep their history in different places and
	 * only the active one is ever populated, so every source lists both and takes whichever is live.
	 */
	void GatherCandidates(const FLumenViewState& Lumen, EScenePassCaptureSource Source, FLumenCandidates& Out)
	{
		const FScreenProbeGatherTemporalState& ScreenProbe = Lumen.ScreenProbeGatherState;
		const FReSTIRTemporalAccumulationState& ReSTIR = Lumen.ReSTIRGatherState.TemporalAccumulationState;

		// ScreenProbeGather records the resolution it wrote at; ReSTIR and reflections carry a rect.
		const FIntRect ScreenProbeRect(FIntPoint::ZeroValue, ScreenProbe.HistoryEffectiveResolution);
		const FIntRect ReSTIRRect = ReSTIR.DiffuseIndirectHistoryViewRect;
		const FIntRect ReflectionRect = Lumen.ReflectionState.HistoryViewRect;

		switch (Source)
		{
		case EScenePassCaptureSource::LumenDiffuseIndirect:
			Out.Add({ TEXT("ScreenProbeGather.DiffuseIndirectHistoryRT"), &ScreenProbe.DiffuseIndirectHistoryRT, ScreenProbeRect });
			Out.Add({ TEXT("ReSTIRGather.DiffuseIndirectHistoryRT"), &ReSTIR.DiffuseIndirectHistoryRT, ReSTIRRect });
			break;

		case EScenePassCaptureSource::LumenRoughSpecularIndirect:
			Out.Add({ TEXT("ScreenProbeGather.RoughSpecularIndirectHistoryRT"), &ScreenProbe.RoughSpecularIndirectHistoryRT, ScreenProbeRect });
			Out.Add({ TEXT("ReSTIRGather.RoughSpecularIndirectHistoryRT"), &ReSTIR.RoughSpecularIndirectHistoryRT, ReSTIRRect });
			break;

		case EScenePassCaptureSource::LumenShortRangeAO:
			Out.Add({ TEXT("ScreenProbeGather.ShortRangeAOHistoryRT"), &ScreenProbe.ShortRangeAOHistoryRT, ScreenProbeRect });
			break;

		case EScenePassCaptureSource::LumenBackfaceDiffuseIndirect:
			Out.Add({ TEXT("ScreenProbeGather.BackfaceDiffuseIndirectHistoryRT"), &ScreenProbe.BackfaceDiffuseIndirectHistoryRT, ScreenProbeRect });
			break;

		case EScenePassCaptureSource::LumenReflections:
			Out.Add({ TEXT("ReflectionState.SpecularAndSecondMomentHistory"), &Lumen.ReflectionState.SpecularAndSecondMomentHistory, ReflectionRect });
			break;

		default:
			break;
		}
	}

	FString DescribeBuffer(const TRefCountPtr<IPooledRenderTarget>& Buffer)
	{
		if (!Buffer.IsValid())
		{
			return TEXT("null");
		}

		const FPooledRenderTargetDesc& Desc = Buffer->GetDesc();
		return FString::Printf(TEXT("%dx%d fmt=%d"), Desc.Extent.X, Desc.Extent.Y, int32(Desc.Format));
	}
}

#endif // SCENEPASSCAPTURE_LUMEN

void ScenePassCapture_LogLumenState(const FSceneView& View)
{
	if (CVarScenePassCaptureLumenDebug.GetValueOnRenderThread() == 0)
	{
		return;
	}

	// Roughly once a second at 60fps. Enough to see state without drowning the log.
	static int32 FrameCounter = 0;
	if ((FrameCounter++ % 60) != 0)
	{
		return;
	}

#if SCENEPASSCAPTURE_LUMEN
	const FSceneViewState* ViewState = static_cast<const FSceneViewState*>(View.State);
	if (!ViewState)
	{
		UE_LOG(LogScenePassCapture, Display, TEXT("[LumenDebug] view has no ViewState, so there is no Lumen history at all. Scene captures and preview views behave this way."));
		return;
	}

	const FLumenViewState& Lumen = ViewState->Lumen;
	const FScreenProbeGatherTemporalState& ScreenProbe = Lumen.ScreenProbeGatherState;
	const FReSTIRTemporalAccumulationState& ReSTIR = Lumen.ReSTIRGatherState.TemporalAccumulationState;

	UE_LOG(LogScenePassCapture, Display, TEXT("[LumenDebug] ScreenProbeGather: Diffuse=%s RoughSpec=%s ShortRangeAO=%s Backface=%s"),
		*DescribeBuffer(ScreenProbe.DiffuseIndirectHistoryRT),
		*DescribeBuffer(ScreenProbe.RoughSpecularIndirectHistoryRT),
		*DescribeBuffer(ScreenProbe.ShortRangeAOHistoryRT),
		*DescribeBuffer(ScreenProbe.BackfaceDiffuseIndirectHistoryRT));

	UE_LOG(LogScenePassCapture, Display, TEXT("[LumenDebug] ReSTIRGather: Diffuse=%s RoughSpec=%s | Reflections=%s"),
		*DescribeBuffer(ReSTIR.DiffuseIndirectHistoryRT),
		*DescribeBuffer(ReSTIR.RoughSpecularIndirectHistoryRT),
		*DescribeBuffer(Lumen.ReflectionState.SpecularAndSecondMomentHistory));
#else
	UE_LOG(LogScenePassCapture, Display, TEXT("[LumenDebug] Lumen sources were compiled out (SCENEPASSCAPTURE_LUMEN=0)."));
#endif
}

FRDGTextureRef ScenePassCapture_ResolveLumenTexture(FRDGBuilder& GraphBuilder, const FSceneView& View, EScenePassCaptureSource Source, FIntRect& OutSourceRect)
{
	OutSourceRect = FIntRect();

#if SCENEPASSCAPTURE_LUMEN
	const FSceneViewState* ViewState = static_cast<const FSceneViewState*>(View.State);
	if (!ViewState)
	{
		return nullptr;
	}

	FLumenCandidates Candidates;
	GatherCandidates(ViewState->Lumen, Source, Candidates);

	for (const FLumenCandidate& Candidate : Candidates)
	{
		if (Candidate.Buffer && Candidate.Buffer->IsValid())
		{
			FRDGTextureRef Texture = GraphBuilder.RegisterExternalTexture(*Candidate.Buffer);

			// Clamp to the allocation: a stale rect from a previous resolution would sample padding.
			const FIntPoint Extent = Texture->Desc.Extent;
			FIntRect Rect = Candidate.ViewRect;
			Rect.Min.X = FMath::Clamp(Rect.Min.X, 0, Extent.X);
			Rect.Min.Y = FMath::Clamp(Rect.Min.Y, 0, Extent.Y);
			Rect.Max.X = FMath::Clamp(Rect.Max.X, Rect.Min.X, Extent.X);
			Rect.Max.Y = FMath::Clamp(Rect.Max.Y, Rect.Min.Y, Extent.Y);

			OutSourceRect = Rect;
			return Texture;
		}
	}

	// Null whenever the matching Lumen feature is off. r.ScenePassCapture.LumenDebug 1 says which.
	return nullptr;
#else
	return nullptr;
#endif
}
