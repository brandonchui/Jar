#pragma once

#include "ColorBuffer.h"
#include "DepthBuffer.h"
#include <cstdint>

namespace Graphics
{
	class GraphicsContext;
}

struct GBuffer
{
	void Create(uint32_t width, uint32_t height);
	void Destroy();
	void Resize(uint32_t width, uint32_t height);
	void Clear(Graphics::GraphicsContext& ctx);
	void SetAsRenderTargets(Graphics::GraphicsContext& ctx);

	ColorBuffer& GetRenderTarget0() { return mRenderTarget0; }
	ColorBuffer& GetRenderTarget1() { return mRenderTarget1; }
	ColorBuffer& GetRenderTarget2() { return mRenderTarget2; }
	ColorBuffer& GetRenderTarget3() { return mRenderTarget3; }
	DepthBuffer& GetDepthBuffer() { return mDepth; }

	uint32_t GetWidth() const { return mWidth; }
	uint32_t GetHeight() const { return mHeight; }

private:
	// rgba8_UNORM
	// Albedo / AO
	ColorBuffer mRenderTarget0;

	// rgb16_float
	// World Normal, Roughness
	ColorBuffer mRenderTarget1;

	//rgba8_unorm
	// Metallic / null / null / Flags
	// TODO material id?
	ColorBuffer mRenderTarget2;

	// rgba16_float
	// Emissive / null
	ColorBuffer mRenderTarget3;

	// d32_float
	DepthBuffer mDepth;

	uint32_t mWidth = 0;
	uint32_t mHeight = 0;
};
