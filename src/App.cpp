#include "App.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_properties.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include "graphics/Core.h"
#include "graphics/SwapChain.h"
#include "graphics/CommandListManager.h"
#include "graphics/CommandContext.h"
#include "graphics/ColorBuffer.h"
#include "graphics/slang/SlangCore.h"

#include "Renderer.h"
#include "Scene.h"
#include "ui/UISystem.h"
#include "ui/widgets/Viewport.h"
#include "ui/widgets/Outliner.h"
#include "ui/widgets/TitleBar.h"
#include "ui/widgets/Properties.h"
#include "imgui.h"

App::App() = default;

App::~App()
{
	Shutdown();
}

bool App::Initialize()
{
	mLogger = spdlog::stdout_color_mt("App");
	mLogger->set_pattern("[%H:%M:%S] [%^%l%$] [%n] %v");

	mWindow = SDL_CreateWindow(
		WINDOW_TITLE, static_cast<int>(WINDOW_WIDTH), static_cast<int>(WINDOW_HEIGHT),
		SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_BORDERLESS);

	if (mWindow == nullptr)
	{
		mLogger->error("Failed to create window: {}", SDL_GetError());
		return false;
	}

	// Get DPI scale for UI scalig. Querying the info from SDL.
	SDL_DisplayID displayID = SDL_GetDisplayForWindow(mWindow);
	if (displayID != 0)
	{
		float contentScale = SDL_GetDisplayContentScale(displayID);
		if (contentScale > 0.0F)
		{
			mDpiScale = contentScale;
		}
	}

	mLogger->info("Display DPI scale: {:.2f}", mDpiScale);

	// Setting up the global DX12 objects including the debug layers.
	Graphics::Init();

	if (Graphics::gDevice == nullptr)
	{
		mLogger->error("Failed to initialize graphics system");
		SDL_DestroyWindow(mWindow);
		mWindow = nullptr;
		return false;
	}

	mSwapChain = std::make_unique<SwapChain>();
	mSwapChain->Create(GetHwnd(), WINDOW_WIDTH, WINDOW_HEIGHT, Graphics::gDevice,
					   Graphics::gCommandListManager->GetCommandQueue());

	// Initialize UI system FIRST (Dear ImGui)
	// Renderer needs UISystem for allocating viewport SRV from ImGui's descriptor heap
	mUISystem = std::make_unique<UISystem>();
	if (!mUISystem->Initialize(mWindow, Graphics::gDevice,
							   Graphics::gCommandListManager->GetCommandQueue(),
							   DXGI_FORMAT_R8G8B8A8_UNORM, 2))
	{
		mLogger->error("Failed to initialize UI system");
		return false;
	}

	mRenderer = std::make_unique<Renderer>();
	mRenderer->Initialize(mUISystem.get());
	mRenderer->SetViewport(WINDOW_WIDTH, WINDOW_HEIGHT);

	// NOTE This is a slang test for shader test code, due for removal.
	// SlangHelper::Compile();  // Commented out during SlangHelper refactoring

	mLastTicks = SDL_GetTicks();

	// Initialize system cursors for window resizing
	mCursorDefault = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT);
	mCursorNWSE = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NWSE_RESIZE);
	mCursorNESW = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NESW_RESIZE);
	mCursorWE = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_EW_RESIZE);
	mCursorNS = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NS_RESIZE);

	// Main spotlight - configured for material showcase
	SpotLight testLight;
	testLight.position = Float3(16.0F, 44.0F, 51.0F);
	testLight.direction = Float3(0.0F, -0.08F, -1.0F);
	testLight.color = Float3(1.0F, 1.0F, 1.0F);
	testLight.intensity = 4.8F;
	testLight.range = 100.0F;
	testLight.innerConeAngle = 0.436332F; // 25 degrees
	testLight.outerConeAngle = 1.413717F; // 81 degrees
	testLight.falloff = 0.00001F;

	mRenderer->AddSpotLight(testLight);

	// The UI system has a callback that continously calls
	// the RenderUI function in App.cpp. Keeps logic seperated.
	mUISystem->SetRenderCallback([this]() {
		RenderUI();
	});
	mLogger->info("UI system initialized successfully");

	// OK
	mInitialized = true;
	mLogger->info("App initialized successfully");
	return true;
}

void App::Shutdown()
{
	if (Graphics::gCommandListManager)
	{
		CommandQueue& queue = Graphics::gCommandListManager->GetGraphicsQueue();
		uint64_t fence = queue.Signal();
		queue.WaitForFence(fence);
	}

	// Shutdown UI system before renderer.
	if (mUISystem)
	{
		mUISystem->Shutdown();
		mUISystem.reset();
	}

	if (mRenderer)
	{
		mRenderer.reset();
	}

	if (mSwapChain)
	{
		mSwapChain->Shutdown();
		mSwapChain.reset();
	}

	Graphics::Shutdown();

	if (mCursorDefault)
		SDL_DestroyCursor(mCursorDefault);
	if (mCursorNWSE)
		SDL_DestroyCursor(mCursorNWSE);
	if (mCursorNESW)
		SDL_DestroyCursor(mCursorNESW);
	if (mCursorWE)
		SDL_DestroyCursor(mCursorWE);
	if (mCursorNS)
		SDL_DestroyCursor(mCursorNS);

	if (mWindow != nullptr)
	{
		SDL_DestroyWindow(mWindow);
		mWindow = nullptr;
	}

	mInitialized = false;
	if (mLogger)
	{
		mLogger->info("App shut down");
	}
}

