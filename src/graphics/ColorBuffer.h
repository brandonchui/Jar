#pragma once

#include "PixelBuffer.h"
#include "DescriptorHeap.h"

/// Color render target with RTV support for pixel shader outputs.
/// The ColorBuffer holds additional render target variables while
/// the PixelBuffer just holds width, height, format
class ColorBuffer : public PixelBuffer
{
public:
	ColorBuffer();

	/// Creates a new color buffer texture resource
	void Create(const wchar_t* name, uint32_t width, uint32_t height, uint32_t arraySize,
				DXGI_FORMAT format);

	/// Fills parameters diretly from the swapchain resource.
	void CreateFromSwapChain(const wchar_t* name, ID3D12Resource* pResource);

	/// Create the RTV for this buffer. Will allocate descriptor internally.
	void CreateView(ID3D12Device* pDevice);

	/// Create an SRV for this color buffer so it can be sampled as a texture.
	void CreateSRV(D3D12_CPU_DESCRIPTOR_HANDLE srvHandle);

	D3D12_CPU_DESCRIPTOR_HANDLE GetRTV() const { return mRtv; }

private:
	DescriptorHandle mRtv;
};
