// Copyright Exiin Game Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "RenderGraphResources.h"
#include "ShaderParameterStruct.h"

/** Must stay in sync with the SPC_MODE_* defines in ScenePassCaptureCopy.usf. */
enum class EScenePassCaptureCopyMode : uint32
{
	Copy = 0,
	Channel = 1,
	DecodeNormal = 2,
	LinearDepth = 3,
	DeviceDepth = 4,
};

/** Fullscreen blit from a renderer buffer into a user render target. */
class FScenePassCaptureCopyPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FScenePassCaptureCopyPS);
	SHADER_USE_PARAMETER_STRUCT(FScenePassCaptureCopyPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FVector4f, InvDeviceZToWorldZ)
		SHADER_PARAMETER(FVector4f, ChannelMask)
		SHADER_PARAMETER(FVector2f, InputUVScale)
		SHADER_PARAMETER(FVector2f, InputUVBias)
		SHADER_PARAMETER(FVector2f, OutputInvExtent)
		SHADER_PARAMETER(float, DepthNormalizeScale)
		SHADER_PARAMETER(uint32, CopyMode)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

/** Texture2DArray variant, for the Lumen history buffers that are arrays. Samples slice 0. */
class FScenePassCaptureCopyArrayPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FScenePassCaptureCopyArrayPS);
	SHADER_USE_PARAMETER_STRUCT(FScenePassCaptureCopyArrayPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FVector4f, InvDeviceZToWorldZ)
		SHADER_PARAMETER(FVector4f, ChannelMask)
		SHADER_PARAMETER(FVector2f, InputUVScale)
		SHADER_PARAMETER(FVector2f, InputUVBias)
		SHADER_PARAMETER(FVector2f, OutputInvExtent)
		SHADER_PARAMETER(float, DepthNormalizeScale)
		SHADER_PARAMETER(uint32, CopyMode)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2DArray, InputTextureArray)
		SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

/** Same idea, but unpacks the custom stencil uint2 SRV. */
class FScenePassCaptureStencilPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FScenePassCaptureStencilPS);
	SHADER_USE_PARAMETER_STRUCT(FScenePassCaptureStencilPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FVector2f, StencilPixelScale)
		SHADER_PARAMETER(FIntPoint, StencilPixelOffset)
		SHADER_PARAMETER(float, StencilNormalizeScale)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<uint2>, StencilTexture)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};
