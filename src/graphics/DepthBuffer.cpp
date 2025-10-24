#include "DepthBuffer.h"
#include "Core.h"
#include "CommandContext.h"
#include <d3dx12/d3dx12.h>
#include <cassert>

DepthBuffer::DepthBuffer() = default;

void DepthBuffer::Create(const wchar_t* name, uint32_t width, uint32_t height, DXGI_FORMAT format)
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
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.Format = format;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = format;
	clearValue.DepthStencil.Depth = 1.0F;
	clearValue.DepthStencil.Stencil = 0;

	CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);

	HRESULT hr = Graphics::gDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc,
															D3D12_RESOURCE_STATE_DEPTH_WRITE,
															&clearValue, IID_PPV_ARGS(&mResource));

	assert(SUCCEEDED(hr) && "Failed to create depth buffer");

	// Initial state.
	mUsageState = D3D12_RESOURCE_STATE_DEPTH_WRITE;

#ifdef _DEBUG
	mResource->SetName(name);
#endif
}

void DepthBuffer::CreateView(ID3D12Device* pDevice)
{
	assert(pDevice != nullptr);
	assert(mResource != nullptr);

	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle =
		Graphics::gDescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_DSV]->Allocate(1);
	mDSV = DescriptorHandle(dsvHandle, {0});

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = mFormat;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

	pDevice->CreateDepthStencilView(mResource.Get(), &dsvDesc, dsvHandle);
}

void DepthBuffer::Clear(Graphics::GraphicsContext& context, float depth)
{
	context.ClearDepth(mDSV, depth);
}
