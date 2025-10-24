#include "UploadBuffer.h"
#include "Core.h"
#include <directx/d3dx12_core.h>
#include <cassert>
#include <cstring>

UploadBuffer::UploadBuffer() = default;

UploadBuffer::~UploadBuffer()
{
	if ((mResource != nullptr) && (mMappedData != nullptr))
	{
		mResource->Unmap(0, nullptr);
	}
	mMappedData = nullptr;

	if (mAllocation != nullptr)
	{
		mAllocation->Release();
		mAllocation = nullptr;
	}
}

void UploadBuffer::Initialize(const void* data, UINT sizeInBytes)
{
	assert(Graphics::gAllocator != nullptr && "D3D12MemoryAllocator not initialized");
	assert(data != nullptr);

	D3D12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeInBytes);

	// D3D12MemoryAllocator is a better way to create resources and makes things
	// more clearer in terms of managing memory.
	D3D12MA::ALLOCATION_DESC allocDesc = {};
	allocDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;

	HRESULT hr = Graphics::gAllocator->CreateResource(&allocDesc, &bufferDesc,
													  D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
													  &mAllocation, IID_PPV_ARGS(&mResource));

	assert(SUCCEEDED(hr) && "Failed to create upload buffer with D3D12MemoryAllocator");
	mGpuVirtualAddress = mResource->GetGPUVirtualAddress();

	UINT8* pDataBegin = nullptr;
	CD3DX12_RANGE readRange(0, 0);

	hr = mResource->Map(0, &readRange, reinterpret_cast<void**>(&pDataBegin));
	assert(SUCCEEDED(hr) && "Failed to map upload buffer");

	memcpy(pDataBegin, data, sizeInBytes);

	mResource->Unmap(0, nullptr);

	mUsageState = D3D12_RESOURCE_STATE_GENERIC_READ;
}

void UploadBuffer::Initialize(UINT sizeInBytes)
{
	assert(Graphics::gAllocator != nullptr && "D3D12MemoryAllocator not initialized");

	D3D12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeInBytes);
	D3D12MA::ALLOCATION_DESC allocDesc = {};
	allocDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;

	HRESULT hr = Graphics::gAllocator->CreateResource(&allocDesc, &bufferDesc,
													  D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
													  &mAllocation, IID_PPV_ARGS(&mResource));

	assert(SUCCEEDED(hr) && "Failed to create upload buffer with D3D12MemoryAllocator");
	mGpuVirtualAddress = mResource->GetGPUVirtualAddress();

	CD3DX12_RANGE readRange(0, 0);
	hr = mResource->Map(0, &readRange, reinterpret_cast<void**>(&mMappedData));
	assert(SUCCEEDED(hr) && "Failed to map upload buffer");

	mUsageState = D3D12_RESOURCE_STATE_GENERIC_READ;
}

void UploadBuffer::Copy(const void* data, UINT sizeInBytes, UINT offset)
{
	assert(mMappedData != nullptr && "Buffer must be initialized for dynamic uploading");

	auto* dest = static_cast<uint8_t*>(mMappedData);
	memcpy(dest + offset, data, sizeInBytes);
}
