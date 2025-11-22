#include "SlangCompiler.h"
#include "SlangUtilities.h"
#include "SlangRootSignatureBuilder.h"
#include "SlangInputLayout.h"

#ifdef HAS_SLANG
#include <array>
#include <slang-com-ptr.h>
#include <wrl/client.h>
#include <fstream>
#include <sstream>
#endif

namespace SlangHelper
{

#ifdef HAS_SLANG

	CompiledShaderData CompileShaderForPSO(const std::filesystem::path& shaderPath,
										   ID3D12Device* device)
	{
		GetLogger()->info("SlangHelper::CompileShaderForPSO() - {}", shaderPath.string());
		CompiledShaderData result = {};

		if (!std::filesystem::exists(shaderPath))
		{
			GetLogger()->error("\tShader file not found: {}", shaderPath.string());
			return result;
		}

		// FIX sstreams are slow, need to change.
		std::ifstream file(shaderPath);
		std::stringstream buffer;
		buffer << file.rdbuf();
		std::string source = buffer.str();

		// Initialize slang compiler sessions.
		// See: https://docs.shader-slang.org/en/latest/compilation-api.html
		Slang::ComPtr<slang::IGlobalSession> globalSession;
		if (SLANG_FAILED(slang::createGlobalSession(globalSession.writeRef())))
		{
			GetLogger()->error("\tFailed to create Slang global session");
			return result;
		}

		// Slang sessions are just scoped cache.
		slang::SessionDesc sessionDesc = {};

		// Add search paths for shader includes.
		// NOTE Probably will need to update more search paths:
		//     - /utils/
		//     - /ibl/
		std::filesystem::path shaderDir = shaderPath.parent_path();
		std::filesystem::path commonDir = shaderDir / "common";
		std::string shaderDirStr = shaderDir.string();
		std::string commonDirStr = commonDir.string();
		std::array<const char*, 2> searchPaths = {{shaderDirStr.c_str(), commonDirStr.c_str()}};
		sessionDesc.searchPaths = searchPaths.data();
		sessionDesc.searchPathCount = 2;

		slang::TargetDesc targetDesc = {};
		targetDesc.format = SLANG_DXIL;
		targetDesc.profile = globalSession->findProfile("sm_6_8");
		targetDesc.floatingPointMode = SLANG_FLOATING_POINT_MODE_FAST;
		sessionDesc.targets = &targetDesc;
		sessionDesc.targetCount = 1;
		sessionDesc.defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR;

		// Compiling options, mainly just setting it to high.
#ifdef ENABLE_BINDLESS
		std::array<slang::CompilerOptionEntry, 2> optionEntries;
#else
		std::array<slang::CompilerOptionEntry, 1> optionEntries;
#endif
		optionEntries[0].name = slang::CompilerOptionName::Optimization;
		optionEntries[0].value.kind = slang::CompilerOptionValueKind::Int;
		optionEntries[0].value.intValue0 = SLANG_OPTIMIZATION_LEVEL_HIGH;

#ifdef ENABLE_BINDLESS
		// Pass ENABLE_BINDLESS macro to shader compiler
		optionEntries[1].name = slang::CompilerOptionName::MacroDefine;
		optionEntries[1].value.kind = slang::CompilerOptionValueKind::String;
		optionEntries[1].value.stringValue0 = "ENABLE_BINDLESS";
		optionEntries[1].value.stringValue1 = "1";

		sessionDesc.compilerOptionEntries = optionEntries.data();
		sessionDesc.compilerOptionEntryCount = 2;
#else
		sessionDesc.compilerOptionEntries = optionEntries.data();
		sessionDesc.compilerOptionEntryCount = 1;
#endif

		Slang::ComPtr<slang::ISession> session;
		if (SLANG_FAILED(globalSession->createSession(sessionDesc, session.writeRef())))
		{
			GetLogger()->error("\tFailed to create compilation session");
			return result;
		}

		// Modules essentially just load the shader for the session to use. Mainly
		// because we have the source in memory from the sstream, we just call the
		// loadModuleFromSourceString instead of the loadModule.
		Slang::ComPtr<slang::IBlob> diagnosticsBlob;
		Slang::ComPtr<slang::IModule> module;

		module = session->loadModuleFromSourceString(shaderPath.stem().string().c_str(),
													 shaderPath.string().c_str(), source.c_str(),
													 diagnosticsBlob.writeRef());

		if (!module)
		{
			GetLogger()->warn("\tFailed to compile module");
			if (diagnosticsBlob && diagnosticsBlob->getBufferSize() > 0)
			{
				GetLogger()->error("  Module Error: {}",
								   static_cast<const char*>(diagnosticsBlob->getBufferPointer()));
			}
			return result;
		}
		GetLogger()->info("\tModule compiled");

		// Query entry points
		// See: https://docs.shader-slang.org/en/latest/compilation-api.html#query-entry-points
		Slang::ComPtr<slang::IEntryPoint> vertexEntryPoint;
		Slang::ComPtr<slang::IEntryPoint> fragEntryPoint;
		Slang::ComPtr<slang::IEntryPoint> computeEntryPoint;

		Slang::ComPtr<slang::IBlob> diagnostics;

		// Technically in the .slang we don't need the attirbute shader["fragment"],
		// since we use findAndCheckEntryPoint.
		// If we are using findEntryPointByName then we might have to apparently.
		module->findAndCheckEntryPoint("vertexMain", SLANG_STAGE_VERTEX,
									   vertexEntryPoint.writeRef(), diagnostics.writeRef());
		if (vertexEntryPoint)
		{
			GetLogger()->info("\tFound: vertexMain");
		}

		diagnostics.setNull();
		module->findAndCheckEntryPoint("fragmentMain", SLANG_STAGE_FRAGMENT,
									   fragEntryPoint.writeRef(), diagnostics.writeRef());
		if (fragEntryPoint)
		{
			GetLogger()->info("\tFound: fragmentMain");
		}

		// Some alternative path for legacy hlsl style code with the pixel shader,
		// including the old vsMain or psMain style naming convention.
		if (!vertexEntryPoint)
		{
			diagnostics.setNull();
			module->findAndCheckEntryPoint("vsMain", SLANG_STAGE_VERTEX,
										   vertexEntryPoint.writeRef(), diagnostics.writeRef());
			if (vertexEntryPoint)
				GetLogger()->info("\tFound: vsMain");
		}

		if (!fragEntryPoint)
		{
			diagnostics.setNull();
			// SLANG_STAGE_PIXEL same as the SLANG_STAGE_FRAGMENT
			module->findAndCheckEntryPoint("psMain", SLANG_STAGE_PIXEL, fragEntryPoint.writeRef(),
										   diagnostics.writeRef());
			if (fragEntryPoint)
				GetLogger()->info("\tFound: psMain");
		}

		// Compute shader entries
		diagnostics.setNull();
		module->findAndCheckEntryPoint("computeMain", SLANG_STAGE_COMPUTE,
									   computeEntryPoint.writeRef(), diagnostics.writeRef());
		if (computeEntryPoint)
			GetLogger()->info("\tFound: computeMain");

		if (!computeEntryPoint)
		{
			diagnostics.setNull();
			module->findAndCheckEntryPoint("csMain", SLANG_STAGE_COMPUTE,
										   computeEntryPoint.writeRef(), diagnostics.writeRef());
			if (computeEntryPoint)
				GetLogger()->info("\tFound: csMain");
		}

		// Setting up the Component
		std::vector<slang::IComponentType*> components;
		components.push_back(module);

		// TODO UNUSED for now
		int32_t vertexIndex = -1;
		int32_t pixelIndex = -1;
		int32_t computeIndex = -1;

		if (vertexEntryPoint)
		{
			vertexIndex = static_cast<int32_t>(components.size());
			components.push_back(vertexEntryPoint);
		}
		if (fragEntryPoint)
		{
			pixelIndex = static_cast<int32_t>(components.size());
			components.push_back(fragEntryPoint);
		}
		if (computeEntryPoint)
		{
			computeIndex = static_cast<int32_t>(components.size());
			components.push_back(computeEntryPoint);
		}

		// Compose Modules and Entry Points
		Slang::ComPtr<slang::IComponentType> program;
		Slang::ComPtr<slang::IBlob> linkDiagnostics;

		SlangResult compResult = session->createCompositeComponentType(
			components.data(), static_cast<SlangInt>(components.size()), program.writeRef(),
			linkDiagnostics.writeRef());

		if (SLANG_FAILED(compResult))
		{
			GetLogger()->info("\tFailed to create program for compilation");
			if (linkDiagnostics && linkDiagnostics->getBufferSize() > 0)
			{
				GetLogger()->info("\tLink Error: {}",
								  static_cast<const char*>(linkDiagnostics->getBufferPointer()));
			}
			return result;
		}

		// Linking just makes sure there isn't any missing depedencies.
		// NOTE Maybe look into SLANG_RETURN_ON_FAIL ?
		GetLogger()->info("Linking program:");
		Slang::ComPtr<slang::IComponentType> linkedProgram;
		{
			Slang::ComPtr<slang::IBlob> linkDiag;
			SlangResult linkResult = program->link(linkedProgram.writeRef(), linkDiag.writeRef());

			if (SLANG_FAILED(linkResult))
			{
				GetLogger()->info("\tFailed to link program");
				if (linkDiag && linkDiag->getBufferSize() > 0)
				{
					GetLogger()->info("\tLink Error:{}",
									  static_cast<const char*>(linkDiag->getBufferPointer()));
				}
				return result;
			}
		}

		GetLogger()->info("\tProgram linked.");

		// This part is for getting target kernal code, in our case it is
		// the Direct3D DXIL bytecode.
		// NOTE: Prepare for spirv as it seems Microsoft will be moving
		// towards this direction in the future.
		GetLogger()->info("\tExtracting DXIL bytecode:");

		uint32_t entryPointIndex = 0;

		if (vertexEntryPoint)
		{
			auto* layout = linkedProgram->getLayout();
			if (layout)
			{
				auto* epLayout = layout->getEntryPointByIndex(entryPointIndex);
				if (epLayout)
				{
					const char* epName = epLayout->getName();
					GetLogger()->info("\tEntry point {} is: {}", entryPointIndex,
									  epName ? epName : "unknown");
				}
			}

			GetLogger()->info("\tExtracting vertex shader bytecode (entry point index {})...",
							  entryPointIndex);
			Slang::ComPtr<slang::IBlob> vertexBlob;
			diagnostics.setNull();
			SlangResult codeResult = linkedProgram->getEntryPointCode(
				entryPointIndex, 0, vertexBlob.writeRef(), diagnostics.writeRef());

			// Validate that we got actual DXIL bytecode using the magic number DXBC.
			if (SLANG_SUCCEEDED(codeResult) && vertexBlob)
			{
				const auto* data = static_cast<const uint8_t*>(vertexBlob->getBufferPointer());
				size_t size = vertexBlob->getBufferSize();

				if (size >= 4)
				{
					uint32_t magic = *reinterpret_cast<const uint32_t*>(data);
					GetLogger()->info("\tBytecode magic: 0x{:08X} (should be 0x43425844 for DXBC)",
									  magic);
				}

				// Save the blob to a lifetime managed std::vector from our data struct.
				result.CopyBlobToVector(vertexBlob, result.vertexBytecode);
				GetLogger()->info("\tVertex shader set: {} bytes DXIL",
								  result.vertexBytecode.size());
				entryPointIndex++;
			}
			else
			{
				GetLogger()->error("\tFailed to get vertex shader bytecode (result={})",
								   static_cast<int>(codeResult));
				if (diagnostics && diagnostics->getBufferSize() > 0)
				{
					GetLogger()->error("\tVertex Shader Bytecode Error: {}",
									   static_cast<const char*>(diagnostics->getBufferPointer()));
				}
			}
		}

		if (fragEntryPoint)
		{
			GetLogger()->info("\tExtracting pixel shader bytecode (entry point index {})...",
							  entryPointIndex);
			Slang::ComPtr<slang::IBlob> fragBlob;
			diagnostics.setNull();
			SlangResult codeResult = linkedProgram->getEntryPointCode(
				entryPointIndex, 0, fragBlob.writeRef(), diagnostics.writeRef());

			if (SLANG_SUCCEEDED(codeResult) && fragBlob)
			{
				result.CopyBlobToVector(fragBlob, result.fragBytecode);
				GetLogger()->info("\tFrag shader set: {} bytes DXIL", result.fragBytecode.size());
				entryPointIndex++;
			}
			else
			{
				GetLogger()->info("\tFailed to get fragment shader bytecode (result={})",
								  static_cast<uint32_t>(codeResult));
				if (diagnostics && diagnostics->getBufferSize() > 0)
				{
					GetLogger()->error("\t Frag Shader Bytecode Error: {}",
									   static_cast<const char*>(diagnostics->getBufferPointer()));
				}
			}
		}

		if (computeEntryPoint)
		{
			GetLogger()->info("\tExtracting compute shader bytecode (entry point index {})...",
							  entryPointIndex);
			Slang::ComPtr<slang::IBlob> computeBlob;
			diagnostics.setNull();
			SlangResult codeResult = linkedProgram->getEntryPointCode(
				entryPointIndex, 0, computeBlob.writeRef(), diagnostics.writeRef());

			if (SLANG_SUCCEEDED(codeResult) && computeBlob)
			{
				result.CopyBlobToVector(computeBlob, result.computeBytecode);
				GetLogger()->info("\tCompute shader set: {} bytes DXIL",
								  result.computeBytecode.size());
			}
			else
			{
				GetLogger()->error("\tFailed to get compute shader bytecode");
			}
		}

		// Getting compiled data from other helpers
		slang::ProgramLayout* layout = linkedProgram->getLayout(0);
		if (layout && layout->getEntryPointCount() > 0 && vertexEntryPoint)
		{
			GetLogger()->info("\tExtracting input layout:");

			auto* epReflection = layout->getEntryPointByIndex(0);
			if (epReflection && epReflection->getStage() == SLANG_STAGE_VERTEX)
			{
				// SlangInputLayout.h
				result.inputLayout = ExtractInputLayoutFromReflection(epReflection,
																	  result.semanticNames);
				GetLogger()->info("\tFound {} input elements", result.inputLayout.size());
			}
		}

		// Creating Root Signature Builder
		if (device && layout)
		{
			RootSignatureBuilder builder(layout, device);
			builder.SetBindlessMode(true);
			if (SUCCEEDED(builder.Build()))
			{
				result.rootSignature = builder.GetRootSignature();
				// Take ownership
				result.rootSignature->AddRef();
			}
			else
			{
				GetLogger()->error("Failed to build root signature");
			}
		}

		GetLogger()->info("\tShader compilation complete");

		// Debug output what we got
		GetLogger()->info("\tFinal compilation results:");
		if (!result.vertexBytecode.empty())
		{
			GetLogger()->info("\tVertex shader: {} bytes", result.vertexBytecode.size());
			GetLogger()->info("\tVertex bytecode address: {}",
							  static_cast<void*>(result.vertexBytecode.data()));
		}
		else
		{
			GetLogger()->warn("\tVertex shader: NOT CREATED");
		}

		if (!result.fragBytecode.empty())
		{
			GetLogger()->info("\tFragment shader: {} bytes", result.fragBytecode.size());
			GetLogger()->info("\tFragment bytecode address: {}",
							  static_cast<void*>(result.fragBytecode.data()));
		}
		else
		{
			GetLogger()->info("\tFragment shader: NOT CREATED");
		}

		GetLogger()->info("\tRoot signature: {}", result.rootSignature ? "Created" : "NOT CREATED");
		GetLogger()->info("\tInput layout elements: {}", result.inputLayout.size());

		return result;
	}

#endif // HAS_SLANG

} // namespace SlangHelper