bool App::ProcessEvent(const SDL_Event& event)
{
	if (mUISystem)
	{
		mUISystem->ProcessEvent(event);
	}

	/// If ImGui is using the mouse/keyboard, don't pass events
	/// to the application.
	ImGuiIO& io = ImGui::GetIO();

	switch (event.type)
	{
	case SDL_EVENT_QUIT:
		mRunning = false;
		return true;

	case SDL_EVENT_KEY_DOWN:
		if (event.key.scancode == SDL_SCANCODE_ESCAPE)
		{
			mRunning = false;
			return true;
		}
		break;

	case SDL_EVENT_WINDOW_RESIZED:
		Resize(static_cast<uint32_t>(event.window.data1),
			   static_cast<uint32_t>(event.window.data2));
		break;

	case SDL_EVENT_MOUSE_MOTION:
		if (mViewportHovered && mRenderer && mRenderer->GetCamera() && mIsRotatingCamera)
		{
			mRenderer->GetCamera()->OnMouseMove(static_cast<float>(event.motion.xrel),
												static_cast<float>(event.motion.yrel),
												mIsRotatingCamera);
		}
		break;

	case SDL_EVENT_MOUSE_BUTTON_DOWN:
		if (mViewportHovered && event.button.button == SDL_BUTTON_LEFT)
		{
			mIsRotatingCamera = true;
		}
		break;

	case SDL_EVENT_MOUSE_BUTTON_UP:
		if (event.button.button == SDL_BUTTON_LEFT)
		{
			mIsRotatingCamera = false;
		}
		break;

	case SDL_EVENT_MOUSE_WHEEL:
		if (mViewportHovered && mRenderer && mRenderer->GetCamera())
		{
			mRenderer->GetCamera()->OnMouseWheel(static_cast<float>(event.wheel.y));
		}
		break;

	case SDL_EVENT_DROP_FILE:
		if (event.drop.data != nullptr && mLogger)
		{
			mLogger->info("File dropped: {}", event.drop.data);
		}
		break;

	default:
		break;
	}

	return false;
}

void App::Update(float deltaTime)
{
	mRenderer->Update(deltaTime);
}

void App::Render(Graphics::GraphicsContext& context)
{
	if (!mSwapChain || !Graphics::gCommandListManager)
		return;

	if (mUISystem)
	{
		mUISystem->NewFrame();
	}

	// Render scene to offscreen viewport texture
	mRenderer->Render(context);

	// Since we already have an offscreen texture for the main contents
	// of the Renderer, we now draw the UI part. This will all go through
	// the main swapchain onto the render target.
	ColorBuffer& currentBackBuffer = Graphics::gDisplayPlane[Graphics::gCurrentBuffer];
	context.TransitionResource(currentBackBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
	context.SetRenderTarget(currentBackBuffer.GetRTV());

	// Probably not needed as the imgui dockspace will have its own clearing
	// it seems like.
	float clearColor[4] = {0.1F, 0.1F, 0.1F, 1.0F};
	context.ClearColor(currentBackBuffer.GetRTV(), clearColor);

	if (mUISystem)
	{
		mUISystem->Render(context.GetCommandList());
	}

	context.TransitionResource(currentBackBuffer, D3D12_RESOURCE_STATE_PRESENT);
	context.Flush();

	// Execute the closed command list.
	CommandQueue& queue = Graphics::gCommandListManager->GetGraphicsQueue();
	uint64_t fenceValue = queue.ExecuteCommandList(context.GetCommandList());

	mSwapChain->Present();

	queue.WaitForFence(fenceValue);
}

HWND App::GetHwnd() const
{
	if (mWindow == nullptr)
	{
		return nullptr;
	}

	// SDL3 uses a new API to get the HWND compared to the SDL2.
	// Refer back to the SDL3 migration guide for more details.
	SDL_PropertiesID props = SDL_GetWindowProperties(mWindow);
	HWND hwnd = static_cast<HWND>(
		SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));

	return hwnd;
}

void App::Resize(uint32_t width, uint32_t height)
{
	if (mSwapChain && mRenderer && (Graphics::gCommandListManager != nullptr))
	{
		if (mLogger)
		{
			mLogger->info("Resizing to {}x{}", width, height);
		}

		CommandQueue& queue = Graphics::gCommandListManager->GetGraphicsQueue();

		// Let the GPU finish the frame before destorying it as part
		// of the Resize() call.
		uint64_t fenceValue = queue.Signal();
		queue.WaitForFence(fenceValue);

		mSwapChain->Resize(width, height);

		mRenderer->SetViewport(width, height);
	}
}

