#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <cstdint>

/// Encapsulates a D3D12 command queue with fence synchronization.
/// TODO ring fence
class CommandQueue
{
	friend class CommandListManager;

public:
	CommandQueue(D3D12_COMMAND_LIST_TYPE type);
	~CommandQueue();

	/// Creates command queue and Fence, signals and create event.
	void Create(ID3D12Device* pDevice);
	void Shutdown();

	/// Seteventoncompletion wait for single object, not really the best
	/// method for the waiting for sync.
	void WaitForFence(uint64_t fenceValue);

	/// Signal the queue and return fence value
	uint64_t Signal();

	/// Execute command list and return fence value. The list is filled
	/// seperately in another data structured, the primary goal of the
	/// function is to only execute the list.
	uint64_t ExecuteCommandList(ID3D12CommandList* list);

	uint64_t GetCompletedFenceVlaue() const { return mFence->GetCompletedValue(); }

	ID3D12CommandQueue* GetCommandQueue();

private:
	const D3D12_COMMAND_LIST_TYPE TYPE;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> mCommandQueue;
	Microsoft::WRL::ComPtr<ID3D12Fence> mFence;
	uint64_t mNextFenceValue = 1;
	uint64_t mLastCompletedFenceValue = 0;
	HANDLE mFenceEventHandle = nullptr;
};

/// Manages GPU queues and provides factory methods for creating command resources.
/// NOTE: Misleading name, it manages queues rather than command lists.
class CommandListManager
{
public:
	CommandListManager() = default;
	~CommandListManager() = default;

	/// Convienance function that just stored internal pointer for the device,
	/// and also creates the CommandQueue
	void Create(ID3D12Device14* pDevice);
	void Shutdown();

	CommandQueue& GetGraphicsQueue() { return mGraphicsQueue; }

	/// Only supporting a DIRECT command list type since we are not using
	/// compute shaders just yet.
	/// TODO support other queues
	CommandQueue& GetQueue(D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT)
	{
		switch (type)
		{
		case D3D12_COMMAND_LIST_TYPE_DIRECT:
			return mGraphicsQueue;
		case D3D12_COMMAND_LIST_TYPE_COMPUTE:
		case D3D12_COMMAND_LIST_TYPE_COPY:
		default:
			// No compute or copy yet do not add just placeholder or refactor
			return mGraphicsQueue;
		}
	}

	void CreateNewCommandList(D3D12_COMMAND_LIST_TYPE type, ID3D12GraphicsCommandList** list,
							  ID3D12CommandAllocator** allocator);

	ID3D12CommandQueue* GetCommandQueue() { return mGraphicsQueue.GetCommandQueue(); }

	//not sure if needed?
	void WaitForFence(uint64_t fenceValue);

private:
	ID3D12Device14* mDevice = nullptr;
	CommandQueue mGraphicsQueue{D3D12_COMMAND_LIST_TYPE_DIRECT};
};
