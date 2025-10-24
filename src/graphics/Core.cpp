#include "Core.h"
#include "d3d12sdklayers.h"
#include "d3dcommon.h"
#include "DescriptorHeap.h"
#include "CommandListManager.h"
#include "CommandContext.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include <cstdint>
#include <dxgi1_6.h>
#include <format>
#include <cassert>
#include <string>
#include <windows.h>
#include <D3D12MemAlloc.h>

#if _DEBUG
#include <dxgidebug.h>
#endif

namespace Graphics
{
	// Global variables that are needed across the project. Probably
	// not safe, but it is easier for now.
	D3D_FEATURE_LEVEL gD3DFeatureLevel = D3D_FEATURE_LEVEL_12_2;
	ID3D12Device14* gDevice = nullptr;
	D3D12MA::Allocator* gAllocator = nullptr;

	// Global descriptor allocators for each heap type
	DescriptorAllocator* gDescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES] = {};

	// Global command list management
	CommandListManager* gCommandListManager = nullptr;
	GraphicsContext* gGraphicsContext = nullptr;

	std::shared_ptr<spdlog::logger> gLogger = nullptr;

	void InitLogger()
	{
		if (!gLogger)
		{
			gLogger = spdlog::get("D3D12Core");
			if (!gLogger)
			{
				gLogger = spdlog::stdout_color_mt("D3D12Core");
				gLogger->set_pattern("[%H:%M:%S] [%^%l%$] [%n] %v");
				gLogger->set_level(spdlog::level::debug);
			}
		}
	}

	// NOTE Static functions for this translation unit only!
	static std::string HRESULTToString(HRESULT hr)
	{
		return std::format("HRESULT: 0x{:08X}", static_cast<uint32_t>(hr));
	}

	static void EnableDebugLayers(DWORD& dxgiFactoryFlags)
	{
		HRESULT hr = S_OK;
		uint32_t useDebugLayers = 0;

#if _DEBUG
		useDebugLayers = 1;
#endif

		// Adding debug layers for capturing the warnings/errors during debug
		if (useDebugLayers != 0U)
		{
			Microsoft::WRL::ComPtr<ID3D12Debug6> debugInterface;
			hr = D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface));

			if (SUCCEEDED(hr))
			{
				debugInterface->EnableDebugLayer();

				// Slow but nessessary
				debugInterface->SetEnableGPUBasedValidation(TRUE);
				debugInterface->SetEnableSynchronizedCommandQueueValidation(TRUE);

				gLogger->info("Debug layer enabled");
			}
			else
			{
				gLogger->warn("Failed to enable debug layer: {}", HRESULTToString(hr));
			}

#if _DEBUG
			// DXGI debug interface
			Microsoft::WRL::ComPtr<IDXGIInfoQueue> dxgiInfoQueue;
			if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(dxgiInfoQueue.GetAddressOf()))))
			{
				dxgiFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;

				dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL,
												  DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, FALSE);
				dxgiInfoQueue->SetBreakOnSeverity(
					DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, TRUE);

				//NOTE No filters for now
				DXGI_INFO_QUEUE_FILTER filter = {};
				dxgiInfoQueue->AddStorageFilterEntries(DXGI_DEBUG_DXGI, &filter);

				gLogger->info("DXGI debug interface enabled and will break on errors");
			}
