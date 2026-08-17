// Copyright Exiin Game Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ScenePassCaptureTypes.generated.h"

class UTextureRenderTarget2D;

SCENEPASSCAPTURE_API DECLARE_LOG_CATEGORY_EXTERN(LogScenePassCapture, Log, All);

/**
 * Which renderer buffer to tap. The hook point differs per source, see the comments below.
 * Sources marked "post-opaque" are read at the end of the opaque pass, before translucency is composited.
 * Sources marked "post-process chain" are read from a callback inserted into the post-processing chain.
 */
UENUM(BlueprintType)
enum class EScenePassCaptureSource : uint8
{
	/** Post-opaque. HDR scene color with opaque and masked geometry lit, before any translucency is composited. */
	SceneColorPreTranslucency UMETA(DisplayName = "Scene Color (Pre-Translucency, HDR)"),

	/** Post-process chain, at the BL_SceneColorBeforeBloom point. HDR, post translucency, pre bloom and pre tonemap. */
	SceneColorBeforeBloom UMETA(DisplayName = "Scene Color (Before Bloom, HDR)"),

	/** Post-process chain, at the BL_SceneColorAfterTonemapping point. This is the final displayed image. */
	SceneColorAfterTonemap UMETA(DisplayName = "Scene Color (After Tonemap)"),

	/** Post-opaque. Linear depth in world units (centimetres). Needs a float target, R32f is the natural choice. */
	SceneDepthWorldUnits UMETA(DisplayName = "Scene Depth (World Units)"),

	/** Post-opaque. Raw reversed-Z device depth, exactly as the depth buffer stores it. Near plane is 1, far is 0. */
	SceneDepthDeviceZ UMETA(DisplayName = "Scene Depth (Raw Device Z)"),

	/** Post-opaque. GBufferA. Encoded in [0,1] unless you enable Decode Normals on the entry. */
	WorldNormal UMETA(DisplayName = "World Normal"),

	/** Post-opaque. GBufferC RGB. */
	BaseColor UMETA(DisplayName = "Base Color"),

	/** Post-opaque. GBufferB R, broadcast to RGB. */
	Metallic UMETA(DisplayName = "Metallic"),

	/** Post-opaque. GBufferB G, broadcast to RGB. */
	Specular UMETA(DisplayName = "Specular"),

	/** Post-opaque. GBufferB B, broadcast to RGB. */
	Roughness UMETA(DisplayName = "Roughness"),

	/** Post-opaque. Screen space velocity. Empty unless a velocity pass is actually running this frame. */
	Velocity UMETA(DisplayName = "Screen Velocity"),

	/** Post-opaque. Screen space ambient occlusion buffer. */
	AmbientOcclusion UMETA(DisplayName = "Ambient Occlusion"),

	/** Post-opaque. Custom depth in world units. Requires Custom Depth to be enabled in project settings. */
	CustomDepthWorldUnits UMETA(DisplayName = "Custom Depth (World Units)"),

	/** Post-opaque. Custom stencil value. Scaled by Stencil Normalize Range on the entry. */
	CustomStencil UMETA(DisplayName = "Custom Stencil"),

	// New sources are appended rather than inserted so existing profiles keep their assignments.

	/** Post-process chain. The separate translucency layer on its own, before it is composited into scene color. */
	SeparateTranslucency UMETA(DisplayName = "Separate Translucency (HDR)"),

	/** Post-opaque. GBufferD, whose meaning depends on the shading model: subsurface color, clear coat, hair, and so on. */
	GBufferCustomData UMETA(DisplayName = "GBuffer D (Custom Data)"),

	/** Post-opaque. GBufferE, the precomputed shadow factors. */
	PrecomputedShadowFactors UMETA(DisplayName = "GBuffer E (Precomputed Shadows)"),

	/** Post-opaque. GBufferF, tangent and anisotropy. Only allocated when anisotropic materials are enabled. */
	Anisotropy UMETA(DisplayName = "GBuffer F (Anisotropy / Tangent)"),

	/** Post-opaque. The partial depth buffer, raw device Z. Empty unless a pass that needs it ran this frame. */
	ScenePartialDepth UMETA(DisplayName = "Scene Partial Depth (Raw Device Z)"),

	// Screen-space Lumen. These are the denoised temporal history buffers, so they are the previous
	// frame's result reprojected, and they can be at a downsampled resolution. Unlike the atlases
	// they are ordinary screen-shaped images you can actually composite with.

	/** Lumen denoised diffuse indirect lighting, screen space. The GI contribution on its own. */
	LumenDiffuseIndirect UMETA(DisplayName = "Lumen: Diffuse Indirect (Screen)"),

	/** Lumen denoised rough specular indirect, screen space. */
	LumenRoughSpecularIndirect UMETA(DisplayName = "Lumen: Rough Specular Indirect (Screen)"),

	/** Lumen short range ambient occlusion, screen space. This is Lumen's own AO, and the thing to use instead of the SSAO source when Lumen is on. */
	LumenShortRangeAO UMETA(DisplayName = "Lumen: Short Range AO (Screen)"),

