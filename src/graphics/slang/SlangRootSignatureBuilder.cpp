#include "SlangRootSignatureBuilder.h"
#include "SlangUtilities.h"
#include "d3d12.h"

#include <algorithm>

namespace SlangHelper
{

#ifdef HAS_SLANG

	RootSignatureBuilder::RootSignatureBuilder(slang::ProgramLayout* layout, ID3D12Device* device)
	: mLayout(layout)
	, mDevice(device)
	{
	}

	HRESULT RootSignatureBuilder::Build()
	{
		if (!mLayout || !mDevice)
		{
			GetLogger()->error("RootSignatureBuilder::Build() - Invalid layout or device");
			return E_INVALIDARG;
		}

		GetLogger()->info("RootSignatureBuilder: Building root signature from reflection");

		// We are building the root sig from the slang, which will be done in several
		// phases, will be using the reflection API to do this.
		//
		// Phase 1 will collect all resources
		CollectResources(mLayout);

		// Phase 2 will build optimized root parameters
		BuildRootParameters();

		// Phase 3 will create the D3D12 root signature
		return CreateRootSignature();
	}

	void RootSignatureBuilder::CollectResources(slang::ProgramLayout* layout)
	{
		GetLogger()->info("\tPhase 1: Collecting resources from reflection");

		uint32_t paramCount = layout->getParameterCount();
		GetLogger()->info("\t\tGlobal parameters: {}", paramCount);

		for (uint32_t i = 0; i < paramCount; ++i)
		{
			slang::VariableLayoutReflection* param = layout->getParameterByIndex(i);
			CollectParameter(param);
		}

		GetLogger()->info("\t\tCollected: {} CBV groups, {} SRV groups, {} UAV groups, {} samplers",
						  mCbvs.size(), mSrvs.size(), mUavs.size(), mStaticSamplers.size());
	}

