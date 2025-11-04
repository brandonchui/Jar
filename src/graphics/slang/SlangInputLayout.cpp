#include "SlangInputLayout.h"
#include "SlangUtilities.h"
#include <algorithm>
#include <string>

namespace SlangHelper
{

#ifdef HAS_SLANG

	std::vector<D3D12_INPUT_ELEMENT_DESC>
	ExtractInputLayoutFromReflection(slang::EntryPointReflection* entryPoint, std::vector<std::string>& outSemanticNames)
	{
		GetLogger()->info("Calling ExtractInputLayoutFromReflection():");
		std::vector<D3D12_INPUT_ELEMENT_DESC> inputElements;
		outSemanticNames.clear();

		if (!entryPoint || entryPoint->getStage() != SLANG_STAGE_VERTEX)
		{
			GetLogger()->info("\tNot a vertex shader entry point");
			return inputElements;
		}

		uint32_t paramCount = entryPoint->getParameterCount();
		GetLogger()->info("\tEntry point has {} parameters", paramCount);

		for (uint32_t i = 0; i < paramCount; ++i)
		{
			auto* param = entryPoint->getParameterByIndex(i);
			if (!param)
				continue;

			const char* paramName = param->getName();
			GetLogger()->info("\tChecking parameter {}: {}", i,
							  paramName ? paramName : "<unnamed>");

			bool isVaryingInput = false;
			uint32_t categoryCount = param->getCategoryCount();
			for (uint32_t c = 0; c < categoryCount; ++c)
			{
				auto category = param->getCategoryByIndex(c);
				if (category == slang::ParameterCategory::VaryingInput)
				{
					isVaryingInput = true;
					break;
				}
			}

			if (!isVaryingInput)
			{
				GetLogger()->info("\tNot a varying input, skipping");
				continue;
			}

			auto* typeLayout = param->getTypeLayout();
			if (!typeLayout)
			{
				GetLogger()->info("\tNo type layout");
				continue;
			}

			// Checking if we get a _struct_ from the reflection, usually
			// the vertex in/out are structs.
			if (typeLayout->getKind() == slang::TypeReflection::Kind::Struct)
			{
				GetLogger()->info("\tFound struct inputs: ");
				uint32_t fieldCount = typeLayout->getFieldCount();

				struct FieldInfo
				{
					slang::VariableLayoutReflection* field;
					size_t offset;
					uint32_t index;
				};

				std::vector<FieldInfo> fields;
				fields.reserve(fieldCount);
				outSemanticNames.reserve(outSemanticNames.size() + fieldCount);

				for (uint32_t fieldIdx = 0; fieldIdx < fieldCount; fieldIdx++)
				{
					auto* field = typeLayout->getFieldByIndex(fieldIdx);
					if (!field)
						continue;

					size_t fieldOffset = field->getOffset();
					const char* fieldName = field->getSemanticName();
					GetLogger()->info("\t\tField[{}]: name={}, offset={}", fieldIdx,
									  fieldName ? fieldName : "none", fieldOffset);
					fields.push_back({.field = field, .offset = fieldOffset, .index = fieldIdx});
				}

				// The declaration might of got messed up somewhere in the process,
				// so just sort to double check it is correct.
				std::ranges::sort(fields.begin(), fields.end(),
								  [](const FieldInfo& a, const FieldInfo& b) {
									  return a.index < b.index;
								  });

				size_t currentOffset = 0;

				GetLogger()->info("\tAfter sorting by index, field order:");
				for (size_t j = 0; j < fields.size(); j++)
				{
					const char* name = fields[j].field->getSemanticName();
					GetLogger()->info("\t\t[{}]: {} (original index {})", j, name ? name : "none",
									  fields[j].index);
				}

				for (const auto& fieldInfo : fields)
				{
					auto* field = fieldInfo.field;
					const char* semanticName = field->getSemanticName();
					size_t semanticIndex = field->getSemanticIndex();

					GetLogger()->info(
						"\t\tProcessing Field[{}]: semantic={}, reported offset={} bytes, calculated offset={}",
						fieldInfo.index, semanticName ? semanticName : "none", fieldInfo.offset,
						currentOffset);

					if (!semanticName)
					{
						GetLogger()->warn("\t\tField {} has no semantic", fieldInfo.index);
						continue;
					}

					// DXGI_FORMAT finder
					DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
					size_t elementSize = 0;
					auto* fieldType = field->getTypeLayout();

					if (fieldType)
					{
						if (fieldType->getKind() == slang::TypeReflection::Kind::Vector)
						{
							auto* elementType = fieldType->getElementTypeLayout();
							size_t elementCount = fieldType->getElementCount();

							if (elementType && elementType->getScalarType() ==
												   slang::TypeReflection::ScalarType::Float32)
							{
								switch (elementCount)
								{
								case 2: // 8 bytes, 2 floats
									format = DXGI_FORMAT_R32G32_FLOAT;
									elementSize = 8;
									break;
								case 3: //3 floats but gets padded
									format = DXGI_FORMAT_R32G32B32_FLOAT;
									elementSize = 16;
									break;
								case 4:
									format = DXGI_FORMAT_R32G32B32A32_FLOAT;
									elementSize = 16;
									break;
								}
							}
						}
						else if (fieldType->getKind() == slang::TypeReflection::Kind::Scalar)
						{
							// Check for scalar float
							if (fieldType->getScalarType() == slang::TypeReflection::Float32)
							{
								format = DXGI_FORMAT_R32_FLOAT;
								elementSize = 4;
							}
						}
					}

					if (format != DXGI_FORMAT_UNKNOWN)
					{
						outSemanticNames.push_back(std::string(semanticName));

						D3D12_INPUT_ELEMENT_DESC element = {};
						element.SemanticName = outSemanticNames.back().c_str();
						element.SemanticIndex = static_cast<UINT>(semanticIndex);
						element.Format = format;
						element.InputSlot = 0;
						element.AlignedByteOffset = static_cast<UINT>(currentOffset);
						element.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
						element.InstanceDataStepRate = 0;

						inputElements.push_back(element);
						GetLogger()->info(
							"\t\tAdded input element: {} (index {}, format {}, actual offset {})",
							semanticName, semanticIndex, static_cast<uint32_t>(format),
							currentOffset);

						currentOffset += elementSize;
					}
				}
			}
			else
			{
				// Handle nonstruct inputs aka individual parameter semantics
				const char* semanticName = param->getSemanticName();
				size_t semanticIndex = param->getSemanticIndex();

				if (!semanticName)
					continue;

				DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;

				if (typeLayout->getKind() == slang::TypeReflection::Kind::Vector)
				{
					auto* elementType = typeLayout->getElementTypeLayout();
					size_t elementCount = typeLayout->getElementCount();

					if (elementType &&
						elementType->getScalarType() == slang::TypeReflection::ScalarType::Float32)
					{
						switch (elementCount)
						{
						case 2:
							format = DXGI_FORMAT_R32G32_FLOAT;
							break;
						case 3:
							format = DXGI_FORMAT_R32G32B32_FLOAT;
							break;
						case 4:
							format = DXGI_FORMAT_R32G32B32A32_FLOAT;
							break;
						}
					}
				}

				if (format != DXGI_FORMAT_UNKNOWN)
				{
					outSemanticNames.push_back(std::string(semanticName));

					D3D12_INPUT_ELEMENT_DESC element = {};
					element.SemanticName = outSemanticNames.back().c_str();
					element.SemanticIndex = static_cast<UINT>(semanticIndex);
					element.Format = format;
					element.InputSlot = 0;
					element.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
					element.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
					element.InstanceDataStepRate = 0;

					inputElements.push_back(element);
					GetLogger()->info("\tAdded input element: {} (index {})", semanticName,
									  semanticIndex);
				}
			}
		}

		GetLogger()->info("\tTotal input elements extracted: {}", inputElements.size());
		return inputElements;
	}

#endif // HAS_SLANG

} // namespace SlangHelper
