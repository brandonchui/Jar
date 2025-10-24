#include "CommandListManager.h"
#include <cassert>
#include <windows.h>

CommandQueue::CommandQueue(D3D12_COMMAND_LIST_TYPE type)
: TYPE(type)
{
}

CommandQueue::~CommandQueue()
{
	Shutdown();
}

void CommandQueue::Create(ID3D12Device* pDevice)
{
	assert(pDevice != nullptr);

	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = TYPE;
	queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.NodeMask = 0;

	HRESULT hr = pDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCommandQueue));
	assert(SUCCEEDED(hr) && "Failed to create command queue");

	hr = pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence));
	assert(SUCCEEDED(hr) && "Failed to create fence");

	mFenceEventHandle = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
	assert(mFenceEventHandle != nullptr && "Failed to create fence event");

#ifdef _DEBUG
	if (TYPE == D3D12_COMMAND_LIST_TYPE_DIRECT)
		mCommandQueue->SetName(L"Graphics Command Queue");
	else if (TYPE == D3D12_COMMAND_LIST_TYPE_COMPUTE)
		mCommandQueue->SetName(L"Compute Command Queue");
	else if (TYPE == D3D12_COMMAND_LIST_TYPE_COPY)
		mCommandQueue->SetName(L"Copy Command Queue");
	mFence->SetName(L"Command Queue Fence");
#endif
}

void CommandQueue::Shutdown()
{
	if (mFenceEventHandle != nullptr)
	{
		CloseHandle(mFenceEventHandle);
		mFenceEventHandle = nullptr;
	}

	mFence.Reset();
	mCommandQueue.Reset();
}

uint64_t CommandQueue::ExecuteCommandList(ID3D12CommandList* list)
{
	assert(list != nullptr);
	assert(mCommandQueue != nullptr);

	ID3D12CommandList* ppCommandLists[] = {list};
	mCommandQueue->ExecuteCommandLists(1, ppCommandLists);

	mCommandQueue->Signal(mFence.Get(), mNextFenceValue);

	return mNextFenceValue++;
}

void CommandQueue::WaitForFence(uint64_t fenceValue)
{
	if (mFence->GetCompletedValue() < fenceValue)
	{
		mFence->SetEventOnCompletion(fenceValue, mFenceEventHandle);
		WaitForSingleObject(mFenceEventHandle, INFINITE);
	}
	mLastCompletedFenceValue = fenceValue;
}

void CommandListManager::Create(ID3D12Device14* pDevice)
{
	assert(pDevice != nullptr);
	mDevice = pDevice;

	mGraphicsQueue.Create(pDevice);
}

void CommandListManager::Shutdown()
{
	mGraphicsQueue.Shutdown();
	mDevice = nullptr;
}

void CommandListManager::CreateNewCommandList(D3D12_COMMAND_LIST_TYPE type,
											  ID3D12GraphicsCommandList** list,
											  ID3D12CommandAllocator** allocator)
{
	assert(mDevice != nullptr);
	assert(list != nullptr && allocator != nullptr);

	HRESULT hr = mDevice->CreateCommandAllocator(type, IID_PPV_ARGS(allocator));
	assert(SUCCEEDED(hr) && "Failed to create command allocator");

	hr = mDevice->CreateCommandList(0, type, *allocator, nullptr, IID_PPV_ARGS(list));
	assert(SUCCEEDED(hr) && "Failed to create command list");

	(*list)->Close();
}

void CommandListManager::WaitForFence(uint64_t fenceValue)
{
	mGraphicsQueue.WaitForFence(fenceValue);
}

uint64_t CommandQueue::Signal()
{
	mCommandQueue->Signal(mFence.Get(), mNextFenceValue);
	return mNextFenceValue++;
}

ID3D12CommandQueue* CommandQueue::GetCommandQueue()
{
	return mCommandQueue.Get();
}
