#pragma once

#include <cstdio>
#include "sse/vectormath.hpp"
#include <vectormath.hpp>

/// 80 byte aligned Vertex supporting
/// - Position(Vector3)
/// - Normals (Vector3)
/// - Texture Coordinates (Vector2)
/// - Normal Tangents (Vector4)
/// - Vertex Color (Vector4)
struct Vertex
{
	Vector3 position;
	Vector3 normal;
	Vector2 texCoord;
	Vector4 tangent;
	Vector4 color;
};
