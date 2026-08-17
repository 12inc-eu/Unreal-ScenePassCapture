// Copyright Exiin Game Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RenderGraphFwd.h"
#include "ScenePassCaptureTypes.h"

class FRDGBuilder;
class FSceneView;

/**
 * Bridge to Lumen's screen-space denoiser history buffers.
 *
 * Lumen exposes nothing publicly, so the implementation reaches into Renderer private headers. All
 * of that is confined to ScenePassCaptureLumen.cpp and gated on SCENEPASSCAPTURE_LUMEN, which is set
 * in ScenePassCapture.Build.cs. With the switch off this returns null and nothing else notices.
 *
 * Returns null whenever the matching Lumen feature is not running.
 *
 * OutSourceRect is the valid region inside the returned texture. These buffers are allocated at the
 * padded scene texture extent, so the area past the view rect is uninitialised garbage and must not
 * be copied. An empty rect means "use the whole texture".
 */
FRDGTextureRef ScenePassCapture_ResolveLumenTexture(FRDGBuilder& GraphBuilder, const FSceneView& View, EScenePassCaptureSource Source, FIntRect& OutSourceRect);

/**
 * Logs which Lumen history buffers currently exist, roughly once a second.
 * No-op unless r.ScenePassCapture.LumenDebug is 1. Runs whether or not any Lumen source is in use,
 * so it can be used to find out what is available before wiring a profile up.
 */
void ScenePassCapture_LogLumenState(const FSceneView& View);
