// Copyright Exiin Game Studio. All Rights Reserved.

#include "ScenePassCaptureProfile.h"

#include "Engine/TextureRenderTarget2D.h"

bool UScenePassCaptureProfile::HasAnyWork() const
{
	for (const FScenePassCaptureEntry& Entry : Passes)
	{
		if (Entry.bEnabled && Entry.Target != nullptr)
		{
			return true;
		}
	}
	return false;
}
