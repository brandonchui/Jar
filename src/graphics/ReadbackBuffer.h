#pragma once

#include "GpuResource.h"
#include <string>
#include <memory>
#include <spdlog/spdlog.h>
#include <d3d12.h>

namespace Graphics
{
	/// CPU readable buffer for reading GPU results back to the CPU.
	/// NOTE Only for debugging, too slow.
	class ReadbackBuffer : public GpuResource
	{
	public:
		ReadbackBuffer() = default;
		~ReadbackBuffer() override;

		void Create(const std::wstring& name, uint32_t sizeInBytes);

		/// Map the buffer for CPU reading. Returns a typed pointer.
		template <typename T>
		T* Map()
		{
			if (!mResource)
			{
				sLogger->error("Cannot map - buffer not created");
				return nullptr;
			}

			if (mMappedData)
			{
				sLogger->warn("Buffer already mapped");
				return static_cast<T*>(mMappedData);
			}

			D3D12_RANGE readRange = {0, mBufferSize};
			HRESULT hr = mResource->Map(0, &readRange, &mMappedData);

			if (FAILED(hr))
			{
				sLogger->error("Failed to map readback buffer");
				return nullptr;
			}

			return static_cast<T*>(mMappedData);
		}

		void Unmap();

		template <typename T>
		uint32_t GetElementCount() const
		{
			return mBufferSize / sizeof(T);
		}

		uint32_t GetBufferSize() const { return mBufferSize; }

	private:
		void InitLogger();
		static std::shared_ptr<spdlog::logger> sLogger;

		uint32_t mBufferSize = 0;
		void* mMappedData = nullptr;
	};

} // namespace Graphics
