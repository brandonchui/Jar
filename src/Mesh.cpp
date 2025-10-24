#include "Mesh.h"
#include "graphics/UploadBuffer.h"
#include "graphics/Core.h"
#include "graphics/CommandContext.h"
#include "graphics/CommandListManager.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include <cstdio>
#include <DirectXMesh.h>
#include <utils/WaveFrontReader.h>
#include <filesystem>
#include <algorithm>

using namespace DirectX;

/// This is a bit clunky way of implementing the logger since
/// you don't really want to recreate more then one ideally if
/// alot of meshes are loaded. NOTE Maybe a global?
void Mesh::InitLogger()
{
	if (!mLogger)
	{
		mLogger = spdlog::get("Mesh");
		if (!mLogger)
		{
			mLogger = spdlog::stdout_color_mt("Mesh");
			mLogger->set_pattern("[%H:%M:%S] [%^%l%$] [%n] %v");
			mLogger->set_level(spdlog::level::debug);
		}
	}
}

Mesh::Mesh(const std::string& filepath)
{
	InitLogger();
	LoadFromOBJ(filepath);
}

bool Mesh::LoadFromOBJ(const std::string& filepath)
{
	InitLogger();
	mLogger->info("Loading OBJ file: {}", filepath);

	if (!std::filesystem::exists(filepath))
	{
		mLogger->error("File not found: {}", filepath);
		return false;
	}

	// This is super slow TODO change the obj loader.
	DX::WaveFrontReader<uint32_t> reader;

	std::wstring wFilepath(filepath.begin(), filepath.end());
	HRESULT hr = reader.Load(wFilepath.c_str());

	if (FAILED(hr))
	{
		mLogger->error("Failed to load OBJ file. HRESULT: 0x{:08X}", hr);
		return false;
	}
	mLogger->info("OBJ file loaded successfully");

	mVertices.clear();
	mIndices.clear();

	size_t totalVertices = reader.vertices.size();
	mVertices.reserve(totalVertices);

	// Convert WaveFrontReader data to the Vertex format.
	for (auto& vertice : reader.vertices)
	{
		// Interleaved position/normal/texcoord.
		Vertex vertex;

		vertex.position = Vector3(vertice.position.x, vertice.position.y, vertice.position.z);
		vertex.normal = Vector3(vertice.normal.x, vertice.normal.y, vertice.normal.z);
		vertex.texCoord = Vector2(vertice.textureCoordinate.x, vertice.textureCoordinate.y);

		// TODO Get real tangents
		vertex.tangent = Vector4(1.0F, 0.0F, 0.0F, 1.0F);

		// Vertex is not really used but might be good for debugging purposes?
		vertex.color = Vector4(1.0F, 1.0F, 1.0F, 1.0F);

		mVertices.push_back(vertex);
	}

	if (!reader.indices.empty())
	{
		mIndices.reserve(reader.indices.size());

		for (unsigned int indice : reader.indices)
		{
			mIndices.push_back(indice);
		}
	}
	else
	{
		// NOTE Should probably error out if no indices are found from the loader.
		mLogger->error("No indices found");
	}

	mVertexCount = static_cast<uint32_t>(mVertices.size());
	mIndexCount = static_cast<uint32_t>(mIndices.size());

	if (!mVertices.empty() && !mIndices.empty() && (mIndexCount % 3 == 0))
	{
		mLogger->info("Computing tangent vectors...");

		// Prepare buffers for the ComputeTangentFrame call.
		std::vector<XMFLOAT3> positions(mVertexCount);
		std::vector<XMFLOAT3> normals(mVertexCount);
		std::vector<XMFLOAT2> texCoords(mVertexCount);
		std::vector<XMFLOAT4> tangents(mVertexCount);

		for (size_t i = 0; i < mVertexCount; ++i)
		{
			positions[i] = XMFLOAT3(mVertices[i].position.getX(), mVertices[i].position.getY(),
									mVertices[i].position.getZ());
			normals[i] = XMFLOAT3(mVertices[i].normal.getX(), mVertices[i].normal.getY(),
								  mVertices[i].normal.getZ());
			texCoords[i] = XMFLOAT2(mVertices[i].texCoord.getX(), mVertices[i].texCoord.getY());
		}

		HRESULT tangentResult = DirectX::ComputeTangentFrame(
			mIndices.data(), mIndexCount / 3, positions.data(), normals.data(), texCoords.data(),
			mVertexCount, tangents.data());

		if (SUCCEEDED(tangentResult))
		{
			for (size_t i = 0; i < mVertexCount; ++i)
			{
				mVertices[i].tangent = Vector4(tangents[i].x, tangents[i].y, tangents[i].z,
											   tangents[i].w);
			}
			mLogger->info("\tTangents computed successfully");
		}
		else
		{
			mLogger->warn(
				"\tFailed to compute tangents (HRESULT: 0x{:08X}), using default tangents",
				tangentResult);
		}
	}
	else
	{
		if (mVertices.empty() || mIndices.empty())
		{
			mLogger->warn("\tSkipping tangent computation: no vertex or index data");
		}
		else if (mIndexCount % 3 != 0)
		{
			mLogger->warn("\tSkipping tangent computation: index count ({}) is not a multiple of 3",
						  mIndexCount);
		}
	}

	mLogger->info("Mesh data prepared:");
	mLogger->info("\tFinal vertex count: {}", mVertexCount);
	mLogger->info("\tFinal index count: {}", mIndexCount);
	mLogger->info("\tVertex size: {} bytes", sizeof(Vertex));
	mLogger->info("\tTotal vertex data: {} bytes", mVertexCount * sizeof(Vertex));
	mLogger->info("\tTotal index data: {} bytes", mIndexCount * sizeof(uint32_t));

	ComputeBoundingBox();

	return true;
}

