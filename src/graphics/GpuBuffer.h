#pragma once

#include "GpuResource.h"
#include <d3d12.h>

/// A Gpu side wrapper around GpuResource for geometry
/// view/descriptors. Tells the GPU how to interpret
/// the data.
class GpuBuffer : public GpuResource
{
public:
	GpuBuffer();

	void Initialize(UINT sizeInBytes, D3D12_RESOURCE_STATES initialState);

	D3D12_VERTEX_BUFFER_VIEW VertexBufferView(UINT stride) const;
	D3D12_INDEX_BUFFER_VIEW IndexBufferView(DXGI_FORMAT format = DXGI_FORMAT_R32_UINT) const;

private:
	/// Lets Direct3D know how much of the data to read
	/// from the buffers. It is loaded in from Initialize,
	/// and we just store the value for later use.
	UINT mBufferSize = 0;
};
