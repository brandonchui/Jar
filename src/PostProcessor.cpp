#include "PostProcessor.h"
#include "graphics/CommandContext.h"
#include "graphics/Core.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include <DirectXMath.h>

#ifdef USE_PIX
#include <WinPixEventRuntime/pix3.h>
#endif

void PostProcessor::InitLogger()
{
	if (!mLogger)
	{
		mLogger = spdlog::get("PostProcessor");
		if (!mLogger)
		{
			mLogger = spdlog::stdout_color_mt("PostProcessor");
			mLogger->set_pattern("[%H:%M:%S] [%^%l%$] [%n] %v");
			mLogger->set_level(spdlog::level::debug);
		}
	}
}

void PostProcessor::Initialize(uint32_t width, uint32_t height, DescriptorHeap* textureHeap)
{
	InitLogger();

	mWidth = width;
	mHeight = height;

	mTempTexture = std::make_unique<ColorBuffer>();
	mTempTexture->Create(L"BlurTempTexture", width, height, 1, DXGI_FORMAT_R8G8B8A8_UNORM, true);

#ifdef ENABLE_BINDLESS
	(void)textureHeap;
	mTempTexture->CreateSRV({});
	mTempTexture->CreateUAV({});
#else
	if (textureHeap)
	{
		mTempSRV = textureHeap->Alloc(1);
		mTempUAV = textureHeap->Alloc(1);
		mTempTexture->CreateSRV(mTempSRV.GetCpuHandle());
		mTempTexture->CreateUAV(mTempUAV.GetCpuHandle());
	}
#endif

	mLogger->info("PostProcessor initialized ({}x{})", width, height);
}

void PostProcessor::Resize(uint32_t width, uint32_t height)
{
	if (width == mWidth && height == mHeight)
		return;

	if (width == 0 || height == 0)
		return;

	mWidth = width;
	mHeight = height;

	mTempTexture.reset();
	mTempTexture = std::make_unique<ColorBuffer>();
	mTempTexture->Create(L"BlurTempTexture", width, height, 1, DXGI_FORMAT_R8G8B8A8_UNORM, true);

#ifdef ENABLE_BINDLESS
	mTempTexture->CreateSRV({});
	mTempTexture->CreateUAV({});
#else
	mTempTexture->CreateSRV(mTempSRV.GetCpuHandle());
	mTempTexture->CreateUAV(mTempUAV.GetCpuHandle());
#endif

	mLogger->info("PostProcessor resized to {}x{}", width, height);
}

void PostProcessor::ApplyBlur(Graphics::GraphicsContext& context, ColorBuffer& target,
							  DescriptorHeap* samplerHeap)
{
#ifdef USE_PIX
	PIXBeginEvent(context.GetCommandList(), PIX_COLOR_INDEX(200), "Post-Process");
	PIXBeginEvent(context.GetCommandList(), PIX_COLOR_INDEX(210), "Gaussian Blur");
#endif

	context.TransitionResource(target, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	context.TransitionResource(*mTempTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

#ifdef USE_PIX
	PIXBeginEvent(context.GetCommandList(), PIX_COLOR_INDEX(211), "Blur Horizontal");
#endif

#ifdef ENABLE_BINDLESS
	context.SetDescriptorHeaps(Graphics::gBindlessAllocator->GetHeap(),
							   samplerHeap->GetHeapPointer());
#else
	context.SetDescriptorHeaps(*samplerHeap);
#endif

	context.SetComputeShader("BlurHorizontal");
	context.BindComputePipeline();

	context.SetComputeConstants(0, 1, &mBlurIntensity);

#ifdef ENABLE_BINDLESS
	{
		struct ComputeResources
		{
			DirectX::XMUINT2 mInputTex;
			DirectX::XMUINT2 mOutputTex;
		};

		ComputeResources resources = {};
		resources.mInputTex.x = target.GetSRVIndex();
		resources.mInputTex.y = 0;
		resources.mOutputTex.x = mTempTexture->GetUAVIndex();
		resources.mOutputTex.y = 0;

		context.GetCommandList()->SetComputeRoot32BitConstants(1, 4, &resources, 0);
	}
#else
	context.SetComputeRootDescriptorTable(1, target.GetSRV());
	context.SetComputeRootDescriptorTable(2, mTempUAV.GetGpuHandle());
#endif

	uint32_t groupsX = (mWidth + 7) / 8;
	uint32_t groupsY = (mHeight + 7) / 8;
	context.Dispatch(groupsX, groupsY, 1);

#ifdef USE_PIX
	PIXEndEvent(context.GetCommandList());
#endif

	context.TransitionResource(*mTempTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	context.TransitionResource(target, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

#ifdef USE_PIX
	PIXBeginEvent(context.GetCommandList(), PIX_COLOR_INDEX(212), "Blur Vertical");
#endif

#ifdef ENABLE_BINDLESS
	context.SetDescriptorHeaps(Graphics::gBindlessAllocator->GetHeap(),
							   samplerHeap->GetHeapPointer());
#else
	context.SetDescriptorHeaps(*samplerHeap);
#endif

	context.SetComputeShader("BlurVertical");
	context.BindComputePipeline();

	context.SetComputeConstants(0, 1, &mBlurIntensity);

#ifdef ENABLE_BINDLESS
	{
		struct ComputeResources
		{
			DirectX::XMUINT2 mInputTex;
			DirectX::XMUINT2 mOutputTex;
		};

		ComputeResources resources = {};
		resources.mInputTex.x = mTempTexture->GetSRVIndex();
		resources.mInputTex.y = 0;
		resources.mOutputTex.x = target.GetUAVIndex();
		resources.mOutputTex.y = 0;

		context.GetCommandList()->SetComputeRoot32BitConstants(1, 4, &resources, 0);
	}
#else
	context.SetComputeRootDescriptorTable(1, mTempSRV.GetGpuHandle());
	context.SetComputeRootDescriptorTable(2, target.GetUAV());
#endif

	context.Dispatch(groupsX, groupsY, 1);

#ifdef USE_PIX
	PIXEndEvent(context.GetCommandList());
#endif

	context.TransitionResource(target, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

#ifdef USE_PIX
	PIXEndEvent(context.GetCommandList());
	PIXEndEvent(context.GetCommandList());
#endif
}