void Mesh::UploadToGPU()
{
	if (mIsUploaded)
	{
		mLogger->info("Already uploaded to GPU");
		return;
	}

	if (mVertices.empty() || mIndices.empty())
	{
		mLogger->error("No mesh data to upload");
		return;
	}

	mLogger->info("Uploading mesh to GPU...");

	UINT vertexBufferSize = static_cast<UINT>(mVertices.size() * sizeof(Vertex));
	UINT indexBufferSize = static_cast<UINT>(mIndices.size() * sizeof(uint32_t));

	// Fast empty heaps.
	mVertexBuffer.Initialize(vertexBufferSize, D3D12_RESOURCE_STATE_COPY_DEST);
	mIndexBuffer.Initialize(indexBufferSize, D3D12_RESOURCE_STATE_COPY_DEST);

	// The basic steps for uploading:
	// - Create UploadBuffer and Initialize it
	// - Use a GraphicsContext temp variable
	// - CopyResource the data buffer to upload buffer
	// - Transition
	// - Execute
	UploadBuffer vertexUpload;
	vertexUpload.Initialize(mVertices.data(), vertexBufferSize);

	UploadBuffer indexUpload;
	indexUpload.Initialize(mIndices.data(), indexBufferSize);

	// Create temporary context for copy.
	Graphics::GraphicsContext copyContext;
	copyContext.Create(Graphics::gDevice);
	copyContext.Begin();

	// Since UploadBuffer the CPU can write, we copy over the Upload to the
	// faster GPU only.
	copyContext.GetCommandList()->CopyResource(mVertexBuffer.GetResource(),
											   vertexUpload.GetResource());

	copyContext.GetCommandList()->CopyResource(mIndexBuffer.GetResource(),
											   indexUpload.GetResource());

	copyContext.TransitionResource(mVertexBuffer, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	copyContext.TransitionResource(mIndexBuffer, D3D12_RESOURCE_STATE_INDEX_BUFFER);

	copyContext.Flush();
	CommandQueue& queue = Graphics::gCommandListManager->GetGraphicsQueue();
	uint64_t fenceValue = queue.ExecuteCommandList(copyContext.GetCommandList());
	queue.WaitForFence(fenceValue);

	mIsUploaded = true;
	mLogger->info("Upload complete");
}

void Mesh::ComputeBoundingBox()
{
	if (mVertices.empty())
	{
		mBoundingBox.mMin = Vector3(0, 0, 0);
		mBoundingBox.mMax = Vector3(0, 0, 0);
		return;
	}

	float minX = mVertices[0].position.getX();
	float minY = mVertices[0].position.getY();
	float minZ = mVertices[0].position.getZ();
	float maxX = minX;
	float maxY = minY;
	float maxZ = minZ;

	for (const auto& vertex : mVertices)
	{
		float x = vertex.position.getX();
		float y = vertex.position.getY();
		float z = vertex.position.getZ();

		minX = std::min(minX, x);
		minY = std::min(minY, y);
		minZ = std::min(minZ, z);
		maxX = std::max(maxX, x);
		maxY = std::max(maxY, y);
		maxZ = std::max(maxZ, z);
	}

	mBoundingBox.mMin = Vector3(minX, minY, minZ);
	mBoundingBox.mMax = Vector3(maxX, maxY, maxZ);

	mLogger->info("\tBounding box: min({}, {}, {}) max({}, {}, {})", minX, minY, minZ, maxX, maxY,
				  maxZ);
}
