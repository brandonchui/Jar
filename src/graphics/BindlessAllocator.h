#pragma once

#include <wrl.h>
#include "DescriptorHeap.h"
#include "directx/d3d12.h"
#include <cstdint>

struct Allocation
{
	uint32_t mStartIndex;
	uint32_t mCount;

	bool IsValid() const { return mCount != UINT32_MAX && mStartIndex != UINT32_MAX; }
};

class DescriptorHandle;

class BindlessAllocator
{

public:
	BindlessAllocator() = default;
	~BindlessAllocator();

	bool Initialize(uint32_t numDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE descriptorType);
	void Shutdown();

	D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandleStart() const { return mCpuStart; }
	D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandleStart() const { return mGpuStart; }

	ID3D12DescriptorHeap* GetHeap() const { return mHeap.Get(); }
	uint32_t GetHeapSize() const { return mHeapCount; }
	uint32_t GetDescriptorSize() const { return mDescriptorSize; }

	// Allocations
	Allocation Allocate(uint32_t count);

	DescriptorHandle GetHandle(uint32_t index);

private:
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mHeap;
	D3D12_DESCRIPTOR_HEAP_TYPE mDescriptorType;

	uint32_t mHeapCount;
	uint32_t mNextFreeIndex;

	uint32_t mDescriptorSize;

	bool mIsShaderVisible;

	D3D12_CPU_DESCRIPTOR_HANDLE mCpuStart;

	D3D12_GPU_DESCRIPTOR_HANDLE mGpuStart;
};
