// Copyright Exiin Game Studio. All Rights Reserved.

#include "ScenePassCaptureAnalysis.h"

#include "ScenePassCaptureFormatRules.h"
#include "ScenePassCaptureProfile.h"

#include "Engine/TextureRenderTarget2D.h"
#include "HAL/IConsoleManager.h"
#include "PixelFormat.h"
#include "UObject/UObjectIterator.h"

#define LOCTEXT_NAMESPACE "ScenePassCaptureAnalysis"

namespace
{
	/** Sources read out of the classic GBuffer layout, which Substrate replaces. */
	bool IsGBufferSource(EScenePassCaptureSource Source)
	{
		switch (Source)
		{
		case EScenePassCaptureSource::WorldNormal:
		case EScenePassCaptureSource::BaseColor:
		case EScenePassCaptureSource::Metallic:
		case EScenePassCaptureSource::Specular:
		case EScenePassCaptureSource::Roughness:
			return true;
		default:
			return false;
		}
	}

	bool NeedsCustomDepth(EScenePassCaptureSource Source)
	{
		return Source == EScenePassCaptureSource::CustomDepthWorldUnits || Source == EScenePassCaptureSource::CustomStencil;
	}

	int32 GetCVarInt(const TCHAR* Name, int32 Fallback)
	{
		if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			return Variable->GetInt();
		}
		return Fallback;
	}

	FText GetSourceDisplayName(EScenePassCaptureSource Source)
	{
		if (const UEnum* EnumPtr = StaticEnum<EScenePassCaptureSource>())
		{
			return EnumPtr->GetDisplayNameTextByValue(static_cast<int64>(Source));
		}
		return LOCTEXT("UnknownSource", "Unknown");
	}
}

double FScenePassCaptureAnalysis::EstimateMillisecondsAt(double BytesPerSecond) const
{
	if (BytesPerSecond <= 0.0)
	{
		return 0.0;
	}
	return (static_cast<double>(BytesMovedPerFrame) / BytesPerSecond) * 1000.0;
}

int32 FScenePassCaptureAnalysis::CountIssues(EScenePassCaptureIssueSeverity Severity) const
{
	int32 Count = 0;
	for (const FScenePassCaptureIssue& Issue : Issues)
	{
		if (Issue.Severity == Severity)
		{
			++Count;
		}
	}
	return Count;
}

FText FormatScenePassCaptureBytes(int64 Bytes)
{
	const double Megabytes = static_cast<double>(Bytes) / (1024.0 * 1024.0);

	if (Megabytes >= 1024.0)
	{
		return FText::FromString(FString::Printf(TEXT("%.2f GB"), Megabytes / 1024.0));
	}
	if (Megabytes >= 1.0)
	{
		return FText::FromString(FString::Printf(TEXT("%.1f MB"), Megabytes));
	}
	return FText::FromString(FString::Printf(TEXT("%.0f KB"), static_cast<double>(Bytes) / 1024.0));
}

