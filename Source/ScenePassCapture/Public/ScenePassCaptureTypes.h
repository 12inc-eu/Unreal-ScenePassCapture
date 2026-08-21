// Copyright Exiin Game Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
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

/** When a custom pass re-renders the scene. */
UENUM(BlueprintType)
enum class EScenePassCaptureTiming : uint8
{
	/** Re-render every frame. The most expensive option by far. */
	EveryFrame UMETA(DisplayName = "Every Frame"),

	/** Re-render every N frames, round-robin friendly. */
	EveryNFrames UMETA(DisplayName = "Every N Frames"),

	/** Only when Capture Custom Pass Now is called from Blueprint or C++. */
	OnDemand UMETA(DisplayName = "On Demand"),
};

/**
 * What a custom pass renders. These map onto engine show flags, so a custom pass is the same scene
 * viewed with different settings, not a different buffer.
 */
USTRUCT(BlueprintType)
struct SCENEPASSCAPTURE_API FScenePassCaptureShowFlags
{
	GENERATED_BODY()

	// --- Lighting ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lighting")
	bool bLighting = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lighting")
	bool bDynamicShadows = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lighting")
	bool bGlobalIllumination = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lighting")
	bool bAmbientOcclusion = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lighting")
	bool bReflectionEnvironment = true;

	/** Replaces every material's base color with neutral grey, the engine's Lighting Only view mode. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lighting")
	bool bLightingOnlyOverride = false;

	// --- Geometry ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry")
	bool bStaticMeshes = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry")
	bool bSkeletalMeshes = true;

	/** Groom and hair strands. This is the Hair flag, not VisualizeGroom, which is only a debug view. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry")
	bool bHair = true;

	/**
	 * Nanite renders through its own path, so a Nanite static mesh keeps drawing with Static Meshes
	 * off. Both have to be unchecked to remove it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry")
	bool bNaniteMeshes = true;

	/** Instanced and hierarchical static meshes. These are NOT covered by Static Meshes either. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry")
	bool bInstancedStaticMeshes = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry")
	bool bInstancedGrass = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry")
	bool bBSP = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry")
	bool bLandscape = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry")
	bool bInstancedFoliage = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry")
	bool bParticles = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry")
	bool bTranslucency = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry")
	bool bDecals = true;

	// --- Atmosphere ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atmosphere")
	bool bFog = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atmosphere")
	bool bVolumetricFog = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atmosphere")
	bool bAtmosphere = true;

	// --- Post processing ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Post Processing")
	bool bPostProcessing = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Post Processing")
	bool bBloom = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Post Processing")
	bool bAntiAliasing = true;

	/** Off by default: motion blur in a capture needs persistent temporal state to look right. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Post Processing")
	bool bMotionBlur = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Post Processing")
	bool bDepthOfField = false;
};

/**
 * One extra render of the scene with its own settings, mirrored to the active camera.
 *
 * This is a fundamentally different cost tier to the pass sources above. Those copy a buffer the
 * renderer already produced. This one renders the scene again: a few ms of GPU plus a serialized
 * chunk of render thread, and the render thread part barely shrinks with Resolution Scale because
 * visibility is per-primitive rather than per-pixel.
 */
USTRUCT(BlueprintType)
struct SCENEPASSCAPTURE_API FScenePassCaptureCustomPass
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Custom Pass")
	bool bEnabled = true;

