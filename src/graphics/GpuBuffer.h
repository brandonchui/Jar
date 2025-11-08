#pragma once

#include "GpuResource.h"
#include "BindlessAllocator.h"
#include <d3d12.h>

/// A Gpu side wrapper around GpuResource for geometry
/// view/descriptors. Tells the GPU how to interpret
/// the data.
class GpuBuffer : public GpuResource
{
public:
	GpuBuffer();
	~GpuBuffer();

	void Initialize(UINT sizeInBytes, D3D12_RESOURCE_STATES initialState);

	/// Creates a shader resource view for the buffer.
	/// structureByteStride: The size of each element in bytes, think
	/// like a StructuredBuffer
	void CreateSRV(UINT structureByteStride);

	/// Creates an unordered access view for the buffer.
	/// structureByteStride: The size of each element in bytes, think
	/// like a RWStructuredBuffer
	void CreateUAV(UINT structureByteStride);

	D3D12_VERTEX_BUFFER_VIEW VertexBufferView(UINT stride) const;
	D3D12_INDEX_BUFFER_VIEW IndexBufferView(DXGI_FORMAT format = DXGI_FORMAT_R32_UINT) const;

	/// Bindless Allocation
	uint32_t GetSRVIndex() const { return mSrvAllocation.mStartIndex; }
	uint32_t GetUAVIndex() const { return mUavAllocation.mStartIndex; }
	bool HasSRV() const { return mSrvAllocation.IsValid(); }
	bool HasUAV() const { return mUavAllocation.IsValid(); }

private:
	/// Lets Direct3D know how much of the data to read
	/// from the buffers. It is loaded in from Initialize,
	/// and we just store the value for later use.
	UINT mBufferSize = 0;

	/// Bindless Allocations
	Allocation mSrvAllocation;
	Allocation mUavAllocation;
};