#endif
		}
	}

	static Microsoft::WRL::ComPtr<IDXGIFactory7> CreateDXGIFactory(DWORD dxgiFactoryFlags)
	{
		Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory;
		HRESULT hr = CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&dxgiFactory));

		if (FAILED(hr))
		{
			gLogger->critical("Failed to create DXGI Factory: {}", HRESULTToString(hr));
			assert(false && "Failed to create DXGI Factory");
		}
		else
		{
			gLogger->info("DXGI Factory created successfully");
		}

		// If it fails this will just be nothing
		return dxgiFactory;
	}

	static Microsoft::WRL::ComPtr<IDXGIAdapter4>
	EnumerateAndSelectAdapter(IDXGIFactory7* dxgiFactory)
	{
		Microsoft::WRL::ComPtr<IDXGIAdapter4> selectedAdapter;

		gLogger->info("Enumerating adapters(s)...");

		for (UINT adapterIndex = 0;; adapterIndex++)
		{
			Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter1;
			if (FAILED(dxgiFactory->EnumAdapters1(adapterIndex, &adapter1)))
				break;

			Microsoft::WRL::ComPtr<IDXGIAdapter4> adapter4;
			if (FAILED(adapter1.As(&adapter4)))
				continue;

			DXGI_ADAPTER_DESC3 desc;
			adapter4->GetDesc3(&desc);

			std::wstring wideDesc(desc.Description);
			std::string adapterName(wideDesc.begin(), wideDesc.end());

			// VRAM Memory sizes
			float vramGB = static_cast<float>(desc.DedicatedVideoMemory) /
						   (1024.0F * 1024.0F * 1024.0F);
			float sharedGB = static_cast<float>(desc.SharedSystemMemory) /
							 (1024.0F * 1024.0F * 1024.0F);

			gLogger->info("\tAdapter {}: {}", adapterIndex, adapterName);
			gLogger->info("\t\tDedicated VRAM: {:.2f} GB", vramGB);
			gLogger->info("\t\tShared Memory: {:.2f} GB", sharedGB);
			gLogger->info("\t\tDevice ID: 0x{:04X}", desc.DeviceId);
			gLogger->info("\t\tVendor ID: 0x{:04X}", desc.VendorId);

			if (desc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)
				gLogger->info("\t\tType: Software Adapter");
			else
				gLogger->info("\t\tType: Hardware Adapter");

			// In most cases the first adapter is what we want for our GPU if exist
			if (adapterIndex == 0)
			{
				selectedAdapter = adapter4;
				gLogger->info("\t\t** Using this adapter **");
			}
		}

		if (!selectedAdapter)
		{
			gLogger->critical("No adapter selected");
			assert(false && "No adapter available");
		}

		return selectedAdapter;
	}

	static void LogDeviceCapabilities(ID3D12Device14* device)
	{
		// Need to check certain feature sets as not all versions
		// of Windows will support the latest and greatest.
		D3D12_FEATURE_DATA_D3D12_OPTIONS options = {};
		if (SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &options,
												  sizeof(options))))
		{
			gLogger->info("Device capabilities:");
			gLogger->info("\tTiled Resources Tier: {}",
						  static_cast<int>(options.TiledResourcesTier));
			gLogger->info("\tResource Binding Tier: {}",
						  static_cast<int>(options.ResourceBindingTier));
			gLogger->info("\tConservative Rasterization Tier: {}",
						  static_cast<int>(options.ConservativeRasterizationTier));
			gLogger->info("\tResource Heap Tier: {}", static_cast<int>(options.ResourceHeapTier));
		}

		// Ray tracing check
		D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
		if (SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5,
												  sizeof(options5))))
		{
			if (options5.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED)
			{
				gLogger->info("\tRaytracing Tier: {}", static_cast<int>(options5.RaytracingTier));
			}
		}

		// Mesh Shader check
		D3D12_FEATURE_DATA_D3D12_OPTIONS7 options7 = {};
		if (SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &options7,
												  sizeof(options7))))
		{
			if (options7.MeshShaderTier != D3D12_MESH_SHADER_TIER_NOT_SUPPORTED)
			{
				gLogger->info("\tMesh Shader Tier: {}", static_cast<int>(options7.MeshShaderTier));
			}
		}
	}

	static bool CreateDeviceAndAllocator(IDXGIAdapter4* adapter)
	{
		if (gDevice != nullptr)
			return true;

		//BUG this should be dynamic versioning
		gLogger->info("Creating D3D12 device with feature level 12.2...");

		Microsoft::WRL::ComPtr<ID3D12Device14> pDevice;
		HRESULT hr = D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_2, IID_PPV_ARGS(&pDevice));

		if (FAILED(hr))
		{
			gLogger->critical("Failed to create device with feature level 12.2: {}",
							  HRESULTToString(hr));
			gLogger->critical("This application requires D3D12 Feature Level 12.2");
			return false;
		}

		gLogger->info("Device created successfully with feature level 12.2");

		// Set the device name for debugging
#if _DEBUG
		pDevice->SetName(L"Jar_D3D12Device");
