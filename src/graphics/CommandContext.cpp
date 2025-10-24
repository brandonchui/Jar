#include "CommandContext.h"
#include "GpuResource.h"
#include "slang/SlangCore.h"
#include "Core.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include <d3dx12/d3dx12.h>
#include <cassert>
#include <filesystem>

namespace Graphics
{
	std::shared_ptr<spdlog::logger> CommandContext::sLogger = nullptr;

	void CommandContext::InitLogger()
	{
		if (!sLogger)
		{
			sLogger = spdlog::get("CommandContext");
			if (!sLogger)
			{
				sLogger = spdlog::stdout_color_mt("CommandContext");
				sLogger->set_pattern("[%H:%M:%S] [%^%l%$] [%n] %v");
				sLogger->set_level(spdlog::level::debug);
			}
		}
	}
	void CommandContext::Create(ID3D12Device* pDevice, D3D12_COMMAND_LIST_TYPE type)
	{
		InitLogger();
		assert(pDevice != nullptr);
		mType = type;

		HRESULT hr = pDevice->CreateCommandAllocator(type, IID_PPV_ARGS(&mAllocator));
		assert(SUCCEEDED(hr) && "Failed to create command allocator");

		hr = pDevice->CreateCommandList(0, type, mAllocator.Get(), nullptr,
										IID_PPV_ARGS(&mCommandList));
		assert(SUCCEEDED(hr) && "Failed to create command list");

		mCommandList->Close();
	}

	void CommandContext::Shutdown()
	{
		mCommandList.Reset();
		mAllocator.Reset();
		mRootSignature.Reset();
		mPipelineState.Reset();
	}

	void CommandContext::Begin()
	{
		// Start fresh for every new frame
		HRESULT hr = mAllocator->Reset();
		assert(SUCCEEDED(hr) && "Failed to reset command allocator");

		hr = mCommandList->Reset(mAllocator.Get(), mPipelineState.Get());
		assert(SUCCEEDED(hr) && "Failed to reset command list");

		if (mRootSignature)
		{
			mCommandList->SetGraphicsRootSignature(mRootSignature.Get());
		}
	}

	void CommandContext::Flush(bool waitForCompletion)
	{
		// NOTE This is not a traditional flush like with iostreams so
		// maybe should work on the API call a bit with naming.
		HRESULT hr = mCommandList->Close();
		assert(SUCCEEDED(hr) && "Failed to close(FLUSH) command list");

		// The actual execution is handled by the caller
		// They will call queue.ExecuteCommandList(GetCommandList())
	}

	void CommandContext::TransitionResource(GpuResource& resource, D3D12_RESOURCE_STATES newState)
	{
		D3D12_RESOURCE_STATES oldState = resource.mUsageState;

		if (oldState != newState)
		{
			CD3DX12_RESOURCE_BARRIER barrier =
				CD3DX12_RESOURCE_BARRIER::Transition(resource.GetResource(), oldState, newState);
			mCommandList->ResourceBarrier(1, &barrier);

			resource.mUsageState = newState;
		}
	}

	void CommandContext::SetRootDirectory()
	{
		// NOTE Maybe we need to have a way to set a custom directory for
		// shader file look ups?
	}

	void CommandContext::SetShader(const std::string& shaderName)
	{
		// We are grabbing the shaders,pso, root sigs entirely from the
		// Slang reflection API.
		InitLogger();
		std::filesystem::path shaderPath = "shaders/" + shaderName + ".slang";

		if (!std::filesystem::exists(shaderPath))
		{
			sLogger->error("Shader file not found: {}", shaderPath.string());
			assert(false && "Shader file not found");
			return;
		}

		sLogger->info("Setting shader: {}", shaderName);

		if (!Graphics::gDevice)
		{
			sLogger->error("Graphics::gDevice null");
			return;
		}

		sLogger->debug("About to call CompileShaderForPSO");
		sLogger->debug("\tshaderPath: {}", shaderPath.string());
		sLogger->debug("\tDevice pointer: {}", static_cast<const void*>(Graphics::gDevice));

		sLogger->debug("Calling CompileShaderForPSO...");
		SlangHelper::CompiledShaderData shaderData =
			SlangHelper::CompileShaderForPSO(shaderPath, Graphics::gDevice);

		// We should be getting an address and a reasonable bytecode size if it works.
		sLogger->debug("Shader compilation results:");
		sLogger->debug("\tShaderData address: {}", static_cast<const void*>(&shaderData));
		sLogger->debug("\tVertex bytecode size: {} bytes", shaderData.vertexBytecode.size());
		sLogger->debug("\tFragment bytecode size: {} bytes", shaderData.fragBytecode.size());
		sLogger->debug("\tRoot signature: {}", shaderData.rootSignature ? "YES" : "NO");

		if (!shaderData.vertexBytecode.empty())
		{
			sLogger->debug("\tVertex bytecode is valid: {} bytes at {}",
						   shaderData.vertexBytecode.size(),
						   static_cast<const void*>(shaderData.vertexBytecode.data()));
		}
		else
		{
			sLogger->error("\tNo vertex bytecode");
		}

		if (!shaderData.fragBytecode.empty())
		{
			sLogger->debug("\tPixel bytecode is valid: {} bytes at {}",
						   shaderData.fragBytecode.size(),
						   static_cast<const void*>(shaderData.fragBytecode.data()));
		}
		else
		{
			sLogger->error("\tNo pixel bytecode");
		}

		// Creating the PSO
		if (shaderData.rootSignature != nullptr)
		{
			mRootSignature.Attach(shaderData.rootSignature);

			sLogger->debug("Creating PSO...");
			ID3D12PipelineState* pso = SlangHelper::CreatePSOWithSlangShader(
				shaderData, Graphics::gDevice, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_D32_FLOAT);

			if (pso != nullptr)
			{
				mPipelineState.Attach(pso);
				sLogger->info("PSO created and attached");
			}
			else
			{
				sLogger->error("Failed to create PSO");
			}
		}
		else
		{
			sLogger->error("No root signature generated");
		}
	}

