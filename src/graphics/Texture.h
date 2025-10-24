#pragma once

#include "GpuResource.h"
#include <d3d12.h>
#include <string>
#include <memory>
#include <vector>
#include <spdlog/spdlog.h>
#include <wrl/client.h>

namespace Graphics
{
	class GraphicsContext;
}

class Texture : public GpuResource
{
public:
	Texture();
	~Texture() override;

	/// Loads a DDS file from the file path. Currently not supporting
	/// .png or .tga though probably should TODO support png, tga
	bool LoadFromFile(const std::wstring& filepath);

	DXGI_FORMAT GetFormat() const { return mFormat; }
	uint32_t GetWidth() const { return mWidth; }
	uint32_t GetHeight() const { return mHeight; }
	uint32_t GetMipLevels() const { return mMipLevels; }

	/// Creates a shader resource view given a DescriptorHandle's
	/// CPU handle variable.
	void CreateSRV(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle) const;

	D3D12_CPU_DESCRIPTOR_HANDLE GetSRV() const { return mSrvCpuHandle; }
	D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGpu() const { return mSrvGpuHandle; }

	/// Sets the internal CPU/GPU handles from the DescriptorHandle
	/// class.
	void SetSRVHandles(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
					   D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle);

	/// Uploads texture to GPU immediately, but has to block
	/// until the GPU finishes.
	void UploadToGPU();

	/// Uses an existing GraphicsContext to upload texture to GPU.
	/// Good in situations where you need non blocking uploading.
	/// Needs the memory to be managed.
	bool UploadDeferredData(Graphics::GraphicsContext& context);

	/// Checks if either UploadToGpu or UploadDeferredData has been
	/// used, or checks if it failed to upload.
	bool NeedsUpload() const
	{
		return mDeferredUploadData != nullptr && (mDeferredUploadData->uploadBuffer == nullptr);
	}

	/// Clears the buffer from the deferred upload texture data.
	void ClearUploadBuffer();

private:
	void InitLogger();
	static std::shared_ptr<spdlog::logger> sLogger;
	DXGI_FORMAT mFormat;
	uint32_t mWidth;
	uint32_t mHeight;
	uint32_t mMipLevels;

	D3D12_CPU_DESCRIPTOR_HANDLE mSrvCpuHandle = {};
	D3D12_GPU_DESCRIPTOR_HANDLE mSrvGpuHandle = {};

	struct DeferredUploadData
	{
		// NOTE will most likely change the param name later
		std::unique_ptr<uint8_t[]> ddsData;
		std::vector<D3D12_SUBRESOURCE_DATA> subresources;
		Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuffer;
	};

	std::unique_ptr<DeferredUploadData> mDeferredUploadData;
};
