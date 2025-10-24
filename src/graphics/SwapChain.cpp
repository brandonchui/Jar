#include "SwapChain.h"
#include <d3dx12/d3dx12.h>
#include <cassert>
#include "ColorBuffer.h"
#include "DepthBuffer.h"
#include "Core.h"
#include "CommandListManager.h"
#include "spdlog/sinks/stdout_color_sinks.h"

namespace Graphics
{
	// gDisplayPlane is what is shown to the window for a Present
	ColorBuffer gDisplayPlane[2];
	// Global depth buffer that is shared across frames
	DepthBuffer gSceneDepthBuffer;
	uint32_t gCurrentBuffer = 0;
} // namespace Graphics

void SwapChain::InitLogger()
{
	if (!mLogger)
	{
		mLogger = spdlog::get("SwapChain");
		if (!mLogger)
		{
			mLogger = spdlog::stdout_color_mt("SwapChain");
			mLogger->set_pattern("[%H:%M:%S] [%^%l%$] [%n] %v");
		}
	}
}

void SwapChain::Create(HWND hwnd, uint32_t width, uint32_t height, ID3D12Device14* device,
					   ID3D12CommandQueue* commandQueue)
{
	InitLogger();

	assert(device != nullptr);
	assert(commandQueue != nullptr);
	assert(hwnd != nullptr);

	mDevice = device;
	mWidth = width;
	mHeight = height;

	Microsoft::WRL::ComPtr<IDXGIFactory7> factory;
	HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));
	assert(SUCCEEDED(hr) && "Failed to create DXGI factory");

	// Describe swap chain
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.BufferCount = BUFFER_COUNT;
	swapChainDesc.Width = width;
	swapChainDesc.Height = height;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	swapChainDesc.Scaling = DXGI_SCALING_STRETCH;

	Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain1;
	hr = factory->CreateSwapChainForHwnd(commandQueue, hwnd, &swapChainDesc, nullptr, nullptr,
										 &swapChain1);
	assert(SUCCEEDED(hr) && "Failed to create swap chain");

	hr = swapChain1.As(&mSwapChain);
	assert(SUCCEEDED(hr) && "Failed to get SwapChain4 interface");

	// Disable Alt Enter fullscreen
	factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

	// Create RTVs for swap chain buffers
	CreateRTVs();

	// Create depth buffer
	Graphics::gSceneDepthBuffer.Create(L"Scene Depth Buffer", width, height, DXGI_FORMAT_D32_FLOAT);
	Graphics::gSceneDepthBuffer.CreateView(device);

	Graphics::gCurrentBuffer = mSwapChain->GetCurrentBackBufferIndex();

	mLogger->info("Created {}x{} swap chain with {} buffers", width, height, BUFFER_COUNT);
	mLogger->info("Created {}x{} depth buffer", width, height);
}

void SwapChain::CreateRTVs()
{
	using namespace Graphics;

	for (uint32_t i = 0; i < BUFFER_COUNT; i++)
	{
		HRESULT hr = mSwapChain->GetBuffer(i, IID_PPV_ARGS(&mBackBuffers[i]));
		assert(SUCCEEDED(hr) && "Failed to get swap chain buffer");

		std::wstring name = L"Display Plane " + std::to_wstring(i);
		gDisplayPlane[i].CreateFromSwapChain(name.c_str(), mBackBuffers[i].Get());

		gDisplayPlane[i].CreateView(mDevice);
	}
}

void SwapChain::Present()
{
	HRESULT hr = mSwapChain->Present(1, 0);
	assert(SUCCEEDED(hr) && "Failed to present");

	Graphics::gCurrentBuffer = mSwapChain->GetCurrentBackBufferIndex();
}

void SwapChain::Resize(uint32_t width, uint32_t height)
{
	InitLogger();

	if (width == mWidth && height == mHeight)
		return;

	mWidth = width;
	mHeight = height;

	for (uint32_t i = 0; i < BUFFER_COUNT; i++)
	{
		mBackBuffers[i].Reset();
		Graphics::gDisplayPlane[i].Destroy();
	}

	Graphics::gSceneDepthBuffer.Destroy();

	HRESULT hr = mSwapChain->ResizeBuffers(BUFFER_COUNT, width, height, DXGI_FORMAT_UNKNOWN, 0);
	assert(SUCCEEDED(hr) && "Failed to resize swap chain");

	CreateRTVs();

	Graphics::gSceneDepthBuffer.Create(L"Scene Depth Buffer", width, height, DXGI_FORMAT_D32_FLOAT);
	Graphics::gSceneDepthBuffer.CreateView(mDevice);

	Graphics::gCurrentBuffer = mSwapChain->GetCurrentBackBufferIndex();

	mLogger->info("Resized to {}x{}", width, height);
}

void SwapChain::Shutdown()
{
	if (Graphics::gCommandListManager)
	{
		auto& queue = Graphics::gCommandListManager->GetGraphicsQueue();
		uint64_t fence = queue.Signal();
		queue.WaitForFence(fence);
	}

	for (uint32_t i = 0; i < BUFFER_COUNT; i++)
	{
		mBackBuffers[i].Reset();
		Graphics::gDisplayPlane[i].Destroy();
	}
	Graphics::gSceneDepthBuffer.Destroy();
	mSwapChain.Reset();
}
