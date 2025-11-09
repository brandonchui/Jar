#include "DepthBuffer.h"
#include "Core.h"
#include "CommandContext.h"
#include "CommandListManager.h"
#include <d3dx12/d3dx12.h>
#include <cassert>

DepthBuffer::DepthBuffer()
{
	mSrvAllocation.Reset();
}

DepthBuffer::~DepthBuffer()
{
	if (mSrvAllocation.IsValid())
	{
		uint64_t lastSignaledFence =
			Graphics::gCommandListManager->GetGraphicsQueue().GetLastSignaledFenceValue();
		Graphics::gBindlessAllocator->FreeDeferred(mSrvAllocation, lastSignaledFence);
		mSrvAllocation.Reset();
	}
}

void DepthBuffer::Create(const wchar_t* name, uint32_t width, uint32_t height, DXGI_FORMAT format)
{
	assert(Graphics::gDevice != nullptr);
	assert(width > 0 && height > 0);

	mWidth = width;
	mHeight = height;
	mFormat = format;

	DXGI_FORMAT resourceFormat = format;
	if (format == DXGI_FORMAT_D32_FLOAT)
	{
		resourceFormat = DXGI_FORMAT_R32_TYPELESS;
	}
	else if (format == DXGI_FORMAT_D24_UNORM_S8_UINT)
	{
		resourceFormat = DXGI_FORMAT_R24G8_TYPELESS;
	}

	D3D12_RESOURCE_DESC desc = {};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Width = width;
	desc.Height = height;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.Format = resourceFormat;
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

void DepthBuffer::CreateSRV(D3D12_CPU_DESCRIPTOR_HANDLE srvHandle)
{
	assert(mResource != nullptr);

	DXGI_FORMAT srvFormat = DXGI_FORMAT_UNKNOWN;
	if (mFormat == DXGI_FORMAT_D32_FLOAT)
	{
		srvFormat = DXGI_FORMAT_R32_FLOAT;
	}
	else if (mFormat == DXGI_FORMAT_D24_UNORM_S8_UINT)
	{
		srvFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	}

	assert(srvFormat != DXGI_FORMAT_UNKNOWN && "Unsupported depth format for SRV");

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = srvFormat;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.PlaneSlice = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0F;

	// If we actually get a valid paramter, it is probably the UI system or something else
	if (srvHandle.ptr != 0)
	{
		Graphics::gDevice->CreateShaderResourceView(mResource.Get(), &srvDesc, srvHandle);
	}
	else
	{
		// Carry on with bindless
		mSrvAllocation = Graphics::gBindlessAllocator->Allocate(1);
		if (!mSrvAllocation.IsValid())
		{
			return;
		}

		DescriptorHandle handle =
			Graphics::gBindlessAllocator->GetHandle(mSrvAllocation.mStartIndex);
		Graphics::gDevice->CreateShaderResourceView(mResource.Get(), &srvDesc,
													handle.GetCpuHandle());
	}
}

void DepthBuffer::Clear(Graphics::GraphicsContext& context, float depth)
{
	context.ClearDepth(mDSV, depth);
}
