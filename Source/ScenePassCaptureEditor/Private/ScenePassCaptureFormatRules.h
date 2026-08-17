// Copyright Exiin Game Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Internationalization/Text.h"

struct FScenePassCaptureEntry;

/**
 * One place that decides what render target format a given source needs.
 *
 * Both the "Create Target" button and the profile's validation pane go through here, so the
 * format the button makes is by construction the format the validator asks for.
 */

/** The format this entry should be captured into, taking Depth Normalize Range and Decode Normals into account. */
ETextureRenderTargetFormat ScenePassCapture_GetRecommendedFormat(const FScenePassCaptureEntry& Entry);

/** "RGBA16f", "R32f", and so on. */
FText ScenePassCapture_GetFormatDisplayName(ETextureRenderTargetFormat Format);

/** A sensible asset name for a new target, for example "RT_SceneDepth". */
FString ScenePassCapture_SuggestTargetName(const FScenePassCaptureEntry& Entry);

/**
 * Checks the assigned target's format against what the source actually needs.
 * Returns false when there is something to report, and fills in the message.
 * bOutIsError separates "this will be visibly wrong" from "this works but you are losing something".
 */
bool ScenePassCapture_ValidateTargetFormat(const FScenePassCaptureEntry& Entry, FText& OutMessage, bool& bOutIsError);
