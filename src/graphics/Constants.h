#pragma once

#include <cstdint>
#include <cstdio> //BUG vectormath.hpp needs this
#include <vectormath.hpp>
#include "../Lighting.h"

/// For use as a constant buffer, though the name is kinda bad.
// TODO Change to a better name
struct Transform
{
	Matrix4 wvp;
	Matrix4 world;
	Matrix4 worldInvTrans;
};

/// 80 bytes - must match Slang shader layout exactly
/// The vectormath does some alignment for SIMD so the data
/// types are a bit misleading so just using a simple Float3
/// to get the correct alignment to match the slang struct
///
// NOTE Look into sharing a constant struct in both the
// cpp and slang shader side?
struct MaterialConstants
{
	Vector4 mAlbedoColor;
	Float3 mEmissiveFactor;

	float mMetallicFactor;
	float mRoughnessFactor;
	float mNormalStrength;
	float mAmbientOcclusionStrength;

	uint32_t mFlags;

	uint32_t mHasAlbedoTexture;
	uint32_t mHasNormalTexture;
	uint32_t mHasMetallicTexture;
	uint32_t mHasRoughnessTexture;
	uint32_t mHasAmbientOcclusionTexture;
	uint32_t mHasEmissiveTexture;

	Float2 pad;
};
