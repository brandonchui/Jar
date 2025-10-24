#pragma once

#include "graphics/Texture.h"
#include "graphics/Constants.h"
#include "vectormath.hpp"
#include "Lighting.h"
#include <cstdint>

/// Materials define the properties of some surface given
/// several common textures in PBR workflows, mainly Normal,
/// Metallic, Roughness, AO, and Albedo.
struct Material
{
	std::shared_ptr<Texture> mAlbedoTexture;
	Vector4 mAlbedoColor = Vector4(1.0F, 1.0F, 1.0F, 1.0F);

	std::shared_ptr<Texture> mNormalTexture;
	float mNormalStrength = 1.0F;

	std::shared_ptr<Texture> mMetallicTexture;
	float mMetallicFactor = 0.0F;

	std::shared_ptr<Texture> mRoughnessTexture;
	float mRoughnessFactor = 0.5F;

	std::shared_ptr<Texture> mAmbientOcclusionTexture;
	float mAmbientOcclusionFactor = 1.0F;

	std::shared_ptr<Texture> mEmissiveTexture;
	//NOTE Does this break with Vector3 ?
	Float3 mEmissiveFactor = Float3(0.0F, 0.0F, 0.0F);

	enum Flags
	{
		ALPHA_BLEND = 1 << 0,
		DOUBLE_SIDED = 1 << 1,
		CAST_SHADOWS = 1 << 2,
		RECEIVE_SHADOWS = 1 << 3,
		USE_VERTEX_COLORS = 1 << 4
	};
	uint32_t mFlags = CAST_SHADOWS | RECEIVE_SHADOWS;

	enum ShaderType
	{
		BLINN_PHONG, // Will remove soon.
		PBR
	};

	ShaderType mShaderType = PBR;

	/// For use in the constant buffer uploads since the
	/// PBR shader needs it.
	MaterialConstants ToGPUConstants() const
	{
		MaterialConstants gpu = {};

		gpu.mAlbedoColor = mAlbedoColor;
		gpu.mEmissiveFactor = mEmissiveFactor;
		gpu.mMetallicFactor = mMetallicFactor;
		gpu.mRoughnessFactor = mRoughnessFactor;
		gpu.mNormalStrength = mNormalStrength;
		gpu.mAmbientOcclusionStrength = mAmbientOcclusionFactor;
		gpu.mFlags = mFlags;

		gpu.mHasAlbedoTexture = mAlbedoTexture ? 1 : 0;
		gpu.mHasNormalTexture = mNormalTexture ? 1 : 0;
		gpu.mHasMetallicTexture = mMetallicTexture ? 1 : 0;
		gpu.mHasRoughnessTexture = mRoughnessTexture ? 1 : 0;
		gpu.mHasAmbientOcclusionTexture = mAmbientOcclusionTexture ? 1 : 0;
		gpu.mHasEmissiveTexture = mEmissiveTexture ? 1 : 0;

		return gpu;
	}
};
