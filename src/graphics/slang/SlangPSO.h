#pragma once

#include "SlangTypes.h"

#ifdef HAS_SLANG
#include <d3d12.h>
#include <dxgiformat.h>
#endif

namespace SlangHelper
{

#ifdef HAS_SLANG

	/// Creates the PSO from the CompiledShaderData (SlangHelper files). Bare
	/// bones right now but it works!
	ID3D12PipelineState*
	CreatePSOWithSlangShader(const CompiledShaderData& shaderData, ID3D12Device* device,
							 DXGI_FORMAT renderTargetFormat = DXGI_FORMAT_R8G8B8A8_UNORM,
							 DXGI_FORMAT depthStencilFormat = DXGI_FORMAT_D32_FLOAT);

	/// Creates the PSO when you have multiple render targets
	ID3D12PipelineState*
	CreatePSOWithSlangShaderMRT(const CompiledShaderData& shaderData, ID3D12Device* device,
								const DXGI_FORMAT* renderTargetFormats, uint32_t numRenderTargets,
								DXGI_FORMAT depthStencilFormat = DXGI_FORMAT_D32_FLOAT);

#endif // HAS_SLANG

} // namespace SlangHelper