	void GraphicsContext::SetRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE rtv)
	{
		mCommandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
	}

	void GraphicsContext::SetRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE rtv,
										  D3D12_CPU_DESCRIPTOR_HANDLE dsv)
	{
		mCommandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
	}

	void GraphicsContext::ClearColor(D3D12_CPU_DESCRIPTOR_HANDLE rtv, const float color[4])
	{
		mCommandList->ClearRenderTargetView(rtv, color, 0, nullptr);
	}

	void GraphicsContext::ClearColor(D3D12_CPU_DESCRIPTOR_HANDLE rtv)
	{
		float color[4] = {0.1F, 0.1F, 0.1F, 1.0F};
		mCommandList->ClearRenderTargetView(rtv, color, 0, nullptr);
	}

	void GraphicsContext::ClearColor(ID3D12Resource* pTarget)
	{
		// NOTE deleted this old logic
	}

	void GraphicsContext::ClearColor(ID3D12Resource* pTarget, D3D12_CPU_DESCRIPTOR_HANDLE rtv)
	{
		float color[4] = {0.5, 0.5, 0.5, 1.0};

		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			pTarget, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		mCommandList->ResourceBarrier(1, &barrier);

		mCommandList->ClearRenderTargetView(rtv, color, 0, nullptr);

		barrier = CD3DX12_RESOURCE_BARRIER::Transition(pTarget, D3D12_RESOURCE_STATE_RENDER_TARGET,
													   D3D12_RESOURCE_STATE_PRESENT);
		mCommandList->ResourceBarrier(1, &barrier);
	}

	void GraphicsContext::ClearDepth(D3D12_CPU_DESCRIPTOR_HANDLE dsv, float depth)
	{
		mCommandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, depth, 0, 0, nullptr);
	}

	void GraphicsContext::ClearStencil(D3D12_CPU_DESCRIPTOR_HANDLE dsv, uint8_t stencil)
	{
		mCommandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_STENCIL, 0.0F, stencil, 0,
											nullptr);
	}

	void GraphicsContext::ClearDepthAndStencil(D3D12_CPU_DESCRIPTOR_HANDLE dsv, float depth,
											   uint8_t stencil)
	{
		mCommandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
											depth, stencil, 0, nullptr);
	}

	void GraphicsContext::SetViewport(const D3D12_VIEWPORT& viewport)
	{
		mCommandList->RSSetViewports(1, &viewport);
	}

	void GraphicsContext::SetViewport(float x, float y, float w, float h, float minDepth,
									  float maxDepth)
	{
		D3D12_VIEWPORT viewport = {x, y, w, h, minDepth, maxDepth};
		mCommandList->RSSetViewports(1, &viewport);
	}

	void GraphicsContext::SetScissorRect(const D3D12_RECT& scissor)
	{
		mCommandList->RSSetScissorRects(1, &scissor);
	}

	void GraphicsContext::SetScissorRect(uint32_t left, uint32_t top, uint32_t right,
										 uint32_t bottom)
	{
		D3D12_RECT scissor = {static_cast<LONG>(left), static_cast<LONG>(top),
							  static_cast<LONG>(right), static_cast<LONG>(bottom)};
		mCommandList->RSSetScissorRects(1, &scissor);
	}

	void GraphicsContext::SetConstantBuffer(uint32_t rootIndex,
											D3D12_GPU_VIRTUAL_ADDRESS gpuAddress)
	{
		mCommandList->SetGraphicsRootConstantBufferView(rootIndex, gpuAddress);
	}

	void GraphicsContext::SetConstants(uint32_t rootIndex, uint32_t numConstants,
									   const void* pConstants)
	{
		mCommandList->SetGraphicsRoot32BitConstants(rootIndex, numConstants, pConstants, 0);
	}

	void GraphicsContext::SetConstant(uint32_t rootIndex, uint32_t value, uint32_t offset)
	{
		mCommandList->SetGraphicsRoot32BitConstant(rootIndex, value, offset);
	}

	void GraphicsContext::SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY topology)
	{
		mCommandList->IASetPrimitiveTopology(topology);
	}

	void GraphicsContext::SetVertexBuffer(const D3D12_VERTEX_BUFFER_VIEW& vbv, uint32_t slot)
	{
		mCommandList->IASetVertexBuffers(slot, 1, &vbv);
	}

	void GraphicsContext::SetVertexBuffers(const D3D12_VERTEX_BUFFER_VIEW* pViews, uint32_t count,
										   uint32_t startSlot)
	{
		mCommandList->IASetVertexBuffers(startSlot, count, pViews);
	}

	void GraphicsContext::SetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW& ibv)
	{
		mCommandList->IASetIndexBuffer(&ibv);
	}

	void GraphicsContext::DrawInstanced(uint32_t vertexCount, uint32_t instanceCount,
										uint32_t startVertex, uint32_t startInstance)
	{
		mCommandList->DrawInstanced(vertexCount, instanceCount, startVertex, startInstance);
	}

	void GraphicsContext::DrawIndexedInstanced(uint32_t indexCount, uint32_t instanceCount,
											   uint32_t startIndex, int32_t baseVertex,
											   uint32_t startInstance)
	{
		mCommandList->DrawIndexedInstanced(indexCount, instanceCount, startIndex, baseVertex,
										   startInstance);
	}

} // namespace Graphics
