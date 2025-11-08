#include "GpuBuffer.h"
#include "Core.h"
#include "DescriptorHeap.h"
#include "CommandListManager.h"
#include <directx/d3dx12_core.h>
#include <cassert>

GpuBuffer::GpuBuffer()
{
	mSrvAllocation.Reset();
	mUavAllocation.Reset();
}

GpuBuffer::~GpuBuffer()
{
	uint64_t lastSignaledFence =
		Graphics::gCommandListManager->GetGraphicsQueue().GetLastSignaledFenceValue();

	if (mSrvAllocation.IsValid())
	{
		Graphics::gBindlessAllocator->FreeDeferred(mSrvAllocation, lastSignaledFence);
		mSrvAllocation.Reset();
	}

	if (mUavAllocation.IsValid())
	{
		Graphics::gBindlessAllocator->FreeDeferred(mUavAllocation, lastSignaledFence);
		mUavAllocation.Reset();
	}
}

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

void GpuBuffer::CreateSRV(UINT structureByteStride)
{
	mSrvAllocation = Graphics::gBindlessAllocator->Allocate(1);
	if (!mSrvAllocation.IsValid())
	{
		return;
	}

	DescriptorHandle handle = Graphics::gBindlessAllocator->GetHandle(mSrvAllocation.mStartIndex);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Buffer.FirstElement = 0;

	if (structureByteStride > 0)
	{
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.Buffer.NumElements = mBufferSize / structureByteStride;
		srvDesc.Buffer.StructureByteStride = structureByteStride;
		srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	}
	else
	{
		srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
		srvDesc.Buffer.NumElements = mBufferSize / 4;
		srvDesc.Buffer.StructureByteStride = 0;
		srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
	}

	Graphics::gDevice->CreateShaderResourceView(mResource.Get(), &srvDesc, handle.GetCpuHandle());
}

void GpuBuffer::CreateUAV(UINT structureByteStride)
{
	mUavAllocation = Graphics::gBindlessAllocator->Allocate(1);
	if (!mUavAllocation.IsValid())
	{
		return;
	}

	DescriptorHandle handle = Graphics::gBindlessAllocator->GetHandle(mUavAllocation.mStartIndex);

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement = 0;

	if (structureByteStride > 0)
	{
		uavDesc.Format = DXGI_FORMAT_UNKNOWN;
		uavDesc.Buffer.NumElements = mBufferSize / structureByteStride;
		uavDesc.Buffer.StructureByteStride = structureByteStride;
		uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
	}
	else
	{
		uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
		uavDesc.Buffer.NumElements = mBufferSize / 4;
		uavDesc.Buffer.StructureByteStride = 0;
		uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
	}

	Graphics::gDevice->CreateUnorderedAccessView(mResource.Get(), nullptr, &uavDesc, handle.GetCpuHandle());
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
