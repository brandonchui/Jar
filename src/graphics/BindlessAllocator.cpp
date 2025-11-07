#include "BindlessAllocator.h"
#include "Core.h"
#include "directx/d3d12.h"

bool BindlessAllocator::Initialize(uint32_t numDescriptors,
								   D3D12_DESCRIPTOR_HEAP_TYPE descriptorType)
{
	if (Graphics::gDevice == nullptr)
	{
		return false;
	}

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
	if (mNextFreeIndex + count > mHeapCount)
	{
		//TODO log
		return {UINT32_MAX, UINT32_MAX};
	}
	Allocation res = {};
	res.mCount = count;
	res.mStartIndex = mNextFreeIndex;

	mNextFreeIndex += count;

	return res;
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
