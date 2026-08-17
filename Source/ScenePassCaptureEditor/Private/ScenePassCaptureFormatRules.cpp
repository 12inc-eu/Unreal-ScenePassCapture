// Copyright Exiin Game Studio. All Rights Reserved.

#include "ScenePassCaptureFormatRules.h"

#include "ScenePassCaptureTypes.h"

#define LOCTEXT_NAMESPACE "ScenePassCaptureFormatRules"

namespace
{
	int32 GetChannelCount(ETextureRenderTargetFormat Format)
	{
		switch (Format)
		{
		case RTF_R8:
		case RTF_R16f:
		case RTF_R32f:
			return 1;
		case RTF_RG8:
		case RTF_RG16f:
		case RTF_RG32f:
			return 2;
		default:
			return 4;
		}
	}

	/** Float formats can hold values outside [0,1]. Fixed point formats clamp. */
	bool IsFloatRTF(ETextureRenderTargetFormat Format)
	{
		switch (Format)
		{
		case RTF_R16f:
		case RTF_RG16f:
		case RTF_RGBA16f:
		case RTF_R32f:
		case RTF_RG32f:
		case RTF_RGBA32f:
			return true;
		default:
			return false;
		}
	}

	/** Every float render target format is signed. The fixed point and RGB10A2 ones are not. */
	bool IsSignedRTF(ETextureRenderTargetFormat Format)
	{
		return IsFloatRTF(Format);
	}

	bool IsDepthSource(EScenePassCaptureSource Source)
	{
		return Source == EScenePassCaptureSource::SceneDepthWorldUnits || Source == EScenePassCaptureSource::CustomDepthWorldUnits;
	}

	bool IsHDRSource(EScenePassCaptureSource Source)
	{
		switch (Source)
		{
		case EScenePassCaptureSource::SceneColorPreTranslucency:
		case EScenePassCaptureSource::SceneColorBeforeBloom:
		case EScenePassCaptureSource::SeparateTranslucency:
		case EScenePassCaptureSource::LumenDiffuseIndirect:
		case EScenePassCaptureSource::LumenRoughSpecularIndirect:
		case EScenePassCaptureSource::LumenBackfaceDiffuseIndirect:
		case EScenePassCaptureSource::LumenReflections:
			return true;
		default:
			return false;
		}
	}

	/** How many channels carry real data for this source. */
	int32 GetRequiredChannels(EScenePassCaptureSource Source)
	{
		switch (Source)
		{
		case EScenePassCaptureSource::SceneColorPreTranslucency:
		case EScenePassCaptureSource::SceneColorBeforeBloom:
		case EScenePassCaptureSource::SceneColorAfterTonemap:
		case EScenePassCaptureSource::WorldNormal:
		case EScenePassCaptureSource::BaseColor:
		case EScenePassCaptureSource::SeparateTranslucency:
		case EScenePassCaptureSource::GBufferCustomData:
		case EScenePassCaptureSource::PrecomputedShadowFactors:
		case EScenePassCaptureSource::Anisotropy:
		case EScenePassCaptureSource::LumenDiffuseIndirect:
		case EScenePassCaptureSource::LumenRoughSpecularIndirect:
		case EScenePassCaptureSource::LumenBackfaceDiffuseIndirect:
		case EScenePassCaptureSource::LumenReflections:
			return 3;
		case EScenePassCaptureSource::Velocity:
			return 2;
		default:
			return 1;
		}
	}

	FText GetSourceLabel(EScenePassCaptureSource Source)
	{
		if (const UEnum* EnumPtr = StaticEnum<EScenePassCaptureSource>())
		{
			return EnumPtr->GetDisplayNameTextByValue(static_cast<int64>(Source));
		}
		return LOCTEXT("UnknownSource", "This pass");
	}
}