void App::RenderUI()
{
	/// Show title bar FIRST.
	UI::TitleBarState titleBarState = UI::ShowTitleBar(
		mWindow, WINDOW_TITLE, mIsDraggingWindow, mDragOffset, mIsResizingWindow, mResizeEdge,
		mResizeStartMousePos, mResizeStartWindowPos, mResizeStartWindowSize, mCursorDefault,
		mCursorNWSE, mCursorNESW, mCursorWE, mCursorNS, mDpiScale);

	if (titleBarState.action == UI::TitleBarAction::Close)
	{
		mRunning = false;
	}

	/// Create dockspace below title bar (from the docking branch).
	/// NOTE this is using a modified imgui fork.
	const float titleBarHeight = TITLE_BAR_HEIGHT * mDpiScale;
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + titleBarHeight));
	ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, viewport->WorkSize.y - titleBarHeight));
	ImGui::SetNextWindowViewport(viewport->ID);

	ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar;
	windowFlags |= ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize;
	windowFlags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus;
	windowFlags |= ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0F, 8.0F));

	/// Using imgui's dockspace for nice default UI layouts and docking
	/// like a retained styled UI.
	ImGui::Begin("DockSpace", nullptr, windowFlags);
	ImGui::PopStyleVar();

	ImGuiID dockspaceId = ImGui::GetID("MyDockSpace");
	ImGui::DockSpace(dockspaceId, ImVec2(0.0F, 0.0F), ImGuiDockNodeFlags_PassthruCentralNode);

	ImGui::End();

	/// We are now checking which of the UI widgets state.
	static bool isViewportOpen = true;
	D3D12_GPU_DESCRIPTOR_HANDLE viewportSRV = mRenderer->GetViewportSRV();
	UI::ViewportState viewportState = UI::ShowViewport(&isViewportOpen, viewportSRV);
	mViewportHovered = viewportState.isHovered;

	/// Viewport
	if (viewportState.size.x > 0 && viewportState.size.y > 0)
	{
		mRenderer->ResizeViewport(static_cast<uint32_t>(viewportState.size.x),
								  static_cast<uint32_t>(viewportState.size.y));
	}

	static bool isOutlinerOpen = true;

	/// Outliner
	std::vector<UI::OutlinerItem> items;
	Scene* scene = mRenderer->GetScene();
	if (scene)
	{
		for (const auto& entity : scene->GetEntities())
		{
			items.push_back({.mEntityId = entity->GetId(),
							 .mName = entity->GetName(),
							 .mVisible = entity->IsVisible(),
							 .mSelected = entity->IsSelected()});
		}
	}

	UI::OutlinerCallbacks callbacks;
	callbacks.mGetMeshName = [this](uint32_t id) -> std::string {
		Scene* s = mRenderer->GetScene();
		if (s)
		{
			Entity* entity = s->GetEntity(id);
			return entity ? entity->GetName() : "";
		}
		return "";
	};
	callbacks.mIsVisible = [this](uint32_t id) -> bool {
		Scene* s = mRenderer->GetScene();
		if (s)
		{
			Entity* entity = s->GetEntity(id);
			return entity ? entity->IsVisible() : false;
		}
		return false;
	};
	callbacks.mSetVisible = [this](uint32_t id, bool visible) {
		Scene* s = mRenderer->GetScene();
		if (s)
		{
			Entity* entity = s->GetEntity(id);
			if (entity)
			{
				entity->SetVisible(visible);
			}
		}
	};
	callbacks.mOnSelect = [this](uint32_t id) {
		Scene* s = mRenderer->GetScene();
		if (s)
		{
			s->SetSelected(id, true);
		}
	};
	callbacks.mOnDelete = [this](uint32_t id) {
		Scene* s = mRenderer->GetScene();
		if (s)
		{
			s->RemoveEntity(id);
		}
	};

	UI::ShowOutliner(&isOutlinerOpen, items, callbacks);

	/// Properties widget
	static bool isPropertiesOpen = true;

	// TODO Fake data
	static UI::TransformProperties transform = {.position = {0.0F, 0.0F, 0.0F},
												.rotation = {0.0F, 0.0F, 0.0F},
												.scale = {1.0F, 1.0F, 1.0F}};

	UI::PropertiesCallbacks propCallbacks;
	propCallbacks.onTransformChanged = [](const UI::TransformProperties& t) {
		// TODO: Apply transform to selected object
	};

	propCallbacks.onSpotLightChanged = [this]() {
		mRenderer->UpdateSpotLight();
	};

	propCallbacks.onBlurIntensityChanged = [this](float intensity) {
		mRenderer->SetBlurIntensity(intensity);
	};

	SpotLight* spotlight = mRenderer->GetSpotLight();
	float blurIntensity = mRenderer->GetBlurIntensity();
	UI::ShowProperties(&isPropertiesOpen, "", transform, propCallbacks, spotlight, &blurIntensity);
}
