#pragma once

#include <vector>
#include <cstdint>

#ifdef HAS_SLANG
#include <slang.h>
#include <d3d12.h>
#endif

namespace SlangHelper
{

#ifdef HAS_SLANG

	/// The main data structure to contain all shader data needed for the
	/// renderer to function from the .slang shaders. The slang helpers
	/// will generate the vertex/fragment bytecode and generates PSO and
	/// root sigs based on the reflection data.
	struct CompiledShaderData
	{
		// NOTE Ran into issues with using ComPtr here, so just using vectors for now.
		std::vector<uint8_t> vertexBytecode;
		std::vector<uint8_t> fragBytecode;
		std::vector<uint8_t> computeBytecode;

		ID3D12RootSignature* rootSignature = nullptr;
		std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout;

		/// We need this function to deal with the slang compilation session
		/// lifetime issues. Slang uses sessions, basically a cache within a
		/// scope. We need to capture the generated blob before it gets destroyed.
		void CopyBlobToVector(slang::IBlob* blob, std::vector<uint8_t>& target)
		{
			if (blob)
			{
				const auto* data = static_cast<const uint8_t*>(blob->getBufferPointer());
				size_t size = blob->getBufferSize();
				if (data && size > 0)
				{
					target.assign(data, data + size);
				}
			}
		}
	};

#endif // HAS_SLANG

} // namespace SlangHelper
