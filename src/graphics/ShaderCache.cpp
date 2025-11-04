#include "ShaderCache.h"
#include <functional>

namespace Graphics
{
	bool ShaderCache::Has(uint64_t key) const
	{
		return mCache.contains(key);
	}

	ShaderCache::CachedShader* ShaderCache::Get(uint64_t key)
	{
		auto it = mCache.find(key);
		if (it != mCache.end())
		{
			return &it->second;
		}
		return nullptr;
	}

	void ShaderCache::Store(uint64_t key, ID3D12RootSignature* rootSig, ID3D12PipelineState* pso)
	{
		CachedShader shader;
		shader.rootSignature = rootSig;
		shader.pipelineState = pso;
		mCache[key] = shader;
	}

	uint64_t ShaderCache::ComputeMRTKey(const std::string& shaderName, const DXGI_FORMAT* rtFormats,
										uint32_t numRenderTargets, DXGI_FORMAT depthFormat)
	{
		uint64_t hash = std::hash<std::string>{}(shaderName);

		hash ^= (static_cast<uint64_t>(numRenderTargets) << 32);

		// Implying we have max 8 render targets per gbuffer
		for (uint32_t i = 0; i < numRenderTargets && i < 8; ++i)
		{
			hash ^= (static_cast<uint64_t>(rtFormats[i]) << (i * 8));
		}

		hash ^= (static_cast<uint64_t>(depthFormat) << 56);

		return hash;
	}

	uint64_t ShaderCache::ComputeKey(const std::string& shaderName, DXGI_FORMAT rtFormat,
									 DXGI_FORMAT depthFormat)
	{
		uint64_t hash = std::hash<std::string>{}(shaderName);

		hash ^= (static_cast<uint64_t>(rtFormat) << 32);

		hash ^= (static_cast<uint64_t>(depthFormat) << 48);

		return hash;
	}

	void ShaderCache::Clear()
	{
		mCache.clear();
	}
} // namespace Graphics
