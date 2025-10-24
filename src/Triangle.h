#pragma once

#include "Vertex.h"

// TODO Remove this.

/// Basic Triangle for debugging purposes
namespace Triangle
{
	inline const Vertex VERTICES[] = {{// Top vertex, RED
									   .position = Vector3(0.0F, 0.5F, 0.0F),
									   .normal = Vector3(0.0F, 0.0F, 1.0F),
									   .texCoord = Vector2(0.5F, 0.0F),
									   .tangent = Vector4(1.0F, 0.0F, 0.0F, 1.0F),
									   .color = Vector4(1.0F, 0.0F, 0.0F, 1.0F)},
									  {// Bottom right vertex, GREEN
									   .position = Vector3(0.5F, -0.5F, 0.0F),
									   .normal = Vector3(0.0F, 0.0F, 1.0F),
									   .texCoord = Vector2(1.0F, 1.0F),
									   .tangent = Vector4(1.0F, 0.0F, 0.0F, 1.0F),
									   .color = Vector4(0.0F, 1.0F, 0.0F, 1.0F)},
									  {// Bottom left vertex, BLUE
									   .position = Vector3(-0.5F, -0.5F, 0.0F),
									   .normal = Vector3(0.0F, 0.0F, 1.0F),
									   .texCoord = Vector2(0.0F, 1.0F),
									   .tangent = Vector4(1.0F, 0.0F, 0.0F, 1.0F),
									   .color = Vector4(0.0F, 0.0F, 1.0F, 1.0F)}};

	inline constexpr size_t VERTEX_COUNT = 3;
	inline constexpr size_t VERTEX_BUFFER_SIZE = sizeof(Vertex) * VERTEX_COUNT;
} // namespace Triangle
