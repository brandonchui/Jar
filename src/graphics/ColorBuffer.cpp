#include "ColorBuffer.h"
#include "Core.h"
#include <d3dx12/d3dx12.h>
#include <cassert>

ColorBuffer::ColorBuffer() = default;

void ColorBuffer::Create(const wchar_t* name, uint32_t width, uint32_t height, uint32_t arraySize, DXGI_FORMAT format)
{
	assert(Graphics::gDevice != nullptr);
	assert(width > 0 && height > 0);

	mWidth = width;
	mHeight = height;
	mFormat = format;

	D3D12_RESOURCE_DESC desc = {};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Width = width;
	desc.Height = height;
	desc.DepthOrArraySize = static_cast<UINT16>(arraySize);
	desc.MipLevels = 1;
	desc.Format = format;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = format;
	clearValue.Color[0] = 0.0f;
	clearValue.Color[1] = 0.0f;
	clearValue.Color[2] = 0.0f;
	clearValue.Color[3] = 1.0f;

	CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);

	HRESULT hr = Graphics::gDevice->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		&clearValue,
		IID_PPV_ARGS(&mResource));

	assert(SUCCEEDED(hr) && "Failed to create color buffer");

	// Initial state
	mUsageState = D3D12_RESOURCE_STATE_RENDER_TARGET;

	// Create the RTV automatically
	CreateView(Graphics::gDevice);

#ifdef _DEBUG
	mResource->SetName(name);
#endif
}

// For already exisiting resources, probably in
// the swapchain creation step, use this function.
void ColorBuffer::CreateFromSwapChain(const wchar_t* name, ID3D12Resource* pResource)
{
	assert(pResource != nullptr);

	mResource = pResource;

	D3D12_RESOURCE_DESC desc = pResource->GetDesc();
	mWidth = static_cast<uint32_t>(desc.Width);
	mHeight = static_cast<uint32_t>(desc.Height);
	mFormat = desc.Format;

	// Set initial state to PRESENT.
	mUsageState = D3D12_RESOURCE_STATE_PRESENT;

#ifdef _DEBUG
	mResource->SetName(name);
#endif
}

void ColorBuffer::CreateView(ID3D12Device* pDevice)
{
	assert(pDevice != nullptr);
	assert(mResource != nullptr);

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle =
		Graphics::gDescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_RTV]->Allocate(1);
	mRtv = DescriptorHandle(rtvHandle, {0});

	pDevice->CreateRenderTargetView(mResource.Get(), nullptr, rtvHandle);
}

void ColorBuffer::CreateSRV(D3D12_CPU_DESCRIPTOR_HANDLE srvHandle)
{
	assert(Graphics::gDevice != nullptr);
	assert(mResource != nullptr);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = mFormat;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;

	Graphics::gDevice->CreateShaderResourceView(mResource.Get(), &srvDesc, srvHandle);
}
