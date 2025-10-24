#include "DescriptorHeap.h"
#include "Core.h"
#include <cassert>
#include <vector>

// TODO parallel support
// static std::mutex& GetAllocationMutex()
// {
// 	static std::mutex s_mutex;
// 	return s_mutex;
// }

static std::vector<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>>& GetDescriptorHeapPool()
{
	static std::vector<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>> sPool;
	return sPool;
}

void DescriptorHeap::Create(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t maxCount, bool shaderVisible)
{
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Type = type;
	desc.NumDescriptors = maxCount;
	desc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
							   : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	desc.NodeMask = 0;

	HRESULT hr = Graphics::gDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&mHeap));
	assert(SUCCEEDED(hr));

	mDescriptorSize = Graphics::gDevice->GetDescriptorHandleIncrementSize(type);
	mNumFreeDescriptors = maxCount;

	D3D12_CPU_DESCRIPTOR_HANDLE cpuStart = mHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE gpuStart = shaderVisible
											   ? mHeap->GetGPUDescriptorHandleForHeapStart()
											   : D3D12_GPU_DESCRIPTOR_HANDLE{0};

	mFirstHandle = DescriptorHandle(cpuStart, gpuStart);
	mNextFreeHandle = mFirstHandle;
}

// Returns the handle to the next free slot based on _count_.
DescriptorHandle DescriptorHeap::Alloc(uint32_t count)
{
	assert(HasAvailableSpace(count));

	DescriptorHandle ret = mNextFreeHandle;
	mNextFreeHandle += count * mDescriptorSize;
	mNumFreeDescriptors -= count;

	return ret;
}

ID3D12DescriptorHeap* DescriptorAllocator::RequestNewHeap(D3D12_DESCRIPTOR_HEAP_TYPE type)
{
	// NOTE Mutex should be locked here
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Type = type;
	desc.NumDescriptors = NUM_DESCRIPTORS_PER_HEAP;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	desc.NodeMask = 0;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap;
	HRESULT hr = Graphics::gDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap));
	assert(SUCCEEDED(hr));

	GetDescriptorHeapPool().push_back(heap);
	return heap.Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorAllocator::Allocate(uint32_t count)
{
	// std::lock_guard<std::mutex> lockGuard(GetAllocationMutex());

	if (mCurrentHeap == nullptr || mRemainingFreeHandles < count)
	{
		mCurrentHeap = RequestNewHeap(mType);
		mCurrentHandle = mCurrentHeap->GetCPUDescriptorHandleForHeapStart();
		mRemainingFreeHandles = NUM_DESCRIPTORS_PER_HEAP;

		if (mDescriptorSize == 0)
			mDescriptorSize = Graphics::gDevice->GetDescriptorHandleIncrementSize(mType);
	}

	D3D12_CPU_DESCRIPTOR_HANDLE ret = mCurrentHandle;
	mCurrentHandle.ptr += static_cast<SIZE_T>(count * mDescriptorSize);
	mRemainingFreeHandles -= count;
	return ret;
}

void DescriptorAllocator::DestroyAll()
{
	GetDescriptorHeapPool().clear();
}
