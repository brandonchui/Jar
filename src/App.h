#pragma once

#include <SDL3/SDL.h>
#include <cstdint>
#include <Windows.h>
#include <memory>
#include <d3d12.h>
#include <wrl/client.h>
#include <spdlog/spdlog.h>
#include <imgui.h>

namespace Graphics
{
	class GraphicsContext;
}

class App
{
public:
	App();

	/// There should only ever be one single App class
	/// running, so just making sure these constructors
	/// don't function.
	App(const App&) = delete;
	App(App&&) = delete;
	App& operator=(const App&) = delete;
	App& operator=(App&&) = delete;
	~App();

	/// The main initialization for everything the Renderer
	/// needs, such as SDL window, DX12 objects, and the
	/// initialization of graphics objects.
	bool Initialize();

	/// Cleans up allocated resources for the App class.
	void Shutdown();

	/// Updates the Swapchain and Viewport with the new
	/// width and height parameters.
	void Resize(uint32_t width, uint32_t height);

	/// Processing all SDL events.
	bool ProcessEvent(const SDL_Event& event);

	/// Iteratively calls at each tick update for the App class.
	void Update(float deltaTime);

	/// Renders a GraphicsContext object (has the command list,
	/// allocator, root sig, PSO).
	void Render(Graphics::GraphicsContext& context);

	/// Mainly for the swapchain so it know where to draw the
	/// texture.
	HWND GetHwnd() const;

	/// Capture the previous frame tick, usually for getting
	/// the delta time.
	uint64_t& GetLastTickCount() { return mLastTicks; }

	bool IsRunning() const { return mRunning; }
	SDL_Window* GetWindow() const { return mWindow; }

	/// Get the config manager
	class ConfigManager* GetConfigManager() const { return mConfigManager.get(); }

private:
	/// Renders all UI widgets - called by UISystem callback
	void RenderUI();
	/// The main app drivers for the rendering. We grab the
	/// SDL window proc with the new SDL3 api that will get
	/// fed into the swapchain.

	/// The main Windows window from SDL.
	SDL_Window* mWindow = nullptr;

	/// Swapchain.
	std::unique_ptr<class SwapChain> mSwapChain;

	/// Currently, the Renderer holds all needed objects to
	/// be able to render. Will need to look into changing
	/// this later, feels too coupled.
	std::unique_ptr<class Renderer> mRenderer;

	/// UI system for Dear ImGui integration
	std::unique_ptr<class UISystem> mUISystem;

	/// Configuration manager for loading/saving settings
	std::unique_ptr<class ConfigManager> mConfigManager;

	/// Logger for App messages
	std::shared_ptr<spdlog::logger> mLogger;

	uint64_t mLastTicks = 0;
	bool mRunning = true;
	bool mInitialized = false;
	bool mIsRotatingCamera = false;

	/// This bool flag is for the imgui widget to see if the
	/// widget itself is hovered. Since we only care about
	/// camera controls within the widget itself we have to
	/// see the mouse positions later.
	bool mViewportHovered = false;

	/// The custom titlebar needs some imgui specific flags to
	/// track the drag deltas, mouse position and the various
	/// edges.
	bool mIsDraggingWindow = false;
	ImVec2 mDragOffset;
	bool mIsResizingWindow = false;
	int32_t mResizeEdge = 0;
	ImVec2 mResizeStartMousePos;
	ImVec2 mResizeStartWindowPos;
	ImVec2 mResizeStartWindowSize;

	/// SDL specific cursor for the resizing handles for the entire
	/// window.
	SDL_Cursor* mCursorDefault = nullptr;
	SDL_Cursor* mCursorNWSE = nullptr;
	SDL_Cursor* mCursorNESW = nullptr;
	SDL_Cursor* mCursorWE = nullptr;
	SDL_Cursor* mCursorNS = nullptr;

	float mDpiScale;

	static constexpr const char* WINDOW_TITLE = "Jar";
	static constexpr float TITLE_BAR_HEIGHT = 32.0F;
};
