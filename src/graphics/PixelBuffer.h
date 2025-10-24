#pragma once

#include "GpuResource.h"
#include <cstdint>
#include <dxgi1_6.h>

/// Pixel and texture ONLY data with this class having
/// both DepthBuffer and ColorBuffer as children.
class PixelBuffer : public GpuResource
{
public:
	PixelBuffer();

	uint32_t GetWidth() const { return mWidth; }
	uint32_t GetHeight() const { return mHeight; }
	DXGI_FORMAT GetFormat() const { return mFormat; }

	void CreateTextureResource();

protected:
	uint32_t mWidth = 0;
	uint32_t mHeight = 0;
	DXGI_FORMAT mFormat = DXGI_FORMAT_UNKNOWN;
};