ETextureRenderTargetFormat ScenePassCapture_GetRecommendedFormat(const FScenePassCaptureEntry& Entry)
{
	switch (Entry.Source)
	{
	case EScenePassCaptureSource::SceneColorPreTranslucency:
	case EScenePassCaptureSource::SceneColorBeforeBloom:
	case EScenePassCaptureSource::SeparateTranslucency:
		return RTF_RGBA16f;

	case EScenePassCaptureSource::SceneColorAfterTonemap:
	case EScenePassCaptureSource::BaseColor:
	case EScenePassCaptureSource::GBufferCustomData:
	case EScenePassCaptureSource::PrecomputedShadowFactors:
	case EScenePassCaptureSource::Anisotropy:
		return RTF_RGBA8;

	case EScenePassCaptureSource::ScenePartialDepth:
		return RTF_R16f;

	// Lumen occlusion is a single scalar. The rest of screen-space Lumen is HDR radiance.
	case EScenePassCaptureSource::LumenShortRangeAO:
		return RTF_R8;

	case EScenePassCaptureSource::LumenDiffuseIndirect:
	case EScenePassCaptureSource::LumenRoughSpecularIndirect:
	case EScenePassCaptureSource::LumenBackfaceDiffuseIndirect:
	case EScenePassCaptureSource::LumenReflections:
		return RTF_RGBA16f;

	case EScenePassCaptureSource::SceneDepthWorldUnits:
	case EScenePassCaptureSource::CustomDepthWorldUnits:
		// Raw world centimetres need the range of a float. A normalised range fits in 8 bits.
		return Entry.DepthNormalizeRange > 0.0f ? RTF_R8 : RTF_R32f;

	case EScenePassCaptureSource::SceneDepthDeviceZ:
		// Device Z is 0-1 but banding is brutal at 8 bits, especially with reversed Z.
		return RTF_R16f;

	case EScenePassCaptureSource::WorldNormal:
		return Entry.bDecodeNormals ? RTF_RGBA16f : RTF_RGBA8;

	case EScenePassCaptureSource::Velocity:
		return RTF_RG16f;

	case EScenePassCaptureSource::Metallic:
	case EScenePassCaptureSource::Specular:
	case EScenePassCaptureSource::Roughness:
	case EScenePassCaptureSource::AmbientOcclusion:
	case EScenePassCaptureSource::CustomStencil:
		return RTF_R8;

	default:
		return RTF_RGBA16f;
	}
}

FText ScenePassCapture_GetFormatDisplayName(ETextureRenderTargetFormat Format)
{
	switch (Format)
	{
	case RTF_R8:         return LOCTEXT("R8", "R8");
	case RTF_RG8:        return LOCTEXT("RG8", "RG8");
	case RTF_RGBA8:      return LOCTEXT("RGBA8", "RGBA8");
	case RTF_RGBA8_SRGB: return LOCTEXT("RGBA8_SRGB", "RGBA8 sRGB");
	case RTF_R16f:       return LOCTEXT("R16f", "R16f");
	case RTF_RG16f:      return LOCTEXT("RG16f", "RG16f");
	case RTF_RGBA16f:    return LOCTEXT("RGBA16f", "RGBA16f");
	case RTF_R32f:       return LOCTEXT("R32f", "R32f");
	case RTF_RG32f:      return LOCTEXT("RG32f", "RG32f");
	case RTF_RGBA32f:    return LOCTEXT("RGBA32f", "RGBA32f");
	case RTF_RGB10A2:    return LOCTEXT("RGB10A2", "RGB10A2");
	default:             return LOCTEXT("UnknownFormat", "Unknown");
	}
}