#endif

		LogDeviceCapabilities(pDevice.Get());

		// Transfer the pointer
		gDevice = pDevice.Detach();

		D3D12MA::ALLOCATOR_DESC allocatorDesc = {};
		allocatorDesc.pDevice = gDevice;
		allocatorDesc.pAdapter = adapter;
		allocatorDesc.Flags = D3D12MA::ALLOCATOR_FLAG_NONE;

		hr = D3D12MA::CreateAllocator(&allocatorDesc, &gAllocator);
		if (SUCCEEDED(hr))
		{
			gLogger->info("D3D12MA memory allocator created successfully");
		}
		else
		{
			gLogger->error("Failed to create memory allocator: {}", HRESULTToString(hr));
		}

		return true;
	}

	static void InitializeDescriptorAllocators()
	{
		gDescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] =
			new DescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		gDescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER] =
			new DescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
		gDescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_RTV] =
			new DescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		gDescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_DSV] =
			new DescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

		gLogger->info("Descriptor allocators initialized");
	}

	static void InitializeCommandSystem()
	{
		gCommandListManager = new CommandListManager();
		gCommandListManager->Create(gDevice);
		gLogger->info("Command list manager initialized");

		gGraphicsContext = new GraphicsContext();
		gGraphicsContext->Create(gDevice);
		gLogger->info("Graphics context initialized");
	}

	static void SetupDebugInfoQueue()
	{
#if _DEBUG
		if (gDevice == nullptr)
			return;

		ID3D12InfoQueue1* pInfoQueue1 = nullptr;
		ID3D12InfoQueue* pInfoQueue = nullptr;

		// Try to get InfoQueue1 interface for callback support
		if (SUCCEEDED(gDevice->QueryInterface(IID_PPV_ARGS(&pInfoQueue1))))
		{
			// Better to have our errors presented to us by force aka messagebox
			auto messageCallback = [](D3D12_MESSAGE_CATEGORY category,
									  D3D12_MESSAGE_SEVERITY severity, D3D12_MESSAGE_ID id,
									  LPCSTR pDescription, void* pContext) -> void {
				// Only care about errors and corrupts
				if (severity == D3D12_MESSAGE_SEVERITY_ERROR ||
					severity == D3D12_MESSAGE_SEVERITY_CORRUPTION)
				{
					std::string title;
					switch (severity)
					{
					case D3D12_MESSAGE_SEVERITY_CORRUPTION:
						title = "D3D12 CORRUPTION";
						break;
					case D3D12_MESSAGE_SEVERITY_ERROR:
						title = "D3D12 ERROR";
						break;
					default:
						title = "D3D12 MESSAGE";
						break;
					}

					std::string message =
						std::format("Category: {}\n"
									"Message ID: {}\n\n"
									"Description:\n{}",
									static_cast<int>(category), static_cast<int>(id),
									pDescription ? pDescription : "No description available");

					UINT mbType = MB_OK | MB_ICONERROR | MB_TOPMOST;
					if (severity == D3D12_MESSAGE_SEVERITY_CORRUPTION)
					{
						mbType |= MB_ICONSTOP;
					}

					MessageBoxA(nullptr, message.c_str(), title.c_str(), mbType);

					gLogger->error("{} - {}", title, message);
				}
			};

			DWORD callbackCookie = 0;
			HRESULT hr = pInfoQueue1->RegisterMessageCallback(
				messageCallback, D3D12_MESSAGE_CALLBACK_FLAG_NONE, nullptr, &callbackCookie);

			if (SUCCEEDED(hr))
			{
				gLogger->info("MessageBox callback registered for errors and corruption");
			}
			else
			{
				gLogger->error("Failed to register message callback: {}", HRESULTToString(hr));
			}

			// Filters
			D3D12_MESSAGE_SEVERITY severities[] = {D3D12_MESSAGE_SEVERITY_INFO};

			// NOTE see https://learn.microsoft.com/en-us/windows/win32/api/d3d12sdklayers/ne-d3d12sdklayers-d3d12_message_id
			D3D12_MESSAGE_ID denyIds[] = {
				D3D12_MESSAGE_ID_INVALID_DESCRIPTOR_HANDLE,
				D3D12_MESSAGE_ID_CREATEGRAPHICSPIPELINESTATE_PS_OUTPUT_RT_OUTPUT_MISMATCH,
				D3D12_MESSAGE_ID_COMMAND_LIST_DESCRIPTOR_TABLE_NOT_SET,
				D3D12_MESSAGE_ID_RESOURCE_BARRIER_DUPLICATE_SUBRESOURCE_TRANSITIONS,
				D3D12_MESSAGE_ID_RESOLVE_QUERY_INVALID_QUERY_STATE,
				D3D12_MESSAGE_ID_CREATERESOURCE_STATE_IGNORED,
			};

			D3D12_INFO_QUEUE_FILTER newFilter = {};
			newFilter.DenyList.NumSeverities = _countof(severities);
			newFilter.DenyList.pSeverityList = severities;
			newFilter.DenyList.NumIDs = _countof(denyIds);
			newFilter.DenyList.pIDList = denyIds;

			pInfoQueue1->PushStorageFilter(&newFilter);
			pInfoQueue1->Release();

			gLogger->info("InfoQueue filters applied");
		}
		else if (SUCCEEDED(gDevice->QueryInterface(IID_PPV_ARGS(&pInfoQueue))))
		{
			gLogger->warn("InfoQueue1 not available, using InfoQueue");

			pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
			pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);

			D3D12_MESSAGE_SEVERITY severities[] = {D3D12_MESSAGE_SEVERITY_INFO};

			// Suppress individual messages by their ID
			D3D12_MESSAGE_ID denyIds[] = {
				D3D12_MESSAGE_ID_INVALID_DESCRIPTOR_HANDLE,
				D3D12_MESSAGE_ID_CREATEGRAPHICSPIPELINESTATE_PS_OUTPUT_RT_OUTPUT_MISMATCH,
				D3D12_MESSAGE_ID_COMMAND_LIST_DESCRIPTOR_TABLE_NOT_SET,
				D3D12_MESSAGE_ID_RESOURCE_BARRIER_DUPLICATE_SUBRESOURCE_TRANSITIONS,
				D3D12_MESSAGE_ID_RESOLVE_QUERY_INVALID_QUERY_STATE,
				D3D12_MESSAGE_ID_CREATERESOURCE_STATE_IGNORED,
			};

			D3D12_INFO_QUEUE_FILTER newFilter = {};
			newFilter.DenyList.NumSeverities = _countof(severities);
			newFilter.DenyList.pSeverityList = severities;
			newFilter.DenyList.NumIDs = _countof(denyIds);
			newFilter.DenyList.pIDList = denyIds;

			pInfoQueue->PushStorageFilter(&newFilter);
			pInfoQueue->Release();

			gLogger->info("InfoQueue filters applied no callbacks");
		}