	void RootSignatureBuilder::CollectParameter(slang::VariableLayoutReflection* param)
	{
		if (!param)
			return;

		slang::TypeLayoutReflection* type = param->getTypeLayout();
		if (!type)
			return;

		const char* name = param->getName();
		auto kind = type->getKind();

		// Get all categories this parameter binds to
		uint32_t categoryCount = param->getCategoryCount();

		for (uint32_t c = 0; c < categoryCount; ++c)
		{
			auto category = param->getCategoryByIndex(c);
			size_t offset = param->getOffset(static_cast<SlangParameterCategory>(category));
			size_t space = param->getBindingSpace(static_cast<SlangParameterCategory>(category));

			// Handle different resource categories
			if (category == slang::ParameterCategory::ConstantBuffer ||
				category == slang::ParameterCategory::SubElementRegisterSpace)
			{
				// Check if this is the SUB_ELEMENT_REGISTER_SPACE category
				bool isSubElementSpace =
					(category == slang::ParameterCategory::SubElementRegisterSpace);

				// Constant buffer or ParameterBlock
				ResourceBinding binding;
				binding.name = name ? name : "<unnamed>";

				// For ParameterBlocks with SUB_ELEMENT_REGISTER_SPACE the CBV goes into the sub-element
				// space at register 0
				if (isSubElementSpace && kind == slang::TypeReflection::Kind::ParameterBlock)
				{
					binding.reg = 0;
					binding.space = static_cast<uint32_t>(offset);
				}
				else
				{
					binding.reg = static_cast<uint32_t>(offset);
					binding.space = static_cast<uint32_t>(space);
				}

				binding.sizeBytes = 0;
				binding.hasOnlyUniforms = true;

				// Determine size and content type
				if (kind == slang::TypeReflection::Kind::ParameterBlock ||
					kind == slang::TypeReflection::Kind::ConstantBuffer)
				{
					slang::TypeLayoutReflection* elementType = type->getElementTypeLayout();
					if (elementType)
					{
						binding.sizeBytes = elementType->getSize(SLANG_PARAMETER_CATEGORY_UNIFORM);
						binding.hasOnlyUniforms = HasOnlyUniforms(elementType);
					}
					else
					{
						binding.sizeBytes = type->getSize(SLANG_PARAMETER_CATEGORY_UNIFORM);
					}
				}
				else
				{
					binding.sizeBytes = type->getSize(SLANG_PARAMETER_CATEGORY_UNIFORM);
				}

				// Add to CBV map grouped by (space, reg)
				auto key = std::make_pair(binding.space, binding.reg);
				mCbvs[key].push_back(binding);

				GetLogger()->info("\t\t\tCBV: {} -> b{} space{}, {} bytes, uniforms={}",
								  binding.name, binding.reg, binding.space, binding.sizeBytes,
								  binding.hasOnlyUniforms);

				// Internal resources
				// ParameterBlocks use a subelement register space for contents
				if (kind == slang::TypeReflection::Kind::ParameterBlock)
				{
					size_t subElementSpace = 0;
					bool foundSubElementSpace = false;

					for (uint32_t sc = 0; sc < categoryCount; ++sc)
					{
						auto subCategory = param->getCategoryByIndex(sc);
						if (subCategory == slang::ParameterCategory::SubElementRegisterSpace)
						{
							subElementSpace =
								param->getOffset(static_cast<SlangParameterCategory>(subCategory));
							foundSubElementSpace = true;
							GetLogger()->info("\t\t\t\tParameterBlock sub-element space: {}",
											  subElementSpace);
							break;
						}
					}

					slang::TypeLayoutReflection* elementType = type->getElementTypeLayout();
					if (elementType &&
						elementType->getKind() == slang::TypeReflection::Kind::Struct)
					{
						uint32_t fieldCount = elementType->getFieldCount();
						for (uint32_t f = 0; f < fieldCount; ++f)
						{
							slang::VariableLayoutReflection* field =
								elementType->getFieldByIndex(f);
							if (field)
							{
								CollectParameterBlockField(
									field, foundSubElementSpace ? subElementSpace : space);
							}
						}
					}
				}
			}
			else if (category == slang::ParameterCategory::ShaderResource)
			{
				ResourceBinding binding;
				binding.name = name ? name : "<unnamed>";
				binding.reg = static_cast<uint32_t>(offset);
				binding.space = static_cast<uint32_t>(space);
				binding.sizeBytes = 0;
				binding.hasOnlyUniforms = false;

				auto key = std::make_pair(binding.space, binding.reg);
				mSrvs[key].push_back(binding);

				GetLogger()->info("\t\t\tSRV: {} -> t{} space{}", binding.name, binding.reg,
								  binding.space);
			}
			else if (category == slang::ParameterCategory::UnorderedAccess)
			{
				ResourceBinding binding;
				binding.name = name ? name : "<unnamed>";
				binding.reg = static_cast<uint32_t>(offset);
				binding.space = static_cast<uint32_t>(space);
				binding.sizeBytes = 0;
				binding.hasOnlyUniforms = false;

				auto key = std::make_pair(binding.space, binding.reg);
				mUavs[key].push_back(binding);

				GetLogger()->info("\t\t\tUAV: {} -> u{} space{}", binding.name, binding.reg,
								  binding.space);
			}
			else if (category == slang::ParameterCategory::SamplerState)
			{
				// Use static samplers (0 DWORD cost)
				D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
				samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
				samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
				samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
				samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
				samplerDesc.MipLODBias = 0;
				samplerDesc.MaxAnisotropy = 16;
				samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
				samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
				samplerDesc.MinLOD = 0.0F;
				samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
				samplerDesc.ShaderRegister = static_cast<UINT>(offset);
				samplerDesc.RegisterSpace = static_cast<UINT>(space);
				samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

				mStaticSamplers.push_back(samplerDesc);

				GetLogger()->info("\t\t\tStatic Sampler: {} -> s{} space{}",
								  name ? name : "<unnamed>", offset, space);
			}
		}

		// Handle structs by recursing into fields
		if (kind == slang::TypeReflection::Kind::Struct)
		{
			uint32_t fieldCount = type->getFieldCount();
			for (uint32_t i = 0; i < fieldCount; ++i)
			{
				slang::VariableLayoutReflection* field = type->getFieldByIndex(i);
				if (field)
				{
					CollectParameter(field);
				}
			}
		}
	}

