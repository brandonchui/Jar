#pragma once

#include "Vertex.h"
#include "graphics/GpuBuffer.h"
#include <memory>
#include <vector>
#include <string>
#include <spdlog/spdlog.h>

/// Holds all mesh geometry data including buffers and
/// the CPU/GPU side data for its Vertices.
class Mesh
{
public:
	Mesh() = default;
	Mesh(const std::string& filepath);
	~Mesh() = default;

	/// Load mesh from OBJ file, which fills the vertice/indices
	/// arrays
	bool LoadFromOBJ(const std::string& filepath);

	/// Creates the internal GpuBuffers, only after the vertices
	/// are loaded.
	void UploadToGPU(); // NOTE: should call from the LoadFromOBJ?

	const GpuBuffer& GetVertexBuffer() const { return mVertexBuffer; }
	const GpuBuffer& GetIndexBuffer() const { return mIndexBuffer; }
	uint32_t GetIndexCount() const { return mIndexCount; }
	uint32_t GetVertexCount() const { return mVertexCount; }

	const std::vector<Vertex>& GetVertices() const { return mVertices; }
	const std::vector<uint32_t>& GetIndices() const { return mIndices; }

	void ComputeBoundingBox();

private:
	void InitLogger();

	std::vector<Vertex> mVertices;
	std::vector<uint32_t> mIndices;
	uint32_t mVertexCount = 0;
	uint32_t mIndexCount = 0;

	GpuBuffer mVertexBuffer;
	GpuBuffer mIndexBuffer;

	/// Tracking the upload state since loading another mesh
	/// will cause issues.
	bool mIsUploaded = false;

	/// AABB for raytracing later.
	struct AABB
	{
		Vector3 mMin;
		Vector3 mMax;
	};
	AABB mBoundingBox;

	std::shared_ptr<spdlog::logger> mLogger;
};
