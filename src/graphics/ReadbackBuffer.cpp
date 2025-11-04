#include "ReadbackBuffer.h"
#include "Core.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include <d3dx12/d3dx12.h>

namespace Graphics
{
	std::shared_ptr<spdlog::logger> ReadbackBuffer::sLogger = nullptr;

	void ReadbackBuffer::InitLogger()
	{
		if (!sLogger)
		{
			sLogger = spdlog::get("ReadbackBuffer");
			if (!sLogger)
			{
				sLogger = spdlog::stdout_color_mt("ReadbackBuffer");
				sLogger->set_pattern("[%H:%M:%S] [%^%l%$] [%n] %v");
				sLogger->set_level(spdlog::level::debug);
			}
		}
	}

	ReadbackBuffer::~ReadbackBuffer()
	{
		if (mMappedData)
		{
			Unmap();
		}
	}

	void ReadbackBuffer::Create(const std::wstring& name, uint32_t sizeInBytes)
	{
		InitLogger();
		mBufferSize = sizeInBytes;

		D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
		D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(sizeInBytes);

		HRESULT hr = gDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc,
													  D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
													  IID_PPV_ARGS(&mResource));

		if (FAILED(hr))
		{
			sLogger->error("Failed to create readback buffer");
			return;
		}

		mResource->SetName(name.c_str());
		mUsageState = D3D12_RESOURCE_STATE_COPY_DEST;

		sLogger->info("Created readback buffer '{}': {} bytes",
					  std::string(name.begin(), name.end()), sizeInBytes);
	}

	void ReadbackBuffer::Unmap()
	{
		if (!mResource)
		{
			sLogger->error("Cannot unmap - buffer not created");
			return;
		}

		if (!mMappedData)
		{
			sLogger->warn("Buffer not mapped");
			return;
		}

		D3D12_RANGE writeRange = {0, 0};
		mResource->Unmap(0, &writeRange);
		mMappedData = nullptr;
	}

} // namespace Graphics
