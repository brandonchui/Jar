#include "UISystem.h"
#include "Theme.h"
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_dx12.h"
#include <SDL3/SDL.h>

UISystem::~UISystem()
{
	Shutdown();
}

bool UISystem::Initialize(SDL_Window* window, ID3D12Device* device,
						  ID3D12CommandQueue* commandQueue, DXGI_FORMAT rtvFormat,
						  uint32_t numFrames)
{
	if (mInitialized)
	{
		return true;
	}

	// Create descriptor heap for Imgui SRV descriptors for fonts, custom tex
	constexpr uint32_t IMGUI_SRV_HEAP_SIZE = 512;
	mImGuiSrvHeap.Create(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, IMGUI_SRV_HEAP_SIZE, true);

	// Imgui Init
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	float dpiScale = 1.0F;
	SDL_DisplayID displayID = SDL_GetDisplayForWindow(window);
	if (displayID != 0)
	{
		float contentScale = SDL_GetDisplayContentScale(displayID);
		if (contentScale > 0.0F)
		{
			dpiScale = contentScale;
		}
	}

	// Apply Material Design theme we made, NOT the defaults
	UI::ApplyMaterialTheme(dpiScale);

	// Load custom Robot font
	UI::LoadCustomFont(dpiScale);

	// Setup Platform backend (SDL3)
	if (!ImGui_ImplSDL3_InitForD3D(window))
	{
		ImGui::DestroyContext();
		return false;
	}

	// Setup Renderer backend DirectX12
	ImGui_ImplDX12_InitInfo initInfo = {};
	initInfo.Device = device;
	initInfo.CommandQueue = commandQueue;
	initInfo.NumFramesInFlight = static_cast<int32_t>(numFrames);
	initInfo.RTVFormat = rtvFormat;
	initInfo.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	initInfo.SrvDescriptorHeap = mImGuiSrvHeap.GetHeapPointer();
	initInfo.UserData = this;

	// Provide allocation callbacks for SRV descriptors, see the
	// imgui docs.
	static auto allocFn = +[](ImGui_ImplDX12_InitInfo* info,
							  D3D12_CPU_DESCRIPTOR_HANDLE* outCpuHandle,
							  D3D12_GPU_DESCRIPTOR_HANDLE* outGpuHandle) -> void {
		auto* self = static_cast<UISystem*>(info->UserData);
		DescriptorHandle handle = self->mImGuiSrvHeap.Alloc(1);
		*outCpuHandle = handle.GetCpuHandle();
		*outGpuHandle = handle.GetGpuHandle();
	};

	static auto freeFn = +[](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
							 D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle) -> void {
		(void)info;
		(void)cpuHandle;
		(void)gpuHandle;
	};

	initInfo.SrvDescriptorAllocFn = allocFn;
	initInfo.SrvDescriptorFreeFn = freeFn;

	if (!ImGui_ImplDX12_Init(&initInfo))
	{
		ImGui_ImplSDL3_Shutdown();
		ImGui::DestroyContext();
		return false;
	}

	if (!ImGui_ImplDX12_CreateDeviceObjects())
	{
		ImGui_ImplDX12_Shutdown();
		ImGui_ImplSDL3_Shutdown();
		ImGui::DestroyContext();
		return false;
	}

	mInitialized = true;
	return true;
}

void UISystem::Shutdown()
{
	if (!mInitialized)
	{
		return;
	}

	ImGui_ImplDX12_Shutdown();
	ImGui_ImplSDL3_Shutdown();

	ImGui::DestroyContext();

	mImGuiSrvHeap.Destroy();

	mInitialized = false;
}

void UISystem::NewFrame()
{
	if (!mInitialized)
	{
		return;
	}

	ImGui_ImplDX12_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();

	if (mRenderCallback)
	{
		mRenderCallback();
	}

	ImGui::Render();
}

void UISystem::Render(ID3D12GraphicsCommandList* commandList)
{
	if (!mInitialized)
	{
		return;
	}

	ID3D12DescriptorHeap* heaps[] = {mImGuiSrvHeap.GetHeapPointer()};
	commandList->SetDescriptorHeaps(1, heaps);

	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
}

void UISystem::ProcessEvent(const SDL_Event& event)
{
	if (!mInitialized)
	{
		return;
	}

	// Forward event to the imgui SDL3 backend
	ImGui_ImplSDL3_ProcessEvent(&event);
}

void UISystem::SetRenderCallback(UICallback callback)
{
	mRenderCallback = std::move(callback);
}

DescriptorHandle UISystem::AllocateDescriptor(uint32_t count)
{
	return mImGuiSrvHeap.Alloc(count);
}