FScenePassCaptureAnalysis AnalyzeScenePassCaptureProfile(const UScenePassCaptureProfile& Profile)
{
	FScenePassCaptureAnalysis Analysis;

	const bool bSubstrateEnabled = GetCVarInt(TEXT("r.Substrate"), 0) != 0;
	const int32 CustomDepthMode = GetCVarInt(TEXT("r.CustomDepth"), 1);

	TMap<const UTextureRenderTarget2D*, int32> TargetUsage;

	for (int32 Index = 0; Index < Profile.Passes.Num(); ++Index)
	{
		const FScenePassCaptureEntry& Entry = Profile.Passes[Index];

		if (!Entry.bEnabled)
		{
			continue;
		}

		const FText SourceName = GetSourceDisplayName(Entry.Source);

		if (!Entry.Target)
		{
			Analysis.Issues.Add({ Index, EScenePassCaptureIssueSeverity::Error, FText::Format(LOCTEXT("NoTarget", "{0}: no render target assigned, this entry does nothing."), SourceName) });
			continue;
		}

		++Analysis.ActiveEntryCount;
		TargetUsage.FindOrAdd(Entry.Target)++;

		const UTextureRenderTarget2D* Target = Entry.Target;
		const EPixelFormat Format = Target->GetFormat();
		const int64 BlockBytes = GPixelFormats[Format].BlockBytes;
		const int64 PixelCount = static_cast<int64>(Target->SizeX) * static_cast<int64>(Target->SizeY);
		const int64 TargetBytes = PixelCount * BlockBytes;

		Analysis.TotalTargetBytes += TargetBytes;

		// A blit reads roughly a target's worth of source and writes a target's worth of destination.
		Analysis.BytesMovedPerFrame += TargetBytes * 2;

		if (Target->SizeX <= 0 || Target->SizeY <= 0)
		{
			Analysis.Issues.Add({ Index, EScenePassCaptureIssueSeverity::Error, FText::Format(LOCTEXT("ZeroSize", "{0}: target has zero size."), SourceName) });
		}

		// Same rules the details panel warning and the Create Target button use, so they can never disagree.
		FText FormatMessage;
		bool bFormatIsError = false;
		if (!ScenePassCapture_ValidateTargetFormat(Entry, FormatMessage, bFormatIsError))
		{
			Analysis.Issues.Add({ Index, bFormatIsError ? EScenePassCaptureIssueSeverity::Error : EScenePassCaptureIssueSeverity::Warning, FormatMessage });
		}

		if (IsGBufferSource(Entry.Source) && bSubstrateEnabled)
		{
			Analysis.Issues.Add({ Index, EScenePassCaptureIssueSeverity::Error, FText::Format(LOCTEXT("SubstrateBreaksGBuffer", "{0}: Substrate is enabled, so the classic GBuffer layout this source reads no longer holds. The result will be wrong."), SourceName) });
		}

		if (NeedsCustomDepth(Entry.Source) && CustomDepthMode == 0)
		{
			Analysis.Issues.Add({ Index, EScenePassCaptureIssueSeverity::Error, FText::Format(LOCTEXT("CustomDepthOff", "{0}: Custom Depth is disabled in project settings, so this buffer is never written."), SourceName) });
		}

		if (Entry.Source == EScenePassCaptureSource::CustomStencil && CustomDepthMode != 3)
		{
			Analysis.Issues.Add({ Index, EScenePassCaptureIssueSeverity::Error, FText::Format(LOCTEXT("CustomStencilOff", "{0}: Custom Depth Stencil is not enabled. Set Custom Depth-Stencil Pass to \"Enabled with Stencil\" in project settings."), SourceName) });
		}

		if (Entry.Source == EScenePassCaptureSource::Velocity)
		{
			Analysis.Issues.Add({ Index, EScenePassCaptureIssueSeverity::Info, FText::Format(LOCTEXT("VelocityConditional", "{0}: only written on frames where a velocity pass actually runs. The target is left untouched otherwise."), SourceName) });
		}

		if (Entry.Source == EScenePassCaptureSource::AmbientOcclusion)
		{
			// Under Lumen the renderer never runs the separate SSAO pass and substitutes a white dummy
			// texture, so the capture is a fully white image. In a single channel target that previews as red.
			if (GetCVarInt(TEXT("r.DynamicGlobalIlluminationMethod"), 0) == 1)
			{
				Analysis.Issues.Add({ Index, EScenePassCaptureIssueSeverity::Error, FText::Format(LOCTEXT("AOUnderLumen", "{0}: Lumen is the Dynamic Global Illumination Method, so the separate SSAO pass never runs and the renderer substitutes a plain white texture. This target will be solid white, which previews as solid red in a single channel format. Use the \"Lumen: Short Range AO (Screen)\" source instead, which is Lumen's own occlusion."), SourceName) });
			}
			else
			{
				Analysis.Issues.Add({ Index, EScenePassCaptureIssueSeverity::Info, FText::Format(LOCTEXT("AOConditional", "{0}: only written on frames where SSAO actually runs. It falls back to white otherwise."), SourceName) });
			}
		}

		if (ScenePassCapture_IsLumenSource(Entry.Source))
		{
#if SCENEPASSCAPTURE_LUMEN
			if (GetCVarInt(TEXT("r.DynamicGlobalIlluminationMethod"), 0) != 1)
			{
				Analysis.Issues.Add({ Index, EScenePassCaptureIssueSeverity::Error, FText::Format(LOCTEXT("LumenSourceWithoutLumen", "{0}: Lumen is not the Dynamic Global Illumination Method, so this buffer never exists and the target is never written."), SourceName) });
			}
			else
			{
				Analysis.Issues.Add({ Index, EScenePassCaptureIssueSeverity::Info, FText::Format(LOCTEXT("LumenHistoryBuffer", "{0}: this is Lumen's denoised temporal history, so it is the previous frame's result reprojected and may be at a downsampled resolution."), SourceName) });
			}
#else
			Analysis.Issues.Add({ Index, EScenePassCaptureIssueSeverity::Error, FText::Format(LOCTEXT("LumenDisabledAtBuild", "{0}: Lumen sources were compiled out. Set bEnableLumenSources back to true in ScenePassCapture.Build.cs and rebuild."), SourceName) });
#endif
		}

		if (Entry.Source == EScenePassCaptureSource::Anisotropy)
		{
			Analysis.Issues.Add({ Index, EScenePassCaptureIssueSeverity::Info, FText::Format(LOCTEXT("AnisotropyConditional", "{0}: GBufferF is only allocated when anisotropic materials are enabled for the project. It is empty otherwise."), SourceName) });
		}

		if (Entry.Source == EScenePassCaptureSource::ScenePartialDepth)
		{
			Analysis.Issues.Add({ Index, EScenePassCaptureIssueSeverity::Info, FText::Format(LOCTEXT("PartialDepthConditional", "{0}: only populated on frames where a pass that needs partial depth ran."), SourceName) });
		}
	}

	for (const TPair<const UTextureRenderTarget2D*, int32>& Pair : TargetUsage)
	{
		if (Pair.Value > 1)
		{
			Analysis.Issues.Add({ INDEX_NONE, EScenePassCaptureIssueSeverity::Warning, FText::Format(LOCTEXT("DuplicateTarget", "{0} is used by {1} entries. They will overwrite each other and only the last one survives."), FText::FromString(Pair.Key->GetName()), FText::AsNumber(Pair.Value)) });
		}
	}

	if (Analysis.ActiveEntryCount == 0)
	{
		Analysis.Issues.Add({ INDEX_NONE, EScenePassCaptureIssueSeverity::Warning, LOCTEXT("NothingToDo", "No enabled entry has a render target, so this profile captures nothing.") });
	}

	return Analysis;
}

#undef LOCTEXT_NAMESPACE