	void RootSignatureBuilder::CollectParameterBlockField(slang::VariableLayoutReflection* field,
														  size_t overrideSpace)
	{
		if (!field)
			return;

		slang::TypeLayoutReflection* type = field->getTypeLayout();
		if (!type)
			return;

		const char* name = field->getName();
		// auto kind = type->getKind();

		// Get all categories this field binds to
		uint32_t categoryCount = field->getCategoryCount();

		for (uint32_t c = 0; c < categoryCount; ++c)
		{
			auto category = field->getCategoryByIndex(c);
			size_t offset = field->getOffset(static_cast<SlangParameterCategory>(category));
			// Override the space with the ParameterBlock's subelement space
			size_t space = overrideSpace;

			if (category == slang::ParameterCategory::ShaderResource)
			{
				ResourceBinding binding;
				binding.name = name ? name : "<unnamed>";
				binding.reg = static_cast<uint32_t>(offset);
				binding.space = static_cast<uint32_t>(space);
				binding.sizeBytes = 0;
				binding.hasOnlyUniforms = false;

				auto key = std::make_pair(binding.space, binding.reg);
				mSrvs[key].push_back(binding);

				GetLogger()->info("\t\t\t\tSRV (from ParameterBlock): {} -> t{} space{}",
								  binding.name, binding.reg, binding.space);
			}
			else if (category == slang::ParameterCategory::UnorderedAccess)
			{
				ResourceBinding binding;
				binding.name = name ? name : "<unnamed>";
				binding.reg = static_cast<uint32_t>(offset);
				binding.space = static_cast<uint32_t>(space);
				binding.sizeBytes = 0;
				binding.hasOnlyUniforms = false;

				auto key = std::make_pair(binding.space, binding.reg);
				mUavs[key].push_back(binding);

				GetLogger()->info("\t\t\t\tUAV (from ParameterBlock): {} -> u{} space{}",
								  binding.name, binding.reg, binding.space);
			}
			else if (category == slang::ParameterCategory::SamplerState)
			{
				// Use static samplers (0 DWORD cost)
				D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
				samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
				samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
				samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
				samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
				samplerDesc.MipLODBias = 0;
				samplerDesc.MaxAnisotropy = 16;
				samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
				samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
				samplerDesc.MinLOD = 0.0F;
				samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
				samplerDesc.ShaderRegister = static_cast<UINT>(offset);
				samplerDesc.RegisterSpace = static_cast<UINT>(space);
				samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

				mStaticSamplers.push_back(samplerDesc);

				GetLogger()->info("\t\t\t\tStatic Sampler (from ParameterBlock): {} -> s{} space{}",
								  name ? name : "<unnamed>", offset, space);
			}
		}
	}

	bool RootSignatureBuilder::HasOnlyUniforms(slang::TypeLayoutReflection* type)
	{
		if (!type)
			return true;

		auto kind = type->getKind();

		// Resources are not uniforms
		if (kind == slang::TypeReflection::Kind::Resource ||
			kind == slang::TypeReflection::Kind::SamplerState)
		{
			return false;
		}

		// Check struct fields recursively
		if (kind == slang::TypeReflection::Kind::Struct)
		{
			uint32_t fieldCount = type->getFieldCount();
			for (uint32_t i = 0; i < fieldCount; ++i)
			{
				slang::VariableLayoutReflection* field = type->getFieldByIndex(i);
				if (field)
				{
					slang::TypeLayoutReflection* fieldType = field->getTypeLayout();
					if (!HasOnlyUniforms(fieldType))
					{
						return false;
					}
				}
			}
		}

		return true;
	}

