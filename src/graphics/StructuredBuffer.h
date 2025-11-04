#pragma once

#include "GpuBuffer.h"
#include <string>
#include <memory>
#include <spdlog/spdlog.h>
#include <d3d12.h>

namespace Graphics
{
	/// Array of typed structs that are accessible by shaders. For
	/// use cases that deal with variable array length and don't
	/// need to update that much, otherwise check out cbuffers.
	class StructuredBuffer : public GpuBuffer
	{
	public:
		StructuredBuffer() = default;
		~StructuredBuffer() override;

		/// Create a data structure accessible by the shader. Essentially,
		/// a shader resource view later on.
		/// If provided initialData, the function will upload
		/// automatically.
		void Create(const std::wstring& name, uint32_t numElements, uint32_t elementSize,
					const void* initialData = nullptr, bool allowUAV = false);

		/// Uploads the bytes into the GPU. Usually, I use this to put the
		/// entire array (std::vector for now) in the entire StructuredBuffer
		/// class.
		void Upload(const void* data, size_t dataSize, size_t destinationOffset = 0);

		/// We create a shader resource view so the shader can see the data
		/// that we upload.
		void CreateSRV(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle) const;

		/// Create a UAV for compute shader write access.
		/// Needs ALLOW_UNORDERED_ACCESS flag
		void CreateUAV(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle);

		uint32_t GetElementCount() const { return mElementCount; }
		uint32_t GetElementSize() const { return mElementSize; }
		size_t GetBufferSize() const { return mBufferSize; }

		D3D12_CPU_DESCRIPTOR_HANDLE GetSRV() const { return mSrvCpuHandle; }
		D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGpu() const { return mSrvGpuHandle; }
		D3D12_CPU_DESCRIPTOR_HANDLE GetUAV() const { return mUavCpuHandle; }
		D3D12_GPU_DESCRIPTOR_HANDLE GetUAVGpu() const { return mUavGpuHandle; }

		void SetSRVHandles(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
						   D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle);
		void SetUAVHandles(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
						   D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle);

	private:
		void InitLogger();
		static std::shared_ptr<spdlog::logger> sLogger;
		/// Used to check the end bound of how big the array should be.
		/// This is used in the SRV descriptor.
		uint32_t mElementCount = 0;

		/// The size of each struct that will be used for the descriptor's
		/// stride paramters.
		uint32_t mElementSize = 0;
		size_t mBufferSize = 0;

		D3D12_CPU_DESCRIPTOR_HANDLE mSrvCpuHandle = {};
		D3D12_GPU_DESCRIPTOR_HANDLE mSrvGpuHandle = {};
		D3D12_CPU_DESCRIPTOR_HANDLE mUavCpuHandle = {};
		D3D12_GPU_DESCRIPTOR_HANDLE mUavGpuHandle = {};
	};

} // namespace Graphics
