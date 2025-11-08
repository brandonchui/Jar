#pragma once

#include "PixelBuffer.h"
#include "DescriptorHeap.h"
#include "BindlessAllocator.h"

namespace Graphics
{
	class GraphicsContext;
}

/// Creates a Depth Buffer with some standard start up settings
/// that trivially works
class DepthBuffer : public PixelBuffer
{
public:
	DepthBuffer();
	~DepthBuffer();

	/// Creates a ID3D12Resource as the depth buffer.
	/// Sets the initial state to D3D12_RESOURCE_STATE_DEPTH_WRITE;
	void Create(const wchar_t* name, uint32_t width, uint32_t height,
				DXGI_FORMAT format = DXGI_FORMAT_D32_FLOAT);

	/// Create the DSV for this buffer. Will allocate descriptor internally.
	void CreateView(ID3D12Device* pDevice);

	/// Create an SRV so can be sampled.
	void CreateSRV(D3D12_CPU_DESCRIPTOR_HANDLE srvHandle);

	D3D12_CPU_DESCRIPTOR_HANDLE GetDSV() const { return mDSV; }

	uint32_t GetSRVIndex() const { return mSrvAllocation.mStartIndex; }
	bool HasSRV() const { return mSrvAllocation.IsValid(); }

	/// Clears the depth target
	void Clear(Graphics::GraphicsContext& context, float depth = 1.0F);

private:
	DescriptorHandle mDSV;
	Allocation mSrvAllocation;
};
