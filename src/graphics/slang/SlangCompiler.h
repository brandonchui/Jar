#pragma once

#include "SlangTypes.h"
#include <filesystem>

#ifdef HAS_SLANG
#include <d3d12.h>
#endif

namespace SlangHelper
{

#ifdef HAS_SLANG

	/// Compile .slang file and extract various pipeline data from it.
	/// NOTE Using ID3D12Device* seems more portable? Renderer is using
	/// ID3D12Device14 so we'll see.
	CompiledShaderData CompileShaderForPSO(const std::filesystem::path& shaderPath,
										   ID3D12Device* device);

#endif // HAS_SLANG

} // namespace SlangHelper
