#pragma once

#include <queue>
#include <wrl.h>
#include "DescriptorHeap.h"
#include "directx/d3d12.h"
#include <cstdint>
#include <map>
#include <vector>
#include <memory>
#include <spdlog/spdlog.h>

struct Allocation
{
	uint32_t mStartIndex;
	uint32_t mCount;

	/// Using generations to prevent stale or invalid indices.
	uint32_t mGeneration;

	bool IsValid() const { return mCount != UINT32_MAX && mStartIndex != UINT32_MAX; }

	void Reset() { mStartIndex = mCount = mGeneration = UINT32_MAX; }
};

struct PendingDeletion
{
	Allocation mAllocation;
	uint64_t mFence;
};

/// Indices 0 - 4
enum NullDescriptor
{
	Texture2D,
	Texture3D,
	TextureCube,
	Buffer,
	StructuredBuffer,
};

class DescriptorHandle;

class BindlessAllocator
{

public:
	BindlessAllocator() = default;
	~BindlessAllocator() = default;

	bool Initialize(uint32_t numDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE descriptorType);

	void Shutdown();

	D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandleStart() const { return mCpuStart; }
	D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandleStart() const { return mGpuStart; }

	ID3D12DescriptorHeap* GetHeap() const { return mHeap.Get(); }
	ID3D12DescriptorHeap* GetHeapPointer() const { return mHeap.Get(); }
	uint32_t GetHeapSize() const { return mHeapCount; }
	uint32_t GetDescriptorSize() const { return mDescriptorSize; }

	/// Allocations
	Allocation Allocate(uint32_t count);

	DescriptorHandle GetHandle(uint32_t index);

	void FreeDeferred(Allocation allocation, uint64_t fence);
	void ProcessDeletions(uint64_t completedFence);

	/// Generation
	uint32_t GetGeneration(uint32_t index) { return mGenerations[index]; }

	bool IsValid(const Allocation& allocation) const;

	/// Nulls
	uint32_t GetNullDescriptorIndex(NullDescriptor kind) const;

private:
	void CreateNullDescriptors();

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mHeap;
	D3D12_DESCRIPTOR_HEAP_TYPE mDescriptorType;

	uint32_t mHeapCount;
	uint32_t mNextFreeIndex;

	std::vector<uint32_t> mGenerations;

	uint32_t mDescriptorSize;

	bool mIsShaderVisible;

	D3D12_CPU_DESCRIPTOR_HANDLE mCpuStart;

	D3D12_GPU_DESCRIPTOR_HANDLE mGpuStart;

	/// Free List
	/// NOTE look into intrusive lists for optimization

	/// This data structure uses:
	///     Key:   Block size, how many descriptors
	///     Value: List of start indcies with that (key) size
	///
	/// When we free an Allocation {startIndex, count}, example we add to the free list
	/// that we just free 10 descriptors at index 50, so mFreeList[10].push_back(50).
	///
	/// In other words, track how many slots in the descriptor heap we can reuse.
	/// NOTE Slow, look into splitting or optimization if profile shows slowness.
	std::map<uint32_t, std::vector<uint32_t>> mFreeList;

	bool Free(Allocation allocation);

	/// Pending Queue
	std::queue<PendingDeletion> mPendingDeletion;

	/// Logger
	static std::shared_ptr<spdlog::logger> sLogger;
	void InitLogger();
};
