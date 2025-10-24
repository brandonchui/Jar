#pragma once

#include <SDL3/SDL.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <functional>
#include "../graphics/DescriptorHeap.h"

/// UISystem is a wrapper around Imgui that revolves around callbacks.
/// Handles initialization, event processing, and rendering.
///
/// Usage:
///   auto ui = std::make_unique<UISystem>();
///   ui->Initialize(window, device, commandQueue, format, numFrames);
///   ui->SetRenderCallback([]() { ImGui::ShowDemoWindow(); });
///
/// In event loop:
///   ui->ProcessEvent(event);
///
/// In render loop:
///   ui->NewFrame();
///   ui->Render(commandList);
class UISystem
{
public:
	UISystem() = default;
	~UISystem();

	/// Initialize Imgui with SDL3 and DirectX12 backends.
	bool Initialize(SDL_Window* window, ID3D12Device* device, ID3D12CommandQueue* commandQueue,
					DXGI_FORMAT rtvFormat, uint32_t numFrames);

	void Shutdown();

	/// Start a new ImGui frame.
	/// Should be called at the start of each frame, before any ImGui calls.
	void NewFrame();

	/// Render ImGui draw data to the command list.
	/// Call after ImGui::Render() and before presenting the frame.
	void Render(ID3D12GraphicsCommandList* commandList);

	/// Forward SDL3 events to ImGui for input handling.
	void ProcessEvent(const SDL_Event& event);

	/// Callback function type for UI rendering.
	/// Use this to build your the actual UI.
	using UICallback = std::function<void()>;

	/// Set the callback for rendering custom UI.
	/// The callback is invoked during NewFrame(), before ImGui::Render(),
	/// typically used as part of a catch all RenderUI(), which in itself
	/// has all the other UI widgets etc.
	void SetRenderCallback(UICallback callback);

	bool IsInitialized() const { return mInitialized; }

	/// Allocate a descriptor from ImGui's SRV heap.
	/// Using this for textures that will be displayed in imgui, like
	/// the viewport textures.
	DescriptorHandle AllocateDescriptor(uint32_t count = 1);

private:
	/// Dedicated descriptor heap for imgui SRV textures, mainly
	/// fonts or textures.
	DescriptorHeap mImGuiSrvHeap;

	/// User provided callback for rendering custom UI.
	UICallback mRenderCallback;

	bool mInitialized = false;
};