#endif
	}

} // namespace Graphics

void Graphics::Init()
{
	InitLogger();

	// If we're in debug, enable these.
	DWORD dxgiFactoryFlags = 0;
	EnableDebugLayers(dxgiFactoryFlags);

	auto dxgiFactory = CreateDXGIFactory(dxgiFactoryFlags);
	if (!dxgiFactory)
		return;

	auto adapter = EnumerateAndSelectAdapter(dxgiFactory.Get());
	if (!adapter)
		return;

	if (!CreateDeviceAndAllocator(adapter.Get()))
		return;

	InitializeDescriptorAllocators();
	InitializeCommandSystem();

	SetupDebugInfoQueue();

	gLogger->info("Graphics system initialization complete");
}

void Graphics::Shutdown()
{
	if (gCommandListManager)
	{
		auto& queue = gCommandListManager->GetGraphicsQueue();
		uint64_t fence = queue.Signal();
		queue.WaitForFence(fence);
	}

	if (gGraphicsContext)
	{
		gGraphicsContext->Shutdown();
		delete gGraphicsContext;
		gGraphicsContext = nullptr;
	}

	if (gCommandListManager)
	{
		gCommandListManager->Shutdown();
		delete gCommandListManager;
		gCommandListManager = nullptr;
	}

	for (auto& i : gDescriptorAllocator)
	{
		delete i;
		i = nullptr;
	}
	DescriptorAllocator::DestroyAll();

	if (gAllocator != nullptr)
	{
		gAllocator->Release();
		gAllocator = nullptr;
		gLogger->info("D3D12MA memory allocator released");
	}

	if (gDevice != nullptr)
	{
		gDevice->Release();
		gDevice = nullptr;
		gLogger->info("Device released");
	}

	gLogger->info("Graphics system shutdown complete");
}
