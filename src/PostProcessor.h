#pragma once

#include "graphics/ColorBuffer.h"
#include "graphics/DescriptorHeap.h"
#include <memory>
#include <spdlog/spdlog.h>

namespace Graphics
{
	class GraphicsContext;
}

class PostProcessor
{
public:
	PostProcessor() = default;
	~PostProcessor() = default;

	void Initialize(uint32_t width, uint32_t height, DescriptorHeap* textureHeap);
	void Resize(uint32_t width, uint32_t height);

	void ApplyBlur(Graphics::GraphicsContext& context, ColorBuffer& target,
				   DescriptorHeap* samplerHeap);

	void SetBlurIntensity(float intensity) { mBlurIntensity = intensity; }
	float GetBlurIntensity() const { return mBlurIntensity; }

private:
	void InitLogger();

	std::unique_ptr<ColorBuffer> mTempTexture;

	DescriptorHandle mTempSRV;
	DescriptorHandle mTempUAV;

	float mBlurIntensity = 0.0F;
	uint32_t mWidth = 0;
	uint32_t mHeight = 0;

	std::shared_ptr<spdlog::logger> mLogger;
};
