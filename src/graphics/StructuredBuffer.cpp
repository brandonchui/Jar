#include "StructuredBuffer.h"
#include "Core.h"
#include "CommandContext.h"
#include "CommandListManager.h"
#include "UploadBuffer.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include <d3dx12/d3dx12.h>

namespace Graphics
{
	std::shared_ptr<spdlog::logger> StructuredBuffer::sLogger = nullptr;

	void StructuredBuffer::InitLogger()
	{
		if (!sLogger)
		{
			sLogger = spdlog::get("StructuredBuffer");
			if (!sLogger)
			{
				sLogger = spdlog::stdout_color_mt("StructuredBuffer");
				sLogger->set_pattern("[%H:%M:%S] [%^%l%$] [%n] %v");
				sLogger->set_level(spdlog::level::debug);
			}
		}
	}

	StructuredBuffer::~StructuredBuffer()
	{
		//clean up
	}

	void StructuredBuffer::Create(const std::wstring& name, uint32_t numElements,
								  uint32_t elementSize, const void* initialData)
	{
		InitLogger();
		mElementCount = numElements;
		mElementSize = elementSize;
		mBufferSize = static_cast<size_t>(numElements) * static_cast<size_t>(elementSize);

		D3D12_RESOURCE_DESC desc = {};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Width = mBufferSize;
		desc.Height = 1;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.Format = DXGI_FORMAT_UNKNOWN;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		desc.Flags = D3D12_RESOURCE_FLAG_NONE;

		D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

		D3D12_RESOURCE_STATES initialState = initialData ? D3D12_RESOURCE_STATE_COPY_DEST
														 : D3D12_RESOURCE_STATE_COMMON;

		HRESULT hr = gDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc,
													  initialState, nullptr,
													  IID_PPV_ARGS(&mResource));

		if (FAILED(hr))
		{
			sLogger->error("Failed to create buffer");
			return;
		}

		mResource->SetName(name.c_str());
		mUsageState = initialState;
		mGpuVirtualAddress = mResource->GetGPUVirtualAddress();

		sLogger->info("Created buffer '{}': {} elements x {} bytes = {} bytes total",
					  std::string(name.begin(), name.end()), numElements, elementSize, mBufferSize);

		if (initialData)
		{
			Upload(initialData, mBufferSize);
		}
	}

	void StructuredBuffer::Upload(const void* data, size_t dataSize, size_t destinationOffset)
	{
		if (!mResource)
		{
			sLogger->error("Buffer not created");
			return;
		}

		if (destinationOffset + dataSize > mBufferSize)
		{
			sLogger->error("Upload size exceeds buffer size");
			return;
		}

		UploadBuffer uploadBuffer;
		uploadBuffer.Initialize(data, static_cast<uint32_t>(dataSize));

		GraphicsContext uploadContext;
		uploadContext.Create(gDevice);
		uploadContext.Begin();

		if (mUsageState != D3D12_RESOURCE_STATE_COPY_DEST)
		{
			uploadContext.TransitionResource(*this, D3D12_RESOURCE_STATE_COPY_DEST);
		}

		uploadContext.GetCommandList()->CopyBufferRegion(mResource.Get(), destinationOffset,
														 uploadBuffer.GetResource(), 0, dataSize);

		// Recall structuredbuffers use srv
		uploadContext.TransitionResource(*this, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE |
													D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		mUsageState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE |
					  D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

		uploadContext.Flush();
		auto& queue = gCommandListManager->GetGraphicsQueue();
		uint64_t fenceValue = queue.ExecuteCommandList(uploadContext.GetCommandList());
		queue.WaitForFence(fenceValue);
	}

	void StructuredBuffer::CreateSRV(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle) const
	{
		if (!mResource)
		{
			sLogger->error("Can't create SRV because buffer not created");
			return;
		}

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = mElementCount;
		srvDesc.Buffer.StructureByteStride = mElementSize;
		srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

		gDevice->CreateShaderResourceView(mResource.Get(), &srvDesc, cpuHandle);

		sLogger->debug("Created SRV for {} elements of {} bytes", mElementCount, mElementSize);
	}

	void StructuredBuffer::SetSRVHandles(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
										 D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle)
	{
		mSrvCpuHandle = cpuHandle;
		mSrvGpuHandle = gpuHandle;
	}

} // namespace Graphics
