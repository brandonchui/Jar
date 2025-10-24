#include "SlangUtilities.h"

#ifdef HAS_SLANG
#include <spdlog/sinks/stdout_color_sinks.h>
#endif

namespace SlangHelper
{

#ifdef HAS_SLANG

	std::shared_ptr<spdlog::logger>& GetLogger()
	{
		static std::shared_ptr<spdlog::logger> logger = []() {
			auto log = spdlog::stdout_color_mt("SlangHelper");
			log->set_level(spdlog::level::debug);
			log->set_pattern("[%H:%M:%S] [%^%l%$] [%n] %v");
			return log;
		}();
		return logger;
	}

	const char* GetBindingTypeName(slang::BindingType type)
	{
		switch (type)
		{
		case slang::BindingType::Unknown:
			return "Unknown";
		case slang::BindingType::Sampler:
			return "Sampler";
		case slang::BindingType::Texture:
			return "Texture";
		case slang::BindingType::ConstantBuffer:
			return "ConstantBuffer";
		case slang::BindingType::ParameterBlock:
			return "ParameterBlock";
		case slang::BindingType::TypedBuffer:
			return "TypedBuffer";
		case slang::BindingType::RawBuffer:
			return "RawBuffer";
		case slang::BindingType::CombinedTextureSampler:
			return "CombinedTextureSampler";
		case slang::BindingType::InputRenderTarget:
			return "InputRenderTarget";
		case slang::BindingType::InlineUniformData:
			return "InlineUniformData";
		case slang::BindingType::RayTracingAccelerationStructure:
			return "RayTracingAccelerationStructure";
		case slang::BindingType::VaryingInput:
			return "VaryingInput";
		case slang::BindingType::VaryingOutput:
			return "VaryingOutput";
		case slang::BindingType::ExistentialValue:
			return "ExistentialValue";
		case slang::BindingType::PushConstant:
			return "PushConstant";
		case slang::BindingType::MutableFlag:
			return "MutableFlag";
		case slang::BindingType::MutableTexture:
			return "MutableTexture";
		case slang::BindingType::MutableTypedBuffer:
			return "MutableTypedBuffer";
		case slang::BindingType::MutableRawBuffer:
			return "MutableRawBuffer";
		case slang::BindingType::BaseMask:
			return "BaseMask";
		case slang::BindingType::ExtMask:
			return "ExtMask";
		default:
			return "UnknownType";
		}
	}

	const char* GetStageName(SlangStage stage)
	{
		switch (stage)
		{
		case SLANG_STAGE_VERTEX:
			return "Vertex";
		case SLANG_STAGE_FRAGMENT:
			return "Pixel/Fragment";
		case SLANG_STAGE_COMPUTE:
			return "Compute";
		case SLANG_STAGE_GEOMETRY:
			return "Geometry";
		case SLANG_STAGE_HULL:
			return "Hull";
		case SLANG_STAGE_DOMAIN:
			return "Domain";
		case SLANG_STAGE_RAY_GENERATION:
			return "RayGeneration";
		case SLANG_STAGE_INTERSECTION:
			return "Intersection";
		case SLANG_STAGE_ANY_HIT:
			return "AnyHit";
		case SLANG_STAGE_CLOSEST_HIT:
			return "ClosestHit";
		case SLANG_STAGE_MISS:
			return "Miss";
		case SLANG_STAGE_CALLABLE:
			return "Callable";
		default:
			return "Unknown";
		}
	}

#endif // HAS_SLANG

} // namespace SlangHelper
