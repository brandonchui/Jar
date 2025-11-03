#pragma once

#include <vector>
#include <string>

#ifdef HAS_SLANG
#include <slang.h>
#include <d3d12.h>
#endif

namespace SlangHelper
{

#ifdef HAS_SLANG

	/// Extract input layout from vertex shader reflection like the TEXTURE,
	/// POSITION ,etc.
	std::vector<D3D12_INPUT_ELEMENT_DESC>
	ExtractInputLayoutFromReflection(slang::EntryPointReflection* entryPoint, std::vector<std::string>& outSemanticNames);

#endif // HAS_SLANG

} // namespace SlangHelper