	void RootSignatureBuilder::BuildRootParameters()
	{
		GetLogger()->info("\tPhase 2: Building optimized root parameters");

		// How do the root sig param get placed? According to the sizes.
		//
		// ROOT CONSTANTS (1 DWORD per constant):
		//   - If ≤ 16 bytes (4 DWORDs or less)
		//
		// ROOT DESCRIPTORS (2 DWORDs GPU address):
		//   - If 17-256 bytes

		// DESCRIPTOR TABLES (1 DWORD per table):
		//   - If > 256 bytes

		// STATIC SAMPLERS (0 DWORDs):

		uint32_t currentSlot = 0;

		// Process CBVs with above rules.
		std::map<std::pair<uint32_t, uint32_t>, std::vector<ResourceBinding>> tableCBVs;

		for (const auto& [key, bindings] : mCbvs)
		{
			for (const auto& binding : bindings)
			{
				if (binding.hasOnlyUniforms && binding.sizeBytes > 0)
				{
					if (binding.sizeBytes <= 64)
					{
						// ROOT CONSTANTS
						D3D12_ROOT_PARAMETER param = {};
						param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
						param.Constants.ShaderRegister = binding.reg;
						param.Constants.RegisterSpace = binding.space;
						param.Constants.Num32BitValues =
							static_cast<UINT>((binding.sizeBytes + 3) / 4);
						param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

						mRootParams.push_back(param);
						mTotalDwordCost += param.Constants.Num32BitValues;

						GetLogger()->info(
							"\t\t[Slot {}] ROOT CONSTANTS: {} ({} bytes = {} DWORDs) -> b{} space{}",
							currentSlot++, binding.name, binding.sizeBytes,
							param.Constants.Num32BitValues, binding.reg, binding.space);
					}
					else if (binding.sizeBytes <= 256)
					{
						// ROOT DESCRIPTOR
						D3D12_ROOT_PARAMETER param = {};
						param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
						param.Descriptor.ShaderRegister = binding.reg;
						param.Descriptor.RegisterSpace = binding.space;
						param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

						mRootParams.push_back(param);

						// Recall Root descriptors cost 2 DWORDs
						mTotalDwordCost += 2;

						GetLogger()->info(
							"\t\t[Slot {}] ROOT CBV: {} ({} bytes, 2 DWORDs) -> b{} space{}",
							currentSlot++, binding.name, binding.sizeBytes, binding.reg,
							binding.space);
					}
					else
					{
						// DESCRIPTOR TABLE
						tableCBVs[key].push_back(binding);
					}
				}
				else
				{
					// Unknown just delegate to the table
					tableCBVs[key].push_back(binding);
				}
			}
		}

		CreateDescriptorTable(tableCBVs, D3D12_DESCRIPTOR_RANGE_TYPE_CBV, "CBV");
		CreateDescriptorTable(mSrvs, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, "SRV");
		CreateDescriptorTable(mUavs, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, "UAV");

		GetLogger()->info("\t\tTotal Root Signature Cost: {} DWORDs (out of 64 DWORD limit)",
						  mTotalDwordCost);

		if (mTotalDwordCost > 64)
		{
			GetLogger()->warn("\t\tWARNING: Root signature exceeds 64 DWORD limit!");
		}
		else
		{
			GetLogger()->info("\t\tOK: {} DWORDs remaining", 64 - mTotalDwordCost);
		}
	}

