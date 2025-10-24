#pragma once

#include "d3dcommon.h"
#include <wrl.h>
#include <d3d12.h>
#include <memory>
#include <spdlog/spdlog.h>
#include "DescriptorHeap.h"
#include <D3D12MemAlloc.h>

class CommandListManager;

namespace Graphics
{
	class GraphicsContext;
}

/// Global D3D12 graphics system state and initialization.
/// Contains:
/// - gDevice: the global device
/// - gAllocator: D3D12MemoryAllocator library
/// - gDescriptorAllocator: global heap allocator "manager" for
///   RTV,DSV, CBV_SRV_UAV, Sampler)
/// - gCommandListManager: global command list and queue manager
namespace Graphics
{
	using namespace Microsoft::WRL;

	extern ID3D12Device14* gDevice;
	extern D3D_FEATURE_LEVEL gD3DFeatureLevel;
	extern D3D12MA::Allocator* gAllocator;
	extern DescriptorAllocator* gDescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];

	extern CommandListManager* gCommandListManager;
	extern GraphicsContext* gGraphicsContext;

	/// Initization for the globals
	void Init();

	/// Cleans up the globals
	void Shutdown();

	/// Global logger mainly for the init D3D12 objects
	void InitLogger();
	extern std::shared_ptr<spdlog::logger> gLogger;

} // namespace Graphics
