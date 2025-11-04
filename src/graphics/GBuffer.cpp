#include "GBuffer.h"
#include "CommandContext.h"
#include "Core.h"
#include "d3d12.h"
#include <spdlog/spdlog.h>

void GBuffer::Create(uint32_t width, uint32_t height)
{
	if (width <= 0 || height <= 0)
		return;

	Destroy();

	mWidth = width;
	mHeight = height;

	mRenderTarget0.Create(L"GBuffer_Albedo_AO", mWidth, mHeight, 1, DXGI_FORMAT_R8G8B8A8_UNORM);
	mRenderTarget0.CreateView(Graphics::gDevice);

	mRenderTarget1.Create(L"GBuffer_Normal_Roughness", mWidth, mHeight, 1,
						  DXGI_FORMAT_R16G16B16A16_FLOAT);
	mRenderTarget1.CreateView(Graphics::gDevice);

	mRenderTarget2.Create(L"GBuffer_Metallic_Flags", mWidth, mHeight, 1,
						  DXGI_FORMAT_R8G8B8A8_UNORM);
	mRenderTarget2.CreateView(Graphics::gDevice);

	mRenderTarget3.Create(L"GBuffer_Emissive", mWidth, mHeight, 1, DXGI_FORMAT_R16G16B16A16_FLOAT);
	mRenderTarget3.CreateView(Graphics::gDevice);

	mDepth.Create(L"GBuffer_Depth", mWidth, mHeight, DXGI_FORMAT_D32_FLOAT);
	mDepth.CreateView(Graphics::gDevice);

	// Log?
}

void GBuffer::Destroy()
{
	mRenderTarget0.Destroy();
	mRenderTarget1.Destroy();
	mRenderTarget2.Destroy();
	mRenderTarget3.Destroy();

	mDepth.Destroy();

	mWidth = 0;
	mHeight = 0;
}

void GBuffer::Resize(uint32_t width, uint32_t height)
{
	if (width == mWidth && height == mHeight)
		return;

	Create(width, height);
}

void GBuffer::Clear(Graphics::GraphicsContext& ctx)
{
	float clearColor[4] = {0.0F, 0.0F, 0.0F, 0.0F};

	ctx.ClearColor(mRenderTarget0.GetRTV(), clearColor);
	ctx.ClearColor(mRenderTarget1.GetRTV(), clearColor);
	ctx.ClearColor(mRenderTarget2.GetRTV(), clearColor);
	ctx.ClearColor(mRenderTarget3.GetRTV(), clearColor);

	ctx.ClearDepth(mDepth.GetDSV(), 1.0F);
}

void GBuffer::SetAsRenderTargets(Graphics::GraphicsContext& ctx)
{
	// TODO: Implement setting GBuffer as render targets
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[4] = {
		mRenderTarget0.GetRTV(),
		mRenderTarget1.GetRTV(),
		mRenderTarget2.GetRTV(),
		mRenderTarget3.GetRTV(),
	};

	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = mDepth.GetDSV();

	ctx.GetCommandList()->OMSetRenderTargets(4, rtvHandles, FALSE, &dsvHandle);

	ctx.SetViewport(0.0F, 0.0F, static_cast<float>(mWidth), static_cast<float>(mHeight));
	ctx.SetScissorRect(0, 0, mWidth, mHeight);
}
