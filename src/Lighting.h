#pragma once

#include <vectormath.hpp>
#include <cstdint>

/// The following Float2,3 types are similar to the math library
/// version of the Vector3, but it is not aligned for SIMD. It
/// appears the math libray I picked out is aligning types to
/// 16 bytes for intrinsics, but it messes up the compiled Slang.
///
// TODO Put this in another probably constant header since I have
// to use these later somewhere too.
struct Float2
{

	Float2()
	: x(0)
	, y(0)
	{
	}

	Float2(float _x, float _y)
	: x(_x)
	, y(_y)
	{
	}

public:
	float x, y;
};

struct Float3
{
	Float3()
	: x(0)
	, y(0)
	, z(0)
	{
	}

	Float3(float _x, float _y, float _z)
	: x(_x)
	, y(_y)
	, z(_z)
	{
	}

	Float3(const Vector3& v)
	: x(static_cast<float>(v.getX()))
	, y(static_cast<float>(v.getY()))
	, z(static_cast<float>(v.getZ()))
	{
	}

	operator Vector3() const { return {x, y, z}; }

public:
	float x, y, z;
};

/// Look in _Lit.slang_ for the mirrored structure.
/// NOTE: look into sharing headers between Slang
/// and Constants

constexpr uint32_t MAX_SPOT_LIGHTS = 16;

// 64 byte struct Spotlight
struct SpotLight
{
public:
	Float3 position;
	float range{};

	Float3 direction;
	float innerConeAngle{};

	Float3 color;
	float outerConeAngle{};

	float intensity{};
	float falloff{};
	Float2 padding;
};

// 96 byte struct LightingConstants
struct LightingConstants
{
public:
	Matrix4 invViewProj; // 64 bytes

	Float3 eyePosition;
	uint32_t numActiveLights{};

	Float3 ambientLight;
	float padding{};
};
