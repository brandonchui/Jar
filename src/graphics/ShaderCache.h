#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <string>
#include <unordered_map>
#include <cstdint>

namespace Graphics
{
	/// Stores compiled shader PSOs and root signatures to avoid recompilation
	class ShaderCache
	{
	public:
		struct CachedShader
		{
			Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
			Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
		};

		bool Has(uint64_t key) const;

		/// Get cached shader, will return nullptr if not found
		CachedShader* Get(uint64_t key);

		void Store(uint64_t key, ID3D12RootSignature* rootSig, ID3D12PipelineState* pso);

		/// Build hash key for MRT shaders based on the name,format, targets etc.
		/// It will generate a uint64_t from it and will use that as the key.
		static uint64_t ComputeMRTKey(const std::string& shaderName, const DXGI_FORMAT* rtFormats,
									  uint32_t numRenderTargets, DXGI_FORMAT depthFormat);

		/// Build hash key for non MRT shaders based on the name,format, targets etc.
		/// It will generate a uint64_t from it and will use that as the key.
		static uint64_t ComputeKey(const std::string& shaderName, DXGI_FORMAT rtFormat,
								   DXGI_FORMAT depthFormat);

		void Clear();

	private:
		std::unordered_map<uint64_t, CachedShader> mCache;
	};
} // namespace Graphics
