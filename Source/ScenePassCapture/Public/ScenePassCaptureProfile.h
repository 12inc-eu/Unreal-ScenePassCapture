// Copyright Exiin Game Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ScenePassCaptureTypes.h"
#include "ScenePassCaptureProfile.generated.h"

/**
 * The data asset that drives the capture. Add one entry per pass you want mirrored into a render target,
 * then hand the profile to the UScenePassCaptureSubsystem for the world you want to capture.
 */
UCLASS(BlueprintType, meta = (DisplayName = "Scene Pass Capture Profile"))
class SCENEPASSCAPTURE_API UScenePassCaptureProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	/** The passes to mirror. Duplicate sources are allowed, for example the same depth at two resolutions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene Pass Capture")
	TArray<FScenePassCaptureEntry> Passes;

	/** Resize every target to match the rendered viewport before capturing. Costs a render target reallocation whenever the viewport changes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene Pass Capture")
	bool bResizeTargetsToViewport = false;

	/** Returns true if at least one entry is enabled and points at a render target. */
	bool HasAnyWork() const;
};
