#pragma once

#include <memory>

#ifdef HAS_SLANG
#include <slang.h>
#include <spdlog/spdlog.h>
#endif

namespace SlangHelper
{

#ifdef HAS_SLANG

	std::shared_ptr<spdlog::logger>& GetLogger();

	/// Converts slang type enums to a string, mainly like
	/// ParameterBlock, Sampler, Texture.
	/// NOTE: blas, tlas?
	const char* GetBindingTypeName(slang::BindingType type);

	/// Returns shader stage names in the .slang file, mainly will be
	/// Vertex, Fragment, Compute, Mesh, Ray_Generation.
	const char* GetStageName(SlangStage stage);

#endif // HAS_SLANG

} // namespace SlangHelper