FString ScenePassCapture_SuggestTargetName(const FScenePassCaptureEntry& Entry)
{
	switch (Entry.Source)
	{
	case EScenePassCaptureSource::SceneColorPreTranslucency: return TEXT("RT_SceneColorPreTranslucency");
	case EScenePassCaptureSource::SceneColorBeforeBloom:     return TEXT("RT_SceneColorBeforeBloom");
	case EScenePassCaptureSource::SceneColorAfterTonemap:    return TEXT("RT_SceneColorFinal");
	case EScenePassCaptureSource::SceneDepthWorldUnits:      return TEXT("RT_SceneDepth");
	case EScenePassCaptureSource::SceneDepthDeviceZ:         return TEXT("RT_SceneDepthDeviceZ");
	case EScenePassCaptureSource::WorldNormal:               return TEXT("RT_WorldNormal");
	case EScenePassCaptureSource::BaseColor:                 return TEXT("RT_BaseColor");
	case EScenePassCaptureSource::Metallic:                  return TEXT("RT_Metallic");
	case EScenePassCaptureSource::Specular:                  return TEXT("RT_Specular");
	case EScenePassCaptureSource::Roughness:                 return TEXT("RT_Roughness");
	case EScenePassCaptureSource::Velocity:                  return TEXT("RT_Velocity");
	case EScenePassCaptureSource::AmbientOcclusion:          return TEXT("RT_AmbientOcclusion");
	case EScenePassCaptureSource::CustomDepthWorldUnits:     return TEXT("RT_CustomDepth");
	case EScenePassCaptureSource::CustomStencil:             return TEXT("RT_CustomStencil");
	case EScenePassCaptureSource::SeparateTranslucency:      return TEXT("RT_SeparateTranslucency");
	case EScenePassCaptureSource::GBufferCustomData:         return TEXT("RT_GBufferCustomData");
	case EScenePassCaptureSource::PrecomputedShadowFactors:  return TEXT("RT_PrecomputedShadows");
	case EScenePassCaptureSource::Anisotropy:                return TEXT("RT_Anisotropy");
	case EScenePassCaptureSource::ScenePartialDepth:         return TEXT("RT_ScenePartialDepth");
	case EScenePassCaptureSource::LumenDiffuseIndirect:         return TEXT("RT_LumenDiffuseIndirect");
	case EScenePassCaptureSource::LumenRoughSpecularIndirect:   return TEXT("RT_LumenRoughSpecular");
	case EScenePassCaptureSource::LumenShortRangeAO:            return TEXT("RT_LumenShortRangeAO");
	case EScenePassCaptureSource::LumenBackfaceDiffuseIndirect: return TEXT("RT_LumenBackfaceDiffuse");
	case EScenePassCaptureSource::LumenReflections:             return TEXT("RT_LumenReflections");
	default:                                                 return TEXT("RT_ScenePass");
	}
}

bool ScenePassCapture_ValidateTargetFormat(const FScenePassCaptureEntry& Entry, FText& OutMessage, bool& bOutIsError)
{
	OutMessage = FText::GetEmpty();
	bOutIsError = false;

	if (!Entry.Target)
	{
		return true;
	}

	const ETextureRenderTargetFormat Format = Entry.Target->RenderTargetFormat;
	const ETextureRenderTargetFormat Recommended = ScenePassCapture_GetRecommendedFormat(Entry);
	const FText FormatName = ScenePassCapture_GetFormatDisplayName(Format);
	const FText RecommendedName = ScenePassCapture_GetFormatDisplayName(Recommended);
	const FText SourceName = GetSourceLabel(Entry.Source);

	// Errors first: these produce visibly wrong output, not just a quality loss.

	if (IsDepthSource(Entry.Source) && Entry.DepthNormalizeRange <= 0.0f && !IsFloatRTF(Format))
	{
		bOutIsError = true;
		OutMessage = FText::Format(LOCTEXT("RawDepthNeedsFloat", "{0} writes raw world centimetres, but {1} clamps to 1. Use {2}, or set Depth Normalize Range to squash the range into 0-1."), SourceName, FormatName, RecommendedName);
		return false;
	}

	if (Entry.Source == EScenePassCaptureSource::WorldNormal && Entry.bDecodeNormals && !IsSignedRTF(Format))
	{
		bOutIsError = true;
		OutMessage = FText::Format(LOCTEXT("DecodedNormalsNeedSigned", "Decoded normals span -1 to 1, but {0} cannot store negatives. Use {1}, or turn Decode Normals off."), FormatName, RecommendedName);
		return false;
	}

	const int32 RequiredChannels = GetRequiredChannels(Entry.Source);
	const int32 AvailableChannels = GetChannelCount(Format);
	if (AvailableChannels < RequiredChannels)
	{
		bOutIsError = true;
		OutMessage = FText::Format(LOCTEXT("NotEnoughChannels", "{0} needs {1} channels but {2} only has {3}. Use {4}."), SourceName, FText::AsNumber(RequiredChannels), FormatName, FText::AsNumber(AvailableChannels), RecommendedName);
		return false;
	}

	// Warnings: it will work, but you are throwing something away.

	if (IsHDRSource(Entry.Source) && !IsFloatRTF(Format))
	{
		OutMessage = FText::Format(LOCTEXT("HDRIntoLDR", "{0} is HDR, so anything above 1 clips in {1}. Use {2} to keep highlights."), SourceName, FormatName, RecommendedName);
		return false;
	}

	if (Entry.Source == EScenePassCaptureSource::SceneDepthDeviceZ && !IsFloatRTF(Format))
	{
		OutMessage = FText::Format(LOCTEXT("DeviceZPrecision", "Device Z in {0} bands badly because reversed Z packs all the near range into the top values. Use {1}."), FormatName, RecommendedName);
		return false;
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
