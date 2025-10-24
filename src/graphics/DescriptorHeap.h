#pragma once

#include <cstdint>
#include <d3d12.h>
#include <wrl/client.h>
// #include <mutex>

/// Simple handle wrapper for CPU/GPU descriptors handles.
class DescriptorHandle
{
public:
	DescriptorHandle()
	{
		mCpuHandle.ptr = 0;
		mGpuHandle.ptr = 0;
	}

	DescriptorHandle(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle)
	: mCpuHandle(cpuHandle)
	, mGpuHandle(gpuHandle)
	{
	}

	void operator+=(uint32_t offsetScaledByDescriptorSize)
	{
		if (mCpuHandle.ptr != 0)
			mCpuHandle.ptr += static_cast<SIZE_T>(offsetScaledByDescriptorSize);
		if (mGpuHandle.ptr != 0)
			mGpuHandle.ptr += static_cast<SIZE_T>(offsetScaledByDescriptorSize);
	}

	/// Conversion operator so no need for getters, so just
	/// use this class in place of actual D3D12_CPU_DESCRIPTOR_HANDLE.
	operator D3D12_CPU_DESCRIPTOR_HANDLE() const { return mCpuHandle; }

	/// GPU handle conversion
	operator D3D12_GPU_DESCRIPTOR_HANDLE() const { return mGpuHandle; }

	D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle() const { return mCpuHandle; }
	D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle() const { return mGpuHandle; }

	bool IsNull() const { return mCpuHandle.ptr == 0; }
	bool IsShaderVisible() const { return mGpuHandle.ptr != 0; }

private:
	/// CPU side - Usually for setting up and managing the descriptors.
	D3D12_CPU_DESCRIPTOR_HANDLE mCpuHandle;

	/// GPU side - Usually for shader visible heaps for draw calls
	/// during command list writing.
	D3D12_GPU_DESCRIPTOR_HANDLE mGpuHandle;
};

/// Linear memory for some # of descriptors.
/// Usually if you know the size beforehand.
class DescriptorHeap
{
public:
	DescriptorHeap() = default;
	~DescriptorHeap() { Destroy(); }

	void Create(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t maxCount, bool shaderVisible = false);
	void Destroy() { mHeap = nullptr; }

	bool HasAvailableSpace(uint32_t count) const { return count <= mNumFreeDescriptors; }
	DescriptorHandle Alloc(uint32_t count = 1);

	ID3D12DescriptorHeap* GetHeapPointer() const { return mHeap.Get(); }

private:
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mHeap;

	/// Descriptor sizes differ per hardware.
	uint32_t mDescriptorSize = 0;

	/// Should error if full for now.
	uint32_t mNumFreeDescriptors = 0;

	/// Keeping track of first index and next free based on how
	/// much we alloc.
	DescriptorHandle mFirstHandle;
	DescriptorHandle mNextFreeHandle;
};

/// Manager for multiple DescriptorHeaps of some kind.
class DescriptorAllocator
{
public:
	DescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE type)
	: mType(type)
	{
		mCurrentHandle.ptr = 0;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE Allocate(uint32_t count);

	static void DestroyAll();

protected:
	static const uint32_t NUM_DESCRIPTORS_PER_HEAP = 256;

	static ID3D12DescriptorHeap* RequestNewHeap(D3D12_DESCRIPTOR_HEAP_TYPE type);

	D3D12_DESCRIPTOR_HEAP_TYPE mType;
	ID3D12DescriptorHeap* mCurrentHeap{};
	D3D12_CPU_DESCRIPTOR_HANDLE mCurrentHandle;
	uint32_t mDescriptorSize{};
	uint32_t mRemainingFreeHandles{};
};
