#include "Renderer.h"
#include "AssetManager.h"
#include "OrbitCamera.h"
#include "graphics/Core.h"
#include "ui/UISystem.h"
#include "d3d12.h"
#include "graphics/CommandContext.h"
#include "graphics/CommandListManager.h"
#include "graphics/ColorBuffer.h"
#include "graphics/UploadBuffer.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include <d3dx12/d3dx12.h>
#include <numbers>
#include <cstddef>
#include <DirectXMesh.h>
#include <vector>
#include <fstream>
#include <nlohmann/json.hpp>

#ifdef USE_PIX
#include <WinPixEventRuntime/pix3.h>
#endif

using namespace Graphics;

void Renderer::InitLogger()
{
	if (!mLogger)
	{
		mLogger = spdlog::get("Renderer");
		if (!mLogger)
		{
			mLogger = spdlog::stdout_color_mt("Renderer");
			mLogger->set_pattern("[%H:%M:%S] [%^%l%$] [%n] %v");
			mLogger->set_level(spdlog::level::debug);
		}
	}
}

void Renderer::Initialize(UISystem* uiSystem)
{
	InitLogger();

	if (uiSystem == nullptr)
	{
		mLogger->error("UISystem is null, cannot allocate viewport SRV");
		return;
	}

	mLogger->info("Structure sizes and alignment:");
	mLogger->info("\tsizeof(Vector3) = {} bytes", sizeof(Vector3));
	mLogger->info("\tsizeof(Float3) = {} bytes", sizeof(Float3));
	mLogger->info("\tsizeof(LightingConstants) = {} bytes", sizeof(LightingConstants));
	mLogger->info("\toffsetof(eyePosition) = {} bytes", offsetof(LightingConstants, eyePosition));
	mLogger->info("\toffsetof(numActiveLights) = {} bytes",
				  offsetof(LightingConstants, numActiveLights));
	mLogger->info("\toffsetof(ambientLight) = {} bytes", offsetof(LightingConstants, ambientLight));
	mLogger->info("\toffsetof(padding) = {} bytes", offsetof(LightingConstants, padding));

	static_assert(sizeof(LightingConstants) == 96, "LightingConstants size mismatch with Slang");
	static_assert(sizeof(SpotLight) == 64, "SpotLight size mismatch with Slang");

	// Initialize constant buffers.
	const uint32_t CONSTANT_BUFFER_SIZE = ((sizeof(Transform) + 255U) & ~255U) * 64;
	mConstantUploadBuffer.Initialize(CONSTANT_BUFFER_SIZE);

	const uint32_t MATERIAL_BUFFER_SIZE = ((sizeof(MaterialConstants) + 255U) & ~255U) * 64;
	mMaterialUploadBuffer.Initialize(MATERIAL_BUFFER_SIZE);

	// Clear the material buffer to prevent garbage data
	{
		MaterialConstants zeroMat = {};
		const uint32_t MATERIAL_ALIGNMENT = (sizeof(MaterialConstants) + 255U) & ~255U;
		for (uint32_t i = 0; i < 64; ++i)
		{
			mMaterialUploadBuffer.Copy(&zeroMat, sizeof(MaterialConstants), i * MATERIAL_ALIGNMENT);
		}
	}

	const uint32_t LIGHTING_BUFFER_SIZE = (sizeof(LightingConstants) + 255U) & ~255U;
	mLightingUploadBuffer.Initialize(LIGHTING_BUFFER_SIZE);

	mConstants.wvp = Matrix4::identity();
	mConstants.world = Matrix4::identity();
	mConstants.worldInvTrans = Matrix4::identity();

	// Initialize lighting constants with Float3 since the math lib are SIMD aligned and
	// that messes the byte alignment up a bit.
	mLightingConstants.eyePosition = Float3(0.0F, 0.0F, -20.0F);
	mLightingConstants.numActiveLights = 0;
	mLightingConstants.ambientLight = Float3(0.1F, 0.1F, 0.1F);

	// The lights data are better suited with StructuredBuffers since the array
	// is not fixed.
	mLightBuffer = std::make_unique<Graphics::StructuredBuffer>();
	mLightBuffer->Create(L"SpotLightBuffer", MAX_SPOT_LIGHTS, sizeof(SpotLight));

	// Initializing the heaps
	mTextureHeap.Create(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1024, true);
	mSamplerHeap.Create(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 16, true);

	mAssetManager = std::make_unique<AssetManager>();
	mAssetManager->Initialize(&mTextureHeap);

	const float ASPECT_RATIO = (mViewport.Width > 0.0F && mViewport.Height > 0.0F)
								   ? mViewport.Width / mViewport.Height
								   : 16.0F / 9.0F;
	const float FOV_Y = 70.0F * (std::numbers::pi_v<float> / 180.0F);

	mCamera = std::make_unique<OrbitCamera>(Vector3(0.0F, 0.0F, 0.0F), 20.0F, FOV_Y, ASPECT_RATIO,
											0.1F, 100.0F);

	mScene = std::make_unique<Scene>();

	// Hardcoded test assets removed — models are now loaded via
	// drag-and-drop or File > Load Model at runtime.

	mLogger->info("Scene has {} entities", mScene->GetEntities().size());

	DescriptorHandle lightHandle = mTextureHeap.Alloc(1);
	mLightBuffer->CreateSRV(lightHandle.GetCpuHandle());
	mLightBuffer->SetSRVHandles(lightHandle.GetCpuHandle(), lightHandle.GetGpuHandle());

#ifdef ENABLE_BINDLESS
	mLogger->info("Textures using bindless heap with {} descriptors",
				  Graphics::gBindlessAllocator->GetHeapSize());
#else
	mMaterialCBVStart = mTextureHeap.Alloc(MAX_MATERIALS);

	const uint32_t MATERIAL_BUFFER_ALIGNMENT = (sizeof(MaterialConstants) + 255U) & ~255U;
	for (uint32_t i = 0; i < MAX_MATERIALS; ++i)
	{
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
		cbvDesc.BufferLocation =
			mMaterialUploadBuffer.GetGpuVirtualAddress() +
			(static_cast<D3D12_GPU_VIRTUAL_ADDRESS>(i * MATERIAL_BUFFER_ALIGNMENT));
		cbvDesc.SizeInBytes = MATERIAL_BUFFER_ALIGNMENT;

		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = mMaterialCBVStart.GetCpuHandle();
		cpuHandle.ptr +=
			static_cast<SIZE_T>(i * Graphics::gDevice->GetDescriptorHandleIncrementSize(
										D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
		Graphics::gDevice->CreateConstantBufferView(&cbvDesc, cpuHandle);
	}

	// Allocate space for material texture SRVs
	// Recall 4 srv, albedo, normal, mellatic, roughness
	mMaterialTextureSRVStart = mTextureHeap.Alloc(MAX_MATERIALS * 4);

	// Allocate 5 descriptors for GBuffer textures
	// Recall Albedo/AO, Normal/Rough, Metallic, Emissive, Depth
	mGBufferSRVStart = mTextureHeap.Alloc(5);
#endif

	mSamplerHandle = mSamplerHeap.Alloc(1);
	D3D12_SAMPLER_DESC samplerDesc = {};
	samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc.MipLODBias = 0;
	samplerDesc.MaxAnisotropy = 1;
	samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	samplerDesc.MinLOD = 0.0F;
	samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
	Graphics::gDevice->CreateSampler(&samplerDesc, mSamplerHandle.GetCpuHandle());

	mViewportTexture = std::make_unique<ColorBuffer>();
	mViewportTexture->Create(L"ViewportTexture", mViewportWidth, mViewportHeight, 1,
							 DXGI_FORMAT_R8G8B8A8_UNORM, true);

	mPostProcessor = std::make_unique<PostProcessor>();
	mPostProcessor->Initialize(mViewportWidth, mViewportHeight, &mTextureHeap);

	// Depth buffer for viewport
	mViewportDepth = std::make_unique<DepthBuffer>();
	mViewportDepth->Create(L"ViewportDepth", mViewportWidth, mViewportHeight,
						   DXGI_FORMAT_D32_FLOAT);
	mViewportDepth->CreateView(Graphics::gDevice);

	// GBuffer for deferred rendering
	mGBuffer = std::make_unique<GBuffer>();
	mGBuffer->Create(mViewportWidth, mViewportHeight);
	mLogger->info("GBuffer created: {}x{}", mViewportWidth, mViewportHeight);

	// G BUFFER SRV
#ifdef ENABLE_BINDLESS
	// Create SRVs in bindless heap for GBuffer textures
	// These are needed for the lighting pass to sample from GBuffer
	mGBuffer->GetRenderTarget0().CreateSRV({});
	mGBuffer->GetRenderTarget1().CreateSRV({});
	mGBuffer->GetRenderTarget2().CreateSRV({});
	mGBuffer->GetRenderTarget3().CreateSRV({});
	mGBuffer->GetDepthBuffer().CreateSRV({});
#else
	// Create SRVs for GBuffer textures lighting pass sampling
	UINT descriptorSize =
		Graphics::gDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = mGBufferSRVStart.GetCpuHandle();

	mGBuffer->GetRenderTarget0().CreateSRV(srvHandle);
	srvHandle.ptr += descriptorSize;

	mGBuffer->GetRenderTarget1().CreateSRV(srvHandle);
	srvHandle.ptr += descriptorSize;

	mGBuffer->GetRenderTarget2().CreateSRV(srvHandle);
	srvHandle.ptr += descriptorSize;

	mGBuffer->GetRenderTarget3().CreateSRV(srvHandle);
	srvHandle.ptr += descriptorSize;

	mGBuffer->GetDepthBuffer().CreateSRV(srvHandle);
#endif

	mViewportSRV = uiSystem->AllocateDescriptor(1);
	mViewportTexture->CreateSRV(mViewportSRV.GetCpuHandle());

#ifdef ENABLE_BINDLESS
	mViewportTexture->CreateSRV({});
	mViewportTexture->CreateUAV({});
#endif

	mLogger->info("Viewport offscreen texture created: {}x{}", mViewportWidth, mViewportHeight);
}

void Renderer::Update(float deltaTime)
{
	// Delete any descriptor allocations form previous frames
	uint64_t completedFence =
		Graphics::gCommandListManager->GetGraphicsQueue().GetCompletedFenceValue();
	Graphics::gBindlessAllocator->ProcessDeletions(completedFence);

	mCamera->Update(deltaTime);

	auto model = Matrix4::identity();
	auto view = mCamera->GetViewMatrix();
	auto projection = mCamera->GetProjectionMatrix();

	// Calculate inverse view projection for world position for deferred
	Matrix4 viewProj = projection * view;
	mLightingConstants.invViewProj = inverse(viewProj);
	mLightingConstants.eyePosition = Float3(mCamera->GetPosition());

	mConstants.wvp = projection * view * model;
	mConstants.world = model;

	Matrix4 modelInverse = inverse(model);
	mConstants.worldInvTrans = transpose(modelInverse);

	if (!mSpotLights.empty())
	{
		mLightBuffer->Upload(mSpotLights.data(), mSpotLights.size() * sizeof(SpotLight));
	}
}

void Renderer::Render(Graphics::GraphicsContext& context)
{
	std::array<DXGI_FORMAT, 4> rtFormats = {//
											// RT0: Albedo/AO
											DXGI_FORMAT_R8G8B8A8_UNORM,
											// RT1: Normal/Roughness
											DXGI_FORMAT_R16G16B16A16_FLOAT,
											// RT2: Metallic/Flags
											DXGI_FORMAT_R8G8B8A8_UNORM,
											// RT3: Emissive
											DXGI_FORMAT_R16G16B16A16_FLOAT};

	context.Begin();

#ifdef USE_PIX
	PIXBeginEvent(context.GetCommandList(), PIX_COLOR_INDEX(0), L"Frame");
#endif

	// GEOMETRY PASS
	//
#ifdef USE_PIX
	PIXBeginEvent(context.GetCommandList(), PIX_COLOR_INDEX(1), L"Geometry Pass");
#endif

#ifdef ENABLE_BINDLESS
	ID3D12DescriptorHeap* bindlessHeap = Graphics::gBindlessAllocator->GetHeap();
	ID3D12DescriptorHeap* samplerHeap = mSamplerHeap.GetHeapPointer();

	if (!bindlessHeap)
	{
		mLogger->error("BINDLESS HEAP IS NULL!");
	}

	if (!samplerHeap)
	{
		mLogger->error("SAMPLER HEAP IS NULL!");
	}

	context.SetDescriptorHeaps(bindlessHeap, samplerHeap);
#endif

	context.SetShaderMRT("GeometryPass", rtFormats.data(), 4, DXGI_FORMAT_D32_FLOAT);

	context.BindGraphicsPipeline();

	context.TransitionResource(mGBuffer->GetRenderTarget0(), D3D12_RESOURCE_STATE_RENDER_TARGET);
	context.TransitionResource(mGBuffer->GetRenderTarget1(), D3D12_RESOURCE_STATE_RENDER_TARGET);
	context.TransitionResource(mGBuffer->GetRenderTarget2(), D3D12_RESOURCE_STATE_RENDER_TARGET);
	context.TransitionResource(mGBuffer->GetRenderTarget3(), D3D12_RESOURCE_STATE_RENDER_TARGET);
	context.TransitionResource(mGBuffer->GetDepthBuffer(), D3D12_RESOURCE_STATE_DEPTH_WRITE);

	mGBuffer->Clear(context);
	mGBuffer->SetAsRenderTargets(context);

	context.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

#ifndef ENABLE_BINDLESS
	context.SetDescriptorHeaps(mTextureHeap, mSamplerHeap);
#endif

	int entityCount = 0;
	const uint32_t CONSTANT_BUFFER_ALIGNMENT = (sizeof(Transform) + 255U) & ~255U;
	const uint32_t MATERIAL_BUFFER_ALIGNMENT = (sizeof(MaterialConstants) + 255U) & ~255U;

	for (const auto& entity : mScene->GetEntities())
	{
		if (!entity->IsVisible())
		{
			mLogger->debug("Entity '{}' is not visible", entity->GetName());
			continue;
		}

		auto mesh = entity->GetMesh();
		if (!mesh)
		{
			mLogger->debug("Entity '{}' has no mesh", entity->GetName());
			continue;
		}

		const Material& mat = entity->GetMaterial();

		Matrix4 world = entity->GetTransform().ToMatrix();
		Matrix4 view = mCamera->GetViewMatrix();
		Matrix4 proj = mCamera->GetProjectionMatrix();

		mConstants.wvp = proj * view * world;
		mConstants.world = world;
		mConstants.worldInvTrans = entity->GetTransform().ToInverseTransposeMatrix();

		MaterialConstants matConsts = mat.ToGPUConstants();

		uint32_t constantBufferOffset = static_cast<uint32_t>(entityCount) *
										CONSTANT_BUFFER_ALIGNMENT;
		uint32_t materialBufferOffset = static_cast<uint32_t>(entityCount) *
										MATERIAL_BUFFER_ALIGNMENT;
		mConstantUploadBuffer.Copy(&mConstants, sizeof(mConstants), constantBufferOffset);
		mMaterialUploadBuffer.Copy(&matConsts, sizeof(matConsts), materialBufferOffset);

		// Geometry pass bindings:
		// b0 transform
		// b1 material constants
		// t0-t3 textures
		// s0 Sampler
		context.SetConstantBuffer(0, mConstantUploadBuffer.GetGpuVirtualAddress() +
										 constantBufferOffset);
		context.SetConstantBuffer(1, mMaterialUploadBuffer.GetGpuVirtualAddress() +
										 materialBufferOffset);

#ifdef ENABLE_BINDLESS

		// Since vectormath does SIMD sizing, prefer to use the DirectX math libs here
		struct MaterialResources
		{
			DirectX::XMUINT2 mAlbedoTex;
			DirectX::XMUINT2 mNormalTex;
			DirectX::XMUINT2 mMetallicTex;
			DirectX::XMUINT2 mRoughnessTex;
		};

		MaterialResources resources = {};

		resources.mAlbedoTex.x = mat.mAlbedoTexture ? mat.mAlbedoTexture->GetSRVIndex() : 0;
		resources.mAlbedoTex.y = 0; // Sampler

		resources.mNormalTex.x = mat.mNormalTexture ? mat.mNormalTexture->GetSRVIndex() : 0;
		resources.mNormalTex.y = 0;

		resources.mMetallicTex.x = mat.mMetallicTexture ? mat.mMetallicTexture->GetSRVIndex() : 0;
		resources.mMetallicTex.y = 0;

		resources.mRoughnessTex.x = mat.mRoughnessTexture ? mat.mRoughnessTexture->GetSRVIndex()
														  : 0;
		resources.mRoughnessTex.y = 0;

		// Set as root constants (b2) - 8 uint32s, 32 bytes
		context.GetCommandList()->SetGraphicsRoot32BitConstants(2, 8, &resources, 0);
#else
		const uint32_t DESCRIPTOR_SIZE = Graphics::gDevice->GetDescriptorHandleIncrementSize(
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		// Using 4 srvs
		const uint32_t MATERIAL_TEXTURE_SRV_OFFSET = static_cast<UINT>(entityCount) * 4;

		D3D12_CPU_DESCRIPTOR_HANDLE destCPU = mMaterialTextureSRVStart.GetCpuHandle();
		destCPU.ptr += static_cast<SIZE_T>(MATERIAL_TEXTURE_SRV_OFFSET * DESCRIPTOR_SIZE);

		// Create SRVs for albedo, normal, metallic, roughness
		if (mat.mAlbedoTexture)
		{
			mat.mAlbedoTexture->CreateSRV(destCPU);
		}
		else
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC nullSrvDesc = {};
			nullSrvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			nullSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			nullSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			nullSrvDesc.Texture2D.MipLevels = 1;
			Graphics::gDevice->CreateShaderResourceView(nullptr, &nullSrvDesc, destCPU);
		}
		destCPU.ptr += DESCRIPTOR_SIZE;

		if (mat.mNormalTexture)
		{
			mat.mNormalTexture->CreateSRV(destCPU);
		}
		else
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC nullSrvDesc = {};
			nullSrvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			nullSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			nullSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			nullSrvDesc.Texture2D.MipLevels = 1;
			Graphics::gDevice->CreateShaderResourceView(nullptr, &nullSrvDesc, destCPU);
		}
		destCPU.ptr += DESCRIPTOR_SIZE;

		if (mat.mMetallicTexture)
		{
			mat.mMetallicTexture->CreateSRV(destCPU);
		}
		else
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC nullSrvDesc = {};
			nullSrvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			nullSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			nullSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			nullSrvDesc.Texture2D.MipLevels = 1;
			Graphics::gDevice->CreateShaderResourceView(nullptr, &nullSrvDesc, destCPU);
		}
		destCPU.ptr += DESCRIPTOR_SIZE;

		if (mat.mRoughnessTexture)
		{
			mat.mRoughnessTexture->CreateSRV(destCPU);
		}
		else
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC nullSrvDesc = {};
			nullSrvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			nullSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			nullSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			nullSrvDesc.Texture2D.MipLevels = 1;
			Graphics::gDevice->CreateShaderResourceView(nullptr, &nullSrvDesc, destCPU);
		}

		// Bind material texture descriptor table with the 4 consecutive SRV
		D3D12_GPU_DESCRIPTOR_HANDLE materialTextureSRVHandle =
			mMaterialTextureSRVStart.GetGpuHandle();
		materialTextureSRVHandle.ptr +=
			static_cast<UINT64>(MATERIAL_TEXTURE_SRV_OFFSET * DESCRIPTOR_SIZE);
		context.GetCommandList()->SetGraphicsRootDescriptorTable(2, materialTextureSRVHandle);
#endif

		D3D12_VERTEX_BUFFER_VIEW vbv = mesh->GetVertexBuffer().VertexBufferView(sizeof(Vertex));
		context.SetVertexBuffer(vbv);

		D3D12_INDEX_BUFFER_VIEW ibv = mesh->GetIndexBuffer().IndexBufferView();
		context.SetIndexBuffer(ibv);

		context.DrawIndexedInstanced(mesh->GetIndexCount());
		entityCount++;
	}

	// Transition for lighting pass next
	context.TransitionResource(mGBuffer->GetRenderTarget0(),
							   D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	context.TransitionResource(mGBuffer->GetRenderTarget1(),
							   D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	context.TransitionResource(mGBuffer->GetRenderTarget2(),
							   D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	context.TransitionResource(mGBuffer->GetRenderTarget3(),
							   D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	context.TransitionResource(mGBuffer->GetDepthBuffer(),
							   D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

#ifdef USE_PIX
	PIXEndEvent(context.GetCommandList());
#endif

	// LIGHTING PASS
#ifdef USE_PIX
	PIXBeginEvent(context.GetCommandList(), PIX_COLOR_INDEX(2), L"Lighting Pass");
#endif

	// Upload lighting constants (eye position, num lights, ambient light)
	mLightingUploadBuffer.Copy(&mLightingConstants, sizeof(LightingConstants), 0);

#ifdef ENABLE_BINDLESS
	context.SetDescriptorHeaps(Graphics::gBindlessAllocator->GetHeap(),
							   mSamplerHeap.GetHeapPointer());
#else
	context.SetDescriptorHeaps(mTextureHeap, mSamplerHeap);
#endif

	context.SetShader("LightingPass");

	// NOTE This probably should be done automatically but for right now manually
	// is fine.
	context.GetCommandList()->SetPipelineState(context.GetPipelineState());
	context.GetCommandList()->SetGraphicsRootSignature(context.GetRootSignature());

	// Root 0 Bind cbuffer, for lighting pass its just some eye, matrices, number of lights etc.
	context.SetConstantBuffer(0, mLightingUploadBuffer.GetGpuVirtualAddress());

// Root 1 Bind GBuffer textures
#ifdef ENABLE_BINDLESS
	{
		// Struct matches LightingPassBindless.slang's GBufferResources
		struct GBufferResources
		{
			DirectX::XMUINT2 mAlbedoAo;
			DirectX::XMUINT2 mNormalRoughness;
			DirectX::XMUINT2 mMetallicFlags;
			DirectX::XMUINT2 mEmissive;
			DirectX::XMUINT2 mDepth;
			DirectX::XMUINT2 mSpotLights;
		};

		GBufferResources gbuffer = {};
		gbuffer.mAlbedoAo.x = mGBuffer->GetRenderTarget0().GetSRVIndex();
		gbuffer.mAlbedoAo.y = 0;

		gbuffer.mNormalRoughness.x = mGBuffer->GetRenderTarget1().GetSRVIndex();
		gbuffer.mNormalRoughness.y = 0;

		gbuffer.mMetallicFlags.x = mGBuffer->GetRenderTarget2().GetSRVIndex();
		gbuffer.mMetallicFlags.y = 0;

		gbuffer.mEmissive.x = mGBuffer->GetRenderTarget3().GetSRVIndex();
		gbuffer.mEmissive.y = 0;

		gbuffer.mDepth.x = mGBuffer->GetDepthBuffer().GetSRVIndex();
		gbuffer.mDepth.y = 0;

		gbuffer.mSpotLights.x = mLightBuffer->GetSRVIndex();
		gbuffer.mSpotLights.y = 0;

		// Set as root constants (b1) - 12 uint32s, 48 bytes
		context.GetCommandList()->SetGraphicsRoot32BitConstants(1, 12, &gbuffer, 0);
	}
#else
	context.GetCommandList()->SetGraphicsRootDescriptorTable(1, mGBufferSRVStart.GetGpuHandle());

	// Root 2 Bind spotlight buffer
	context.GetCommandList()->SetGraphicsRootDescriptorTable(2, mLightBuffer->GetSRVGpu());
#endif

	// The mViewportTexture is our final render target for the imgui widget.
	context.TransitionResource(*mViewportTexture, D3D12_RESOURCE_STATE_RENDER_TARGET);
	context.SetRenderTarget(mViewportTexture->GetRTV(), mViewportDepth->GetDSV());

	context.ClearDepth(mViewportDepth->GetDSV(), 1.0F);

	context.SetViewport(0.0F, 0.0F, static_cast<float>(mViewportWidth),
						static_cast<float>(mViewportHeight));
	context.SetScissorRect(0, 0, mViewportWidth, mViewportHeight);

	// Draw fullscreen triangle, shader creates the quad.
	context.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context.DrawInstanced(3, 1);

	mPostProcessor->ApplyBlur(context, *mViewportTexture, &mSamplerHeap);

#ifdef USE_PIX
	PIXEndEvent(context.GetCommandList()); // Lighting Pass
#endif

#ifdef USE_PIX
	PIXEndEvent(context.GetCommandList()); // Frame
#endif
}

void Renderer::SetViewport(UINT width, UINT height)
{
	mViewport.TopLeftX = 0.0F;
	mViewport.TopLeftY = 0.0F;
	mViewport.Width = static_cast<float>(width);
	mViewport.Height = static_cast<float>(height);
	mViewport.MinDepth = 0.0F;
	mViewport.MaxDepth = 1.0F;

	mScissorRect.left = 0;
	mScissorRect.top = 0;
	mScissorRect.right = static_cast<LONG>(width);
	mScissorRect.bottom = static_cast<LONG>(height);

	if (mCamera && width > 0 && height > 0)
	{
		mCamera->SetAspectRatio(static_cast<float>(width) / static_cast<float>(height));
	}
}

void Renderer::AddSpotLight(const SpotLight& light)
{
	if (mSpotLights.size() >= MAX_SPOT_LIGHTS)
	{
		mLogger->error("Max number of lights ({}) reached", MAX_SPOT_LIGHTS);
		return;
	}

	mSpotLights.push_back(light);
	mLightingConstants.numActiveLights = static_cast<uint32_t>(mSpotLights.size());

	if (!mSpotLights.empty())
	{
		mLightBuffer->Upload(mSpotLights.data(), mSpotLights.size() * sizeof(SpotLight));
	}

	mLogger->info("Added spot light. Total lights: {}", mSpotLights.size());
}

void Renderer::LoadOBJ(const std::string& filePath)
{
	auto mesh = mAssetManager->LoadMesh(filePath);
	if (!mesh)
	{
		mLogger->error("Failed to load OBJ: {}", filePath);
		return;
	}

	std::filesystem::path path(filePath);
	std::string name = path.stem().string();

	mScene->AddEntity(name, mesh);
	mLogger->info("Loaded OBJ '{}' as entity '{}'", filePath, name);
}

void Renderer::ResizeViewport(uint32_t width, uint32_t height)
{
	if (width == mViewportWidth && height == mViewportHeight)
		return;

	if (width == 0 || height == 0)
		return;

	mViewportWidth = width;
	mViewportHeight = height;

	mViewportTexture.reset();
	mViewportDepth.reset();

	mViewportTexture = std::make_unique<ColorBuffer>();
	mViewportTexture->Create(L"ViewportTexture", mViewportWidth, mViewportHeight, 1,
							 DXGI_FORMAT_R8G8B8A8_UNORM, true);

	mViewportDepth = std::make_unique<DepthBuffer>();
	mViewportDepth->Create(L"ViewportDepth", mViewportWidth, mViewportHeight,
						   DXGI_FORMAT_D32_FLOAT);
	mViewportDepth->CreateView(Graphics::gDevice);

	mViewportTexture->CreateSRV(mViewportSRV.GetCpuHandle());

#ifdef ENABLE_BINDLESS
	mViewportTexture->CreateSRV({});
	mViewportTexture->CreateUAV({});
#endif

	mPostProcessor->Resize(mViewportWidth, mViewportHeight);

	if (mGBuffer)
	{
		mGBuffer->Resize(mViewportWidth, mViewportHeight);
		mLogger->info("GBuffer resized to: {}x{}", mViewportWidth, mViewportHeight);

#ifdef ENABLE_BINDLESS
		mGBuffer->GetRenderTarget0().CreateSRV({});
		mGBuffer->GetRenderTarget1().CreateSRV({});
		mGBuffer->GetRenderTarget2().CreateSRV({});
		mGBuffer->GetRenderTarget3().CreateSRV({});
		mGBuffer->GetDepthBuffer().CreateSRV({});
#else
		UINT descriptorSize = Graphics::gDevice->GetDescriptorHandleIncrementSize(
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = mGBufferSRVStart.GetCpuHandle();

		mGBuffer->GetRenderTarget0().CreateSRV(srvHandle);
		srvHandle.ptr += descriptorSize;

		mGBuffer->GetRenderTarget1().CreateSRV(srvHandle);
		srvHandle.ptr += descriptorSize;

		mGBuffer->GetRenderTarget2().CreateSRV(srvHandle);
		srvHandle.ptr += descriptorSize;

		mGBuffer->GetRenderTarget3().CreateSRV(srvHandle);
		srvHandle.ptr += descriptorSize;

		mGBuffer->GetDepthBuffer().CreateSRV(srvHandle);
#endif
	}

	SetViewport(width, height);

	mLogger->info("Viewport resized to: {}x{}", width, height);
}
