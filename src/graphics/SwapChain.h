#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <windows.h>
#include <spdlog/spdlog.h>
#include "ColorBuffer.h"
#include "DepthBuffer.h"

/// Globals for the buffers needed by the swapchain to write
/// to. We are only doing double buffering for the swapchain.
namespace Graphics
{
	extern ColorBuffer gDisplayPlane[2];
	extern DepthBuffer gSceneDepthBuffer;
	extern uint32_t gCurrentBuffer;
} // namespace Graphics

/// The SwapChain class is responsible for everything related
/// to presenting to the HWND the textures.
class SwapChain
{
public:
	SwapChain() = default;
	~SwapChain() = default;

	/// Creates a swapchain from the device that hooks up to the SDL
	/// window proc HWND.
	void Create(HWND hwnd, uint32_t width, uint32_t height, ID3D12Device14* device,
				ID3D12CommandQueue* commandQueue);

	/// Presents the texture to the HWND aka the window. We are
	/// putting VSync for now, but might change it later, or add
	/// some paramters that allows us to change it.
	void Present();

	/// Destorys the depth/color buffers and recreates them with the
	/// provided parameters.
	void Resize(uint32_t width, uint32_t height);

	void Shutdown();

private:
	void InitLogger();

	/// Calls the ColorBuffer method to create the render target views.
	/// Will create whatever the BUFFER_COUNT is, in our case it is 2.
	void CreateRTVs();

	Microsoft::WRL::ComPtr<IDXGISwapChain4> mSwapChain;

	static constexpr uint32_t BUFFER_COUNT = 2;
	Microsoft::WRL::ComPtr<ID3D12Resource> mBackBuffers[BUFFER_COUNT];

	uint32_t mWidth = 0;
	uint32_t mHeight = 0;

	ID3D12Device14* mDevice = nullptr;
	std::shared_ptr<spdlog::logger> mLogger;
};
