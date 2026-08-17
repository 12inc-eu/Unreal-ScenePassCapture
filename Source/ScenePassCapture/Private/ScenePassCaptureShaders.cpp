// Copyright Exiin Game Studio. All Rights Reserved.

#include "ScenePassCaptureShaders.h"

IMPLEMENT_GLOBAL_SHADER(FScenePassCaptureCopyPS, "/Plugin/ScenePassCapture/Private/ScenePassCaptureCopy.usf", "MainPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FScenePassCaptureCopyArrayPS, "/Plugin/ScenePassCapture/Private/ScenePassCaptureCopy.usf", "MainArrayPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FScenePassCaptureStencilPS, "/Plugin/ScenePassCapture/Private/ScenePassCaptureCopy.usf", "StencilPS", SF_Pixel);
