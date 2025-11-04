#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <string>
#include <memory>
#include <spdlog/spdlog.h>
#include "GpuResource.h"

namespace Graphics
{
	/// Encapsulates command list recording with its own allocator.
	/// Each context owns and manages a single command list/allocator pair.
	class CommandContext
	{
	public:
		CommandContext() = default;
		~CommandContext() = default;

		/// Init the owned allocator and lists
		void Create(ID3D12Device* pDevice,
					D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT);
		void Shutdown();

		/// Resets the allocator and list to prepare for a new frame.
		/// Will also set the graphics root signature if one exists.
		void Begin();

		/// Ideally, we should set the root directory since every machine differs
		/// might even originate from the /build/ folder. Need to add some
		/// implemention details around this.
		/// TODO implement root directory setting
		void SetRootDirectory();

		/// Closes the command list, will expand on this.
		/// NOTE trivial implemenation, need to add more here
		void Flush(bool waitForCompletion = true);

		/// Extracts the .slang file and creats root signature, PSO, vertex, pixel
		/// bytecode.
		void SetShader(const std::string& shaderName);

		/// Multi render target version of SetShader(..)
		void SetShaderMRT(const std::string& shaderName, const DXGI_FORMAT* rtFormats,
						  uint32_t numRenderTargets,
						  DXGI_FORMAT depthStencilFormat = DXGI_FORMAT_D32_FLOAT);

		/// Transitions state for the resource given in the parameter.
		void TransitionResource(GpuResource& resource, D3D12_RESOURCE_STATES newState);

		ID3D12GraphicsCommandList* GetCommandList() const { return mCommandList.Get(); }
		ID3D12CommandAllocator* GetAllocator() const { return mAllocator.Get(); }
		D3D12_COMMAND_LIST_TYPE GetType() const { return mType; }
		ID3D12RootSignature* GetRootSignature() const { return mRootSignature.Get(); }
		ID3D12PipelineState* GetPipelineState() const { return mPipelineState.Get(); }

	protected:
		D3D12_COMMAND_LIST_TYPE mType = D3D12_COMMAND_LIST_TYPE_DIRECT;

		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList;
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mAllocator;

		/// PSO and Root signatures and built entirely through the .slang files.
		Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> mPipelineState;

		// TODO need to add way more Pix events
		// void PIXBeginEvent(const wchar_t* label);
		//   void PIXEndEvent(void);

		// TODO: BARRIER TRACKING

		// TODO: DESCRIPTOR HEAP

	private:
		void InitLogger();
		static std::shared_ptr<spdlog::logger> sLogger;
	};

	/// Graphics specific command context for rendering operations.
	/// Adds support for binding buffers, and draw commands in lists.
	/// Many of the methods here are wrappers around the actual command
	/// list methods, but just structured neatly in the GraphicsContext
	/// class.
	class GraphicsContext : public CommandContext
	{
	public:
		GraphicsContext() = default;
		~GraphicsContext() = default;

		/// Render target commands
		void SetRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE rtv);
		void SetRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE rtv, D3D12_CPU_DESCRIPTOR_HANDLE dsv);

		/// Clear commands
		void ClearColor(D3D12_CPU_DESCRIPTOR_HANDLE rtv, const float color[4]);
		void ClearColor(D3D12_CPU_DESCRIPTOR_HANDLE rtv);
		void ClearColor(ID3D12Resource* pTarget);
		void ClearColor(ID3D12Resource* pTarget, D3D12_CPU_DESCRIPTOR_HANDLE rtv);

		void ClearDepth(D3D12_CPU_DESCRIPTOR_HANDLE dsv, float depth = 1.0F);
		void ClearStencil(D3D12_CPU_DESCRIPTOR_HANDLE dsv, uint8_t stencil = 0);
		void ClearDepthAndStencil(D3D12_CPU_DESCRIPTOR_HANDLE dsv, float depth = 1.0F,
								  uint8_t stencil = 0);

		/// Viewport and scissor commands
		void SetViewport(const D3D12_VIEWPORT& viewport);
		void SetViewport(float x, float y, float w, float h, float minDepth = 0.0F,
						 float maxDepth = 1.0F);

		void SetScissorRect(const D3D12_RECT& scissor);
		void SetScissorRect(uint32_t left, uint32_t top, uint32_t right, uint32_t bottom);

		/// Root sig binding
		void SetConstantBuffer(uint32_t rootIndex, D3D12_GPU_VIRTUAL_ADDRESS gpuAddress);
		void SetConstants(uint32_t rootIndex, uint32_t numConstants, const void* pConstants);
		void SetConstant(uint32_t rootIndex, uint32_t value, uint32_t offset = 0);

		///IA
		void SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY topology);
		void SetVertexBuffer(const D3D12_VERTEX_BUFFER_VIEW& vbv, uint32_t slot = 0);
		void SetVertexBuffers(const D3D12_VERTEX_BUFFER_VIEW* pViews, uint32_t count,
							  uint32_t startSlot = 0);
		void SetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW& ibv);

		/// Draw commands
		void DrawInstanced(uint32_t vertexCount, uint32_t instanceCount = 1,
						   uint32_t startVertex = 0, uint32_t startInstance = 0);
		void DrawIndexedInstanced(uint32_t indexCount, uint32_t instanceCount = 1,
								  uint32_t startIndex = 0, int32_t baseVertex = 0,
								  uint32_t startInstance = 0);
	};
} // namespace Graphics
