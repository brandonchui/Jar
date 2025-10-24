#include "GpuBuffer.h"
#include "Core.h"
#include <directx/d3dx12_core.h>
#include <cassert>

GpuBuffer::GpuBuffer() = default;

void GpuBuffer::Initialize(UINT sizeInBytes, D3D12_RESOURCE_STATES initialState)
{
	assert(Graphics::gDevice != nullptr);

	auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeInBytes);

	HRESULT hr = Graphics::gDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE,
															&bufferDesc, initialState, nullptr,
															IID_PPV_ARGS(&mResource));

	assert(SUCCEEDED(hr) && "Failed to create GPU buffer");
	mGpuVirtualAddress = mResource->GetGPUVirtualAddress();

	mUsageState = initialState;
	mBufferSize = sizeInBytes;
	//TODO mResource->SetName(L"");
}

D3D12_VERTEX_BUFFER_VIEW GpuBuffer::VertexBufferView(UINT stride) const
{
	D3D12_VERTEX_BUFFER_VIEW view;

	view.BufferLocation = mResource->GetGPUVirtualAddress();
	view.SizeInBytes = mBufferSize;
	view.StrideInBytes = stride;

	return view;
}

D3D12_INDEX_BUFFER_VIEW GpuBuffer::IndexBufferView(DXGI_FORMAT format) const
{
	D3D12_INDEX_BUFFER_VIEW view;

	view.BufferLocation = mResource->GetGPUVirtualAddress();
	view.SizeInBytes = mBufferSize;
	view.Format = format;

	return view;
}