	/** Lumen backface diffuse indirect, screen space. Used for two sided foliage. */
	LumenBackfaceDiffuseIndirect UMETA(DisplayName = "Lumen: Backface Diffuse Indirect (Screen)"),

	/** Lumen reflections, screen space. RGB is specular, alpha carries the denoiser's second moment. */
	LumenReflections UMETA(DisplayName = "Lumen: Reflections (Screen)"),
};

/** True for anything sourced from Lumen. All of it requires SCENEPASSCAPTURE_LUMEN. */
inline bool ScenePassCapture_IsLumenSource(EScenePassCaptureSource Source)
{
	switch (Source)
	{
	case EScenePassCaptureSource::LumenDiffuseIndirect:
	case EScenePassCaptureSource::LumenRoughSpecularIndirect:
	case EScenePassCaptureSource::LumenShortRangeAO:
	case EScenePassCaptureSource::LumenBackfaceDiffuseIndirect:
	case EScenePassCaptureSource::LumenReflections:
		return true;
	default:
		return false;
	}
}

/** Fraction of the rendered view size to capture at. Smaller costs proportionally less bandwidth and memory. */
UENUM(BlueprintType)
enum class EScenePassCaptureResolutionScale : uint8
{
	Full    UMETA(DisplayName = "1/1 (Full)"),
	Half    UMETA(DisplayName = "1/2"),
	Quarter UMETA(DisplayName = "1/4"),
	Sixth   UMETA(DisplayName = "1/6"),
	Eighth  UMETA(DisplayName = "1/8"),
};

/** The integer divisor a scale corresponds to. */
inline int32 ScenePassCapture_GetResolutionDivisor(EScenePassCaptureResolutionScale Scale)
{
	switch (Scale)
	{
	case EScenePassCaptureResolutionScale::Half:    return 2;
	case EScenePassCaptureResolutionScale::Quarter: return 4;
	case EScenePassCaptureResolutionScale::Sixth:   return 6;
	case EScenePassCaptureResolutionScale::Eighth:  return 8;
	default:                                        return 1;
	}
}

/**
 * Applies a scale to a size, never collapsing to zero.
 *
 * bAlignToMultipleOfTwo rounds both dimensions down to an even number. Worth having because a
 * viewport like 1531x862 divided by 6 lands on 255x143, and odd dimensions make the downsample
 * sample off-centre and upset anything that later halves the size again.
 */
inline FIntPoint ScenePassCapture_ApplyResolutionScale(FIntPoint Size, EScenePassCaptureResolutionScale Scale, bool bAlignToMultipleOfTwo)
{
	const int32 Divisor = ScenePassCapture_GetResolutionDivisor(Scale);
	FIntPoint Result(FMath::Max(1, Size.X / Divisor), FMath::Max(1, Size.Y / Divisor));

	if (bAlignToMultipleOfTwo)
	{
		Result.X = FMath::Max(2, Result.X & ~1);
		Result.Y = FMath::Max(2, Result.Y & ~1);
	}

	return Result;
}

/** One "grab this pass into this render target" instruction. */
USTRUCT(BlueprintType)
struct SCENEPASSCAPTURE_API FScenePassCaptureEntry
{
	GENERATED_BODY()

	/** Uncheck to skip this entry without removing it from the profile. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene Pass Capture")
	bool bEnabled = true;

	/** Which renderer buffer to read. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene Pass Capture")
	EScenePassCaptureSource Source = EScenePassCaptureSource::SceneDepthWorldUnits;

	/** Where to write it. Sized independently of the viewport, the copy rescales with a bilinear (or point, for depth) blit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene Pass Capture")
	TObjectPtr<UTextureRenderTarget2D> Target = nullptr;

	/** Capture at a fraction of the rendered view size. Drives Resize Targets To Viewport and the Create Target button. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene Pass Capture")
	EScenePassCaptureResolutionScale ResolutionScale = EScenePassCaptureResolutionScale::Full;

	/** Round the resulting width and height down to an even number. Applies on top of Resolution Scale. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene Pass Capture", meta = (DisplayName = "Align Size To Multiple Of 2"))
	bool bAlignSizeToMultipleOfTwo = true;

	/** Depth sources only. World distance in centimetres that maps to 1.0. Leave at 0 to write raw world units, which needs a float target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene Pass Capture", meta = (ClampMin = "0.0", UIMax = "100000.0"))
	float DepthNormalizeRange = 0.0f;

	/** Custom Stencil only. Stencil value that maps to 1.0. Stencil is 0-255, so 255 gives you a normalised result. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene Pass Capture", meta = (ClampMin = "1.0", UIMax = "255.0"))
	float StencilNormalizeRange = 255.0f;

	/** World Normal only. Expands the stored [0,1] value back to [-1,1]. Needs a signed target format such as RGBA16f. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene Pass Capture")
	bool bDecodeNormals = false;
};
