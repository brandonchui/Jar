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

Allocation BindlessAllocator::Allocate(uint32_t count)
{
	Allocation res = {};

	if (mFreeList.contains(count))
	{
		// Free list contains available _count_ of descriptors. Use the value pair to get start.
		res.mCount = count;
		res.mStartIndex = mFreeList.at(count).back();

		mFreeList.at(count).pop_back();
	}
	else
	{
		if (mNextFreeIndex + count > mHeapCount)
		{
			sLogger->error("Out of bounds");
			return {.mCount = UINT32_MAX, .mStartIndex = UINT32_MAX};
		}

		// Free list is empty OR no suitable allocation
		// TODO should be implemented to find split blocks or something
		res.mCount = count;
		res.mStartIndex = mNextFreeIndex;

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
	//TODO
}
void BindlessAllocator::ProcessDeletions(uint64_t completedFence)
{
	//TODO
}
