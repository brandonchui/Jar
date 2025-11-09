#pragma once

#include "PixelBuffer.h"
#include "DescriptorHeap.h"
#include "BindlessAllocator.h"

namespace Graphics
{
	extern BindlessAllocator* gBindlessAllocator;
}

/// Color render target with RTV support for pixel shader outputs.
/// The ColorBuffer holds additional render target variables while
/// the PixelBuffer just holds width, height, format
class ColorBuffer : public PixelBuffer
{
public:
	ColorBuffer();
	~ColorBuffer();

	/// Creates a new color buffer texture resource
	void Create(const wchar_t* name, uint32_t width, uint32_t height, uint32_t arraySize,
				DXGI_FORMAT format, bool allowUAV = false);

	/// Fills parameters diretly from the swapchain resource.
	void CreateFromSwapChain(const wchar_t* name, ID3D12Resource* pResource);

	/// Create the RTV for this buffer. Will allocate descriptor internally.
	void CreateView(ID3D12Device* pDevice);

	/// Create an SRV for this color buffer so it can be sampled as a texture.
	void CreateSRV(D3D12_CPU_DESCRIPTOR_HANDLE srvHandle);

	/// Create a UAV for this buffer. Mostly in the use case we need some
	/// texture output, otherwise just use StructuredBuffer instead.
	void CreateUAV(D3D12_CPU_DESCRIPTOR_HANDLE uavHandle);

	D3D12_CPU_DESCRIPTOR_HANDLE GetRTV() const { return mRtv; }
	D3D12_CPU_DESCRIPTOR_HANDLE GetUAV() const { return mUav.GetCpuHandle(); }

	uint32_t GetSRVIndex() const
	{
		return mSrvAllocation.IsValid()
			? mSrvAllocation.mStartIndex
			: Graphics::gBindlessAllocator->GetNullDescriptorIndex(NullDescriptor::Texture2D);
	}
	uint32_t GetUAVIndex() const
	{
		return mUavAllocation.IsValid()
			? mUavAllocation.mStartIndex
			: Graphics::gBindlessAllocator->GetNullDescriptorIndex(NullDescriptor::Texture2D);
	}
	bool HasSRV() const { return mSrvAllocation.IsValid(); }
	bool HasUAV() const { return mUavAllocation.IsValid(); }

private:
	DescriptorHandle mRtv;
	DescriptorHandle mUav;

	Allocation mSrvAllocation;
	Allocation mUavAllocation;
};