	/** Label for the preview tile and the cost readout. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Custom Pass")
	FName PassName = TEXT("Custom Pass");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Custom Pass")
	TObjectPtr<UTextureRenderTarget2D> Target = nullptr;

	/**
	 * What the capture writes out. This is what decides whether you get transparency.
	 *
	 * SceneColor (HDR) is the one to use when isolating meshes: it puts INVERSE opacity in alpha, so
	 * empty background is 1 and solid geometry is 0. Compositing wants coverage, so use 1 - Alpha in
	 * the material. Needs a target with an alpha channel, RGBA16f being the natural choice.
	 *
	 * Final Color (LDR) is the tonemapped image but carries no useful alpha unless the project has
	 * alpha propagation through post processing enabled, so an isolated mesh comes out on solid black.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Custom Pass")
	TEnumAsByte<ESceneCaptureSource> CaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Custom Pass")
	EScenePassCaptureTiming Timing = EScenePassCaptureTiming::EveryNFrames;

	/** Used when Timing is Every N Frames. 4 means one re-render every four frames. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Custom Pass", meta = (ClampMin = "1", UIMax = "30", EditCondition = "Timing == EScenePassCaptureTiming::EveryNFrames"))
	int32 FrameInterval = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Custom Pass")
	EScenePassCaptureResolutionScale ResolutionScale = EScenePassCaptureResolutionScale::Half;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Custom Pass", meta = (DisplayName = "Align Size To Multiple Of 2"))
	bool bAlignSizeToMultipleOfTwo = true;

	/** What this pass renders. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Custom Pass")
	FScenePassCaptureShowFlags ShowFlags;

	/**
	 * Render only actors carrying any of these tags. This is the selection method that works in game:
	 * tags resolve against whatever is in the level at runtime, whereas the actor lists below are
	 * references to specific level actors and are only usable on a level you edited by hand.
	 *
	 * Restricting the visible set is also the single most effective way to cut the render thread cost,
	 * since visibility is per-primitive.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Custom Pass|Selection")
	TArray<FName> ShowOnlyActorTags;

	/** Hide actors carrying any of these tags. Ignored while Show Only is in effect. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Custom Pass|Selection")
	TArray<FName> HiddenActorTags;

	/**
	 * Render only components carrying any of these Component Tags. Finer than the actor lists: it can
	 * pick one mesh out of an actor and leave the rest, which is how you isolate a character's body
	 * without its weapons.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Custom Pass|Selection")
	TArray<FName> ShowOnlyComponentTags;

	/**
	 * Hide components carrying any of these Component Tags. This is also the only way to exclude
	 * component types that have no show flag at all, dynamic meshes among them.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Custom Pass|Selection")
	TArray<FName> HiddenComponentTags;

	/** Specific level actors. Editor and hand-authored levels only: these cannot resolve in a packaged game. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Custom Pass|Selection", meta = (DisplayName = "Show Only Actors (Editor Only)"))
	TArray<TSoftObjectPtr<AActor>> ShowOnlyActors;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Custom Pass|Selection", meta = (DisplayName = "Hidden Actors (Editor Only)"))
	TArray<TSoftObjectPtr<AActor>> HiddenActors;

	/**
	 * Restricts the pass to a depth slab, which is the cheapest way to render "just this character and
	 * nothing else around it". With a Focus Actor set the slab follows that actor's distance from the
	 * camera, so it stays centred on them as they move.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Custom Pass|Depth Slab")
	bool bUseDepthSlab = false;

	/** Slab centre. Leave empty to use Near and Far as plain distances from the camera. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Custom Pass|Depth Slab", meta = (EditCondition = "bUseDepthSlab"))
	TSoftObjectPtr<AActor> FocusActor;

	/** Centimetres in front of the focus actor, or the near clip distance when there is no focus actor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Custom Pass|Depth Slab", meta = (EditCondition = "bUseDepthSlab", ClampMin = "1.0"))
	float SlabNear = 100.0f;

	/** Centimetres behind the focus actor, or the far clip distance when there is no focus actor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Custom Pass|Depth Slab", meta = (EditCondition = "bUseDepthSlab", ClampMin = "1.0"))
	float SlabFar = 100.0f;

	/** Keeps temporal history between captures. Needed for TSR, motion blur and velocity, costs memory. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Custom Pass", meta = (AdvancedDisplay))
	bool bPersistRenderingState = false;
};

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