	void RootSignatureBuilder::CreateDescriptorTable(
		const std::map<std::pair<uint32_t, uint32_t>, std::vector<ResourceBinding>>& resourceMap,
		D3D12_DESCRIPTOR_RANGE_TYPE rangeType, const char* debugName)
	{
		if (resourceMap.empty())
			return;

		std::map<uint32_t, std::vector<ResourceBinding>> bySpace;

		for (const auto& [key, bindings] : resourceMap)
		{
			uint32_t space = key.first;
			for (const auto& binding : bindings)
			{
				bySpace[space].push_back(binding);
			}
		}

		// Create descriptor table for each space
		for (auto& [space, bindings] : bySpace)
		{
			if (bindings.empty())
				continue;

			// Sort by register for contiguous range merging
			std::ranges::sort(bindings.begin(), bindings.end(),
							  [](const ResourceBinding& a, const ResourceBinding& b) {
								  return a.reg < b.reg;
							  });

			std::vector<D3D12_DESCRIPTOR_RANGE> ranges;

			uint32_t minReg = bindings.front().reg;
			uint32_t maxReg = bindings.back().reg;

			D3D12_DESCRIPTOR_RANGE range = {};
			range.RangeType = rangeType;
			range.NumDescriptors = maxReg - minReg + 1;
			range.BaseShaderRegister = minReg;
			range.RegisterSpace = space;
			range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

			ranges.push_back(range);

			mDescriptorTableRanges.push_back(ranges);

			D3D12_ROOT_PARAMETER param = {};
			param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			param.DescriptorTable.NumDescriptorRanges = static_cast<UINT>(ranges.size());
			param.DescriptorTable.pDescriptorRanges = mDescriptorTableRanges.back().data();
			param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

			mRootParams.push_back(param);

			// Tables cost 1 DWORD
			mTotalDwordCost += 1;
			GetLogger()->info("\t\t[Slot {}] DESCRIPTOR TABLE ({}): {} descriptors, space {}",
							  mRootParams.size() - 1, debugName, range.NumDescriptors, space);

			for (const auto& binding : bindings)
			{
				GetLogger()->info("\t\t\t- {}", binding.name);
			}
		}
	}

	HRESULT RootSignatureBuilder::CreateRootSignature()
	{
		GetLogger()->info("\tPhase 3: Creating D3D12 root signature");

		D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
		rootSigDesc.NumParameters = static_cast<UINT>(mRootParams.size());
		rootSigDesc.pParameters = mRootParams.empty() ? nullptr : mRootParams.data();
		rootSigDesc.NumStaticSamplers = static_cast<UINT>(mStaticSamplers.size());
		rootSigDesc.pStaticSamplers = mStaticSamplers.empty() ? nullptr : mStaticSamplers.data();

		D3D12_ROOT_SIGNATURE_FLAGS flags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

		// Enabling more flags if we enabling bindless, see:
		// https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_root_signature_flags
		// if (mIsBindless)
		// {
		flags |= D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED;
		flags |= D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED;
		// }

		rootSigDesc.Flags = flags;

		Microsoft::WRL::ComPtr<ID3DBlob> signature;
		Microsoft::WRL::ComPtr<ID3DBlob> error;

		HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1_0,
												 &signature, &error);

		if (FAILED(hr))
		{
			GetLogger()->error("\t\tFailed to serialize root signature: 0x{:X}", hr);
			if (error)
			{
				GetLogger()->error("\t\tError: {}",
								   static_cast<const char*>(error->GetBufferPointer()));
			}
			return hr;
		}

		GetLogger()->info("\t\tRoot signature serialized successfully");

		hr = mDevice->CreateRootSignature(0, signature->GetBufferPointer(),
										  signature->GetBufferSize(),
										  IID_PPV_ARGS(&mRootSignature));

		if (FAILED(hr))
		{
			GetLogger()->error("\t\tFailed to create root signature: 0x{:X}", hr);
			return hr;
		}

		GetLogger()->info("\t\tRoot signature created successfully");
		return S_OK;
	}

#endif // HAS_SLANG

} // namespace SlangHelper
