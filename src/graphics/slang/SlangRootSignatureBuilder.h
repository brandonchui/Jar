#pragma once

#include <vector>
#include <map>
#include <string>

#ifdef HAS_SLANG
#include <slang.h>
#include <d3d12.h>
#include <wrl/client.h>
#endif

namespace SlangHelper
{

#ifdef HAS_SLANG

	/// Builds root signatures from Slang reflection data.
	/// Based on resource size and type to choose between:
	/// - Root Constants (≤16 bytes): 1 DWORD per 4 bytes
	/// - Root Descriptors (17-256 bytes): 2 DWORDs
	/// - Descriptor Tables (>256 bytes or resources): 1 DWORD
	/// NOTE: Probably a better way to go about this, but if we are just
	/// using a few variables for some Slang struct, it can go in the
	/// root sig as a direct root constant.

	class RootSignatureBuilder
	{
	public:
		RootSignatureBuilder(slang::ProgramLayout* layout, ID3D12Device* device);
		~RootSignatureBuilder() = default;

		/// Build and create the D3D12 root signature from reflection data
		HRESULT Build();

		ID3D12RootSignature* GetRootSignature() const { return mRootSignature.Get(); }

		/// Sets whether to use bindless mode on. Will be off by defauilt but most likely
		/// will leave it on for the app.
		void SetBindlessMode(bool isBindless = false) { mIsBindless = isBindless; }
		bool IsBindlessModeEnabled() const { return mIsBindless; }

	private:
		/// Information about a resource binding from reflection
		struct ResourceBinding
		{
			std::string name;
			/// Register number like b#, t#, u#, s#
			uint32_t reg;

			/// Register space like space0, space 1 etc
			uint32_t space;
			size_t sizeBytes;
			bool hasOnlyUniforms;
		};

		void CollectResources(slang::ProgramLayout* layout);
		void CollectParameter(slang::VariableLayoutReflection* param);
		void CollectParameterBlockField(slang::VariableLayoutReflection* field,
										size_t overrideSpace);

		void BuildRootParameters();
		void CreateDescriptorTable(const std::map<std::pair<uint32_t, uint32_t>,
												  std::vector<ResourceBinding>>& resourceMap,
								   D3D12_DESCRIPTOR_RANGE_TYPE rangeType, const char* debugName);

		HRESULT CreateRootSignature();

		bool HasOnlyUniforms(slang::TypeLayoutReflection* type);

		slang::ProgramLayout* mLayout;
		ID3D12Device* mDevice;

		std::map<std::pair<uint32_t, uint32_t>, std::vector<ResourceBinding>> mCbvs;
		std::map<std::pair<uint32_t, uint32_t>, std::vector<ResourceBinding>> mSrvs;
		std::map<std::pair<uint32_t, uint32_t>, std::vector<ResourceBinding>> mUavs;
		std::vector<D3D12_STATIC_SAMPLER_DESC> mStaticSamplers;

		std::vector<D3D12_ROOT_PARAMETER> mRootParams;
		std::vector<std::vector<D3D12_DESCRIPTOR_RANGE>> mDescriptorTableRanges;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;

		uint32_t mTotalDwordCost = 0;

		bool mIsBindless;
	};

#endif // HAS_SLANG

} // namespace SlangHelper
