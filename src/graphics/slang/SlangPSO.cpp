#include "SlangPSO.h"
#include "SlangUtilities.h"

namespace SlangHelper
{

#ifdef HAS_SLANG

	ID3D12PipelineState* CreatePSOWithSlangShader(const CompiledShaderData& shaderData,
												  ID3D12Device* device,
												  DXGI_FORMAT renderTargetFormat,
												  DXGI_FORMAT depthStencilFormat)
	{
		GetLogger()->info("Calling CreatePSOWithSlangShader()");
		GetLogger()->info("\tFunction received CompiledShaderData at address: {}",
						  (void*)&shaderData);

		// Checking validity of our input data before proceeding.
		GetLogger()->info("\tDevice: {}", device ? "Valid" : "NULL");
		GetLogger()->info("\tRoot signature: {}", shaderData.rootSignature ? "Valid" : "NULL");

		GetLogger()->info("\tChecking bytecode:");
		GetLogger()->info("\tVertex bytecode size: {} bytes", shaderData.vertexBytecode.size());
		GetLogger()->info("\tVertex bytecode data ptr: {}",
						  (void*)shaderData.vertexBytecode.data());
		GetLogger()->info("\tPixel bytecode size: {} bytes", shaderData.fragBytecode.size());
		GetLogger()->info("\tPixel bytecode data ptr: {}", (void*)shaderData.fragBytecode.data());

		if (!device || !shaderData.rootSignature)
		{
			GetLogger()->info("CreatePSOWithSlangShader: Invalid parameters");
			return nullptr;
		}

		if (shaderData.vertexBytecode.empty())
		{
			GetLogger()->error("CompiledPSOWithSlangShader Error: No vertex bytecode!");
			return nullptr;
		}

		if (shaderData.fragBytecode.empty())
		{
			GetLogger()->error("CompiledPSOWithSlangShader Error: No fragment bytecode!");
			return nullptr;
		}

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};

		GetLogger()->info("\nSetting shader bytecode:");

		// Vertex
		psoDesc.VS.pShaderBytecode = shaderData.vertexBytecode.data();
		psoDesc.VS.BytecodeLength = shaderData.vertexBytecode.size();
		GetLogger()->info("\tVertex shader set: {} bytes", shaderData.vertexBytecode.size());

		// Frag
		psoDesc.PS.pShaderBytecode = shaderData.fragBytecode.data();
		psoDesc.PS.BytecodeLength = shaderData.fragBytecode.size();
		GetLogger()->info("\tFrag shader set: {} bytes", shaderData.fragBytecode.size());

		// Set root signature
		GetLogger()->info("Setting root sig:");
		psoDesc.pRootSignature = shaderData.rootSignature;

		// Set input layout
		GetLogger()->info("Setting input layout:");
		GetLogger()->info("\tInput elements in shaderData: {}", shaderData.inputLayout.size());

		if (!shaderData.inputLayout.empty())
		{
			// Debug
			for (size_t i = 0; i < shaderData.inputLayout.size(); i++)
			{
				const auto& elem = shaderData.inputLayout[i];
				GetLogger()->info(
					"    [{}] Semantic: {} Index: {} Format: {} Slot: {} Offset: {} bytes", i,
					elem.SemanticName ? elem.SemanticName : "NULL", elem.SemanticIndex,
					static_cast<uint32_t>(elem.Format), elem.InputSlot, elem.AlignedByteOffset);
			}

			psoDesc.InputLayout.pInputElementDescs = shaderData.inputLayout.data();
			psoDesc.InputLayout.NumElements = static_cast<UINT>(shaderData.inputLayout.size());
			GetLogger()->info("\tInput layout set with {} elements", shaderData.inputLayout.size());
		}
		else
		{
			GetLogger()->warn("\tNo input layout elements found!");
		}

		// Default rasterizer state
		GetLogger()->info("Setting the default rasterizer state.");
		psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
		psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
		psoDesc.RasterizerState.FrontCounterClockwise = TRUE; // OBJ files typically use CCW winding
		psoDesc.RasterizerState.DepthBias = 0;
		psoDesc.RasterizerState.DepthBiasClamp = 0.0F;
		psoDesc.RasterizerState.SlopeScaledDepthBias = 0.0F;
		psoDesc.RasterizerState.DepthClipEnable = TRUE;
		psoDesc.RasterizerState.MultisampleEnable = FALSE;
		psoDesc.RasterizerState.AntialiasedLineEnable = FALSE;
		psoDesc.RasterizerState.ForcedSampleCount = 0;
		psoDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

		// Default blend state
		GetLogger()->info("Setting the default blend state.");
		psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
		psoDesc.BlendState.IndependentBlendEnable = FALSE;
		psoDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;
		psoDesc.BlendState.RenderTarget[0].LogicOpEnable = FALSE;
		psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
		psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
		psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
		psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
		psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		psoDesc.BlendState.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
		psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

		GetLogger()->info("Setting the default depth/stencil state.");
		if (depthStencilFormat != DXGI_FORMAT_UNKNOWN)
		{
			// We found a valid depth provided from the paramter.
			psoDesc.DepthStencilState.DepthEnable = TRUE;
			psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
			psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
			psoDesc.DepthStencilState.StencilEnable = FALSE;
			psoDesc.DSVFormat = depthStencilFormat;
		}
		else
		{
			psoDesc.DepthStencilState.DepthEnable = FALSE;
			psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
			psoDesc.DepthStencilState.StencilEnable = FALSE;
			psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
		}

		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = renderTargetFormat;
		psoDesc.SampleDesc.Count = 1;
		psoDesc.SampleDesc.Quality = 0;

		// Creating the PSO finally.
		ID3D12PipelineState* pso = nullptr;
		HRESULT hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso));

		if (FAILED(hr))
		{
			GetLogger()->error("Failed to create PSO: 0x{:X}", hr);
			return nullptr;
		}

		GetLogger()->info("PSO created successfully");
		return pso;
	}

#endif // HAS_SLANG

} // namespace SlangHelper
