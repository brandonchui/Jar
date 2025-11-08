#include "BindlessAllocator.h"
#include "Core.h"
#include "directx/d3d12.h"
#include <spdlog/sinks/stdout_color_sinks.h>

std::shared_ptr<spdlog::logger> BindlessAllocator::sLogger = nullptr;

void BindlessAllocator::InitLogger()
{
	if (!sLogger)
	{
		sLogger = spdlog::get("BindlessAllocator");
		if (!sLogger)
		{
			sLogger = spdlog::stdout_color_mt("BindlessAllocator");
			sLogger->set_pattern("[%H:%M:%S] [%^%l%$] [%n] %v");
			sLogger->set_level(spdlog::level::debug);
		}
	}
}

bool BindlessAllocator::Initialize(uint32_t numDescriptors,
								   D3D12_DESCRIPTOR_HEAP_TYPE descriptorType)
{
	if (Graphics::gDevice == nullptr)
	{
		return false;
	}
	InitLogger();

	mDescriptorType = descriptorType;
	if (mDescriptorType == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
	{
		mIsShaderVisible = true;
	}

	// Init with descriptor size and all zeroes
	mGenerations.resize(numDescriptors, 0);

	mDescriptorSize = Graphics::gDevice->GetDescriptorHandleIncrementSize(mDescriptorType);

	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = numDescriptors;
	desc.Flags = mIsShaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
								  : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	desc.Type = descriptorType;
	desc.NodeMask = 0;

	HRESULT hr = Graphics::gDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&mHeap));
	if (FAILED(hr))
	{
		return false;
	}

	mCpuStart = mHeap->GetCPUDescriptorHandleForHeapStart();
	mGpuStart = mIsShaderVisible ? mHeap->GetGPUDescriptorHandleForHeapStart()
								 : D3D12_GPU_DESCRIPTOR_HANDLE{0};

	mHeapCount = numDescriptors;
	mNextFreeIndex = 0;

	return true;
}

void BindlessAllocator::Shutdown()
{
	// Process any remaining pending deletions before we ultimately shutdown
	if (!mPendingDeletion.empty())
	{
		sLogger->warn("Shutting down with {} pending deletions", mPendingDeletion.size());

		while (!mPendingDeletion.empty())
		{
			Free(mPendingDeletion.front().mAllocation);
			mPendingDeletion.pop();
		}
	}

	mFreeList.clear();
	mHeap.Reset();

	sLogger->info("Shutdown complete");
}

Allocation BindlessAllocator::Allocate(uint32_t count)
{
	Allocation res = {};

	if (mFreeList.contains(count))
	{
		// Free list contains available _count_ of descriptors. Use the value pair to get start.
		res.mCount = count;
		res.mStartIndex = mFreeList.at(count).back();
		res.mGeneration = mGenerations[res.mStartIndex];

		mFreeList.at(count).pop_back();
	}
	else
	{
		if (mNextFreeIndex + count > mHeapCount)
		{
			sLogger->error("Out of bounds");
			return {.mStartIndex = UINT32_MAX, .mCount = UINT32_MAX, .mGeneration = UINT32_MAX};
		}

		// Free list is empty OR no suitable allocation
		// TODO should be implemented to find split blocks or something
		res.mCount = count;
		res.mStartIndex = mNextFreeIndex;
		res.mGeneration = mGenerations[res.mStartIndex];

		mNextFreeIndex += count;
	}

	return res;
}

bool BindlessAllocator::Free(Allocation allocation)
{
	if (allocation.mStartIndex > mHeapCount || !allocation.IsValid())
	{
		return false;
	}

	mFreeList[allocation.mCount].push_back(allocation.mStartIndex);

	uint32_t total = allocation.mStartIndex + allocation.mCount;

	// NOTE Not sure if I am considering overflow at UINT32_MAX ?
	for (uint32_t i = allocation.mStartIndex; i < total; ++i)
	{
		mGenerations[i]++;
	}

	sLogger->info("Freed {} descriptors at index {}", allocation.mCount, allocation.mStartIndex);

	return true;
}

DescriptorHandle BindlessAllocator::GetHandle(uint32_t index)
{
	assert(index < mHeapCount && "Index out of bounds");

	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
	cpuHandle.ptr = mCpuStart.ptr + (static_cast<size_t>(index) * mDescriptorSize);

	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;
	gpuHandle.ptr = mGpuStart.ptr + (static_cast<size_t>(index) * mDescriptorSize);

	return {cpuHandle, gpuHandle};
}

void BindlessAllocator::FreeDeferred(Allocation allocation, uint64_t fence)
{
	mPendingDeletion.push({.mAllocation = allocation, .mFence = fence});
	sLogger->info("Deferred Deletionf for allocation at fence {}", fence);
}

void BindlessAllocator::ProcessDeletions(uint64_t completedFence)
{
	while (!mPendingDeletion.empty())
	{
		auto& pendingDeletion = mPendingDeletion.front();

		if (pendingDeletion.mFence <= completedFence)
		{
			Free(pendingDeletion.mAllocation);
			mPendingDeletion.pop();
		}
		else
		{
			// The fence is still being worked on, so stop checking the rest.
			// Assumes all fences in the rest of the queue are larger.
			break;
		}
	}

	// sLogger->info("Pending deletions from fence {} are now deleted.", completedFence);
}

bool BindlessAllocator::IsValid(const Allocation& allocation) const
{

	if (allocation.mStartIndex >= mHeapCount)
	{
		return false;
	}

	return mGenerations[allocation.mStartIndex] == allocation.mGeneration;
}
