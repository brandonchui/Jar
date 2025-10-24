#pragma once

#include <d3d12.h>
#include <wrl/client.h>

namespace Graphics
{
	class CommandContext;
}

/// A stateful ID3D12Resource wrapper that is designed to be
/// inherited by anything that needs to use a ID3D12Resource
class GpuResource
{
	/// Since the GpuResource holds the current transitioning state,
	/// we need to allow the CommandContex to access it without
	/// polluting the class with many setters/getters
	friend class Graphics::CommandContext;

public:
	GpuResource();
	virtual ~GpuResource();

	virtual void Destroy()
	{
		mResource = nullptr;
		mGpuVirtualAddress = 0;
	}

	ID3D12Resource* GetResource() const { return mResource.Get(); }
	D3D12_GPU_VIRTUAL_ADDRESS GetGpuVirtualAddress() const { return mGpuVirtualAddress; }

protected:
	Microsoft::WRL::ComPtr<ID3D12Resource> mResource;

	/// When we're doing barrier transitions, it is just easier to
	/// track which current state it is in and just change it
	D3D12_RESOURCE_STATES mUsageState = D3D12_RESOURCE_STATE_COMMON;
	D3D12_RESOURCE_STATES mTransitioningState = static_cast<D3D12_RESOURCE_STATES>(-1);

	D3D12_GPU_VIRTUAL_ADDRESS mGpuVirtualAddress = 0;
};
