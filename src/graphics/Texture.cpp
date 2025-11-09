#include "Texture.h"
#include "Core.h"
#include "CommandContext.h"
#include "CommandListManager.h"
#include "DescriptorHeap.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include <DDSTextureLoader.h>
#include <cstdint>
#include <d3dx12/d3dx12.h>
#include <vector>

std::shared_ptr<spdlog::logger> Texture::sLogger = nullptr;

void Texture::InitLogger()
{
	if (!sLogger)
	{
		sLogger = spdlog::get("Texture");
		if (!sLogger)
		{
			sLogger = spdlog::stdout_color_mt("Texture");
			sLogger->set_pattern("[%H:%M:%S] [%^%l%$] [%n] %v");
			sLogger->set_level(spdlog::level::debug);
		}
	}
}

Texture::Texture()
: mWidth(0)
, mHeight(0)
, mMipLevels(1)
{
	mSrvAllocation.Reset();
}

Texture::~Texture()
{
	if (mSrvAllocation.IsValid())
	{
		uint64_t lastSignaledFence =
			Graphics::gCommandListManager->GetGraphicsQueue().GetLastSignaledFenceValue();
		Graphics::gBindlessAllocator->FreeDeferred(mSrvAllocation, lastSignaledFence);

		mSrvAllocation.Reset();
	}
}

bool Texture::LoadFromFile(const std::wstring& filepath)
{
	InitLogger();
	using namespace DirectX;

	sLogger->info("Loading texture from: {}", std::string(filepath.begin(), filepath.end()));

	// Deferred the texture upload until we need it.
	mDeferredUploadData = std::make_unique<DeferredUploadData>();
	DDS_ALPHA_MODE alphaMode = DDS_ALPHA_MODE_UNKNOWN;

	HRESULT hr = LoadDDSTextureFromFileEx(
		Graphics::gDevice, filepath.c_str(), 0, D3D12_RESOURCE_FLAG_NONE, DDS_LOADER_DEFAULT,
		&mResource, mDeferredUploadData->ddsData, mDeferredUploadData->subresources, &alphaMode);

	if (FAILED(hr))
	{
		sLogger->error("Failed to load DDS file. HRESULT: 0x{:08X}", static_cast<unsigned int>(hr));
		mDeferredUploadData.reset();

		mSrvAllocation.Reset();

		return false;
	}

	D3D12_RESOURCE_DESC desc = mResource->GetDesc();
	mFormat = desc.Format;
	mWidth = static_cast<uint32_t>(desc.Width);
	mHeight = static_cast<uint32_t>(desc.Height);
	mMipLevels = static_cast<uint32_t>(desc.MipLevels);

	sLogger->info("Loaded texture: {}x{}, {} mips, format: {}", mWidth, mHeight, mMipLevels,
				  static_cast<int>(mFormat));

	mUsageState = D3D12_RESOURCE_STATE_COPY_DEST;
	mGpuVirtualAddress = 0;

	sLogger->debug("Texture loaded awaiting upload");
	return true;
}

void Texture::CreateSRV(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle)
{
	mSrvAllocation = Graphics::gBindlessAllocator->Allocate(1);
	if (!mSrvAllocation.IsValid())
	{
		sLogger->error("Failed to allocate SRV descriptor from bindless heap");
		return;
	}

	DescriptorHandle handle = Graphics::gBindlessAllocator->GetHandle(mSrvAllocation.mStartIndex);

	SetSRVHandles(handle.GetCpuHandle(), handle.GetGpuHandle());

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = mFormat;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = mMipLevels;
	srvDesc.Texture2D.PlaneSlice = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0F;

	// Deprecated, moving to Bindless
	// Graphics::gDevice->CreateShaderResourceView(mResource.Get(), &srvDesc, cpuHandle);
	Graphics::gDevice->CreateShaderResourceView(mResource.Get(), &srvDesc, handle.GetCpuHandle());
}

void Texture::SetSRVHandles(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
							D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle)
{
	mSrvCpuHandle = cpuHandle;
	mSrvGpuHandle = gpuHandle;
}

bool Texture::UploadDeferredData(Graphics::GraphicsContext& context)
{
	if (!mDeferredUploadData)
	{
		return true;
	}

	sLogger->debug("Uploading deferred texture data");

	const UINT64 UPLOAD_BUFFER_SIZE = GetRequiredIntermediateSize(
		mResource.Get(), 0, static_cast<UINT>(mDeferredUploadData->subresources.size()));

	CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
	CD3DX12_RESOURCE_DESC uploadBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(UPLOAD_BUFFER_SIZE);

	//BUG use memory allocator
	HRESULT hr = Graphics::gDevice->CreateCommittedResource(
		&uploadHeapProps, D3D12_HEAP_FLAG_NONE, &uploadBufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(&mDeferredUploadData->uploadBuffer));

	if (FAILED(hr))
	{
		sLogger->error("Failed to create upload buffer");
		return false;
	}

	UpdateSubresources(context.GetCommandList(), mResource.Get(),
					   mDeferredUploadData->uploadBuffer.Get(), 0, 0,
					   static_cast<UINT>(mDeferredUploadData->subresources.size()),
					   mDeferredUploadData->subresources.data());

	context.TransitionResource(*this, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	mUsageState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

	// Don't clear the deferred data just yet
	sLogger->debug("Texture data uploaded successfully");
	return true;
}

void Texture::UploadToGPU()
{
	if (!mDeferredUploadData)
	{
		return;
	}

	sLogger->debug("Starting immediate GPU upload");

	Graphics::GraphicsContext uploadContext;
	uploadContext.Create(Graphics::gDevice);
	uploadContext.Begin();

	if (!UploadDeferredData(uploadContext))
	{
		sLogger->error("Failed to upload texture data");
		return;
	}

	uploadContext.Flush();
	auto& queue = Graphics::gCommandListManager->GetGraphicsQueue();
	uint64_t fenceValue = queue.ExecuteCommandList(uploadContext.GetCommandList());
	queue.WaitForFence(fenceValue);

	if (mDeferredUploadData)
	{
		mDeferredUploadData.reset();
		sLogger->debug("Upload buffer cleared after GPU completion");
	}

	sLogger->info("GPU upload complete");
}

void Texture::ClearUploadBuffer()
{
	if (mDeferredUploadData)
	{
		mDeferredUploadData.reset();
		sLogger->debug("Upload buffer cleared");
	}
}
