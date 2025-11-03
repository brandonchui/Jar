#include "Renderer.h"
#include "OrbitCamera.h"
#include "ui/UISystem.h"
#include "d3d12.h"
#include "graphics/CommandContext.h"
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

	static_assert(sizeof(LightingConstants) == 32, "LightingConstants size mismatch with Slang");
	static_assert(sizeof(SpotLight) == 64, "SpotLight size mismatch with Slang");

	// Initialize constant buffers.
	const uint32_t CONSTANT_BUFFER_SIZE = ((sizeof(Transform) + 255U) & ~255U) * 64;
	mConstantUploadBuffer.Initialize(CONSTANT_BUFFER_SIZE);

	const uint32_t MATERIAL_BUFFER_SIZE = ((sizeof(MaterialConstants) + 255U) & ~255U) * 64;
	mMaterialUploadBuffer.Initialize(MATERIAL_BUFFER_SIZE);

	// Clear the material buffer to prevent garbage data
	{
		MaterialConstants zeroMat = {};
		const uint32_t materialAlignment = (sizeof(MaterialConstants) + 255U) & ~255U;
		for (uint32_t i = 0; i < 64; ++i)
		{
			mMaterialUploadBuffer.Copy(&zeroMat, sizeof(MaterialConstants), i * materialAlignment);
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

	const float ASPECT_RATIO = (mViewport.Width > 0.0F && mViewport.Height > 0.0F)
								   ? mViewport.Width / mViewport.Height
								   : 16.0F / 9.0F;
	const float FOV_Y = 70.0F * (std::numbers::pi_v<float> / 180.0F);

	mCamera = std::make_unique<OrbitCamera>(Vector3(0.0F, 0.0F, 0.0F), 20.0F, FOV_Y, ASPECT_RATIO,
											0.1F, 100.0F);

	mScene = std::make_unique<Scene>();

	// ---
	//NOTE Due to remove as it is hard coded.
	// Loading materials
	auto mesh = LoadMesh("assets/ball.obj");
	MaterialAsset plasticMat = LoadMaterialAsset("green_plastic");
	MaterialAsset rustMat = LoadMaterialAsset("rust");
	MaterialAsset goldMat = LoadMaterialAsset("gold");
	MaterialAsset stoneMat = LoadMaterialAsset("stone");
	MaterialAsset woodMat = LoadMaterialAsset("wooden_gate");
	MaterialAsset brushedMetalMat = LoadMaterialAsset("brushed_metal");

	if (mesh)
	{
		const float SPACING = 20.0F;
		const float VERTICAL_SPACING = 20.0F;

		std::vector<MaterialAsset*> materials = {&plasticMat, &rustMat,	 &woodMat,
												 &goldMat,	  &stoneMat, &brushedMetalMat};
		std::vector<std::string> materialNames = {"green_plastic", "rust",	"wooden_gate",
												  "gold",		   "stone", "brushed_metal"};

		for (size_t i = 0; i < 6; ++i)
		{
			std::string name = "Sphere_" + materialNames[i];
			Entity* entity = mScene->AddEntity(name, mesh);

			if (entity)
			{
				size_t row = i / 3;
				size_t col = i % 3;

				float posX = (static_cast<float>(col) - 1) * SPACING;
				float posZ = static_cast<float>(row) * VERTICAL_SPACING;
				entity->GetTransform().position = Vector3(posX, 0.0F, posZ);

				MaterialAsset* mat = materials[i];
				entity->GetMaterial().mAlbedoTexture = mat->albedoTexture;
				entity->GetMaterial().mNormalTexture = mat->normalTexture;
				entity->GetMaterial().mMetallicTexture = mat->metallicTexture;
				entity->GetMaterial().mRoughnessTexture = mat->roughnessTexture;
				entity->GetMaterial().mAmbientOcclusionTexture = mat->aoTexture;
				entity->GetMaterial().mAlbedoColor = mat->albedoColor;
				entity->GetMaterial().mMetallicFactor = mat->metallicFactor;
				entity->GetMaterial().mRoughnessFactor = mat->roughnessFactor;
				entity->GetMaterial().mNormalStrength = mat->normalStrength;
				entity->GetMaterial().mAmbientOcclusionFactor = mat->aoStrength;
			}
		}
	}
	// ---

	mLogger->info("Scene has {} entities", mScene->GetEntities().size());

	DescriptorHandle lightHandle = mTextureHeap.Alloc(1);
	mLightBuffer->CreateSRV(lightHandle.GetCpuHandle());
	mLightBuffer->SetSRVHandles(lightHandle.GetCpuHandle(), lightHandle.GetGpuHandle());

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

	// Offscreen viewport render target
	mViewportTexture = std::make_unique<ColorBuffer>();
	mViewportTexture->Create(L"ViewportTexture", mViewportWidth, mViewportHeight, 1,
							 DXGI_FORMAT_R8G8B8A8_UNORM);

	// Depth buffer for viewport
	mViewportDepth = std::make_unique<DepthBuffer>();
	mViewportDepth->Create(L"ViewportDepth", mViewportWidth, mViewportHeight,
						   DXGI_FORMAT_D32_FLOAT);
	mViewportDepth->CreateView(Graphics::gDevice);

	// GBuffer for deferred rendering
	mGBuffer = std::make_unique<GBuffer>();
	mGBuffer->Create(mViewportWidth, mViewportHeight);
	mLogger->info("GBuffer created: {}x{}", mViewportWidth, mViewportHeight);

	// SRV for ImGui to sample the viewport texture
	// NOTE: Allocate from ImGui's heap so it can reference it when rendering
	mViewportSRV = uiSystem->AllocateDescriptor(1);
	mViewportTexture->CreateSRV(mViewportSRV.GetCpuHandle());

	mLogger->info("Viewport offscreen texture created: {}x{}", mViewportWidth, mViewportHeight);
}

void Renderer::Update(float deltaTime)
{
	mCamera->Update(deltaTime);

	auto model = Matrix4::identity();
	auto view = mCamera->GetViewMatrix();
	auto projection = mCamera->GetProjectionMatrix();

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
	DXGI_FORMAT rtFormats[4] = {//
								// RT0: Albedo/AO
								DXGI_FORMAT_R8G8B8A8_UNORM,
								// RT1: Normal/Roughness
								DXGI_FORMAT_R16G16B16A16_FLOAT,
								// RT2: Metallic/Flags
								DXGI_FORMAT_R8G8B8A8_UNORM,
								// RT3: Emissive
								DXGI_FORMAT_R16G16B16A16_FLOAT};

	context.SetShaderMRT("GeometryPass", rtFormats, 4, DXGI_FORMAT_D32_FLOAT);

	context.Begin();

#ifdef USE_PIX
	PIXBeginEvent(context.GetCommandList(), PIX_COLOR_INDEX(0), L"Frame");
#endif

	// GEOMETRY PASS
#ifdef USE_PIX
	PIXBeginEvent(context.GetCommandList(), PIX_COLOR_INDEX(1), L"Geometry Pass");
#endif

	context.TransitionResource(mGBuffer->GetRenderTarget0(), D3D12_RESOURCE_STATE_RENDER_TARGET);
	context.TransitionResource(mGBuffer->GetRenderTarget1(), D3D12_RESOURCE_STATE_RENDER_TARGET);
	context.TransitionResource(mGBuffer->GetRenderTarget2(), D3D12_RESOURCE_STATE_RENDER_TARGET);
	context.TransitionResource(mGBuffer->GetRenderTarget3(), D3D12_RESOURCE_STATE_RENDER_TARGET);
	context.TransitionResource(mGBuffer->GetDepthBuffer(), D3D12_RESOURCE_STATE_DEPTH_WRITE);

	mGBuffer->Clear(context);
	mGBuffer->SetAsRenderTargets(context);

	context.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	ID3D12DescriptorHeap* heaps[] = {mTextureHeap.GetHeapPointer(), mSamplerHeap.GetHeapPointer()};
	context.GetCommandList()->SetDescriptorHeaps(2, heaps);

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
	context.TransitionResource(mGBuffer->GetDepthBuffer(), D3D12_RESOURCE_STATE_DEPTH_READ);

#ifdef USE_PIX
	PIXEndEvent(context.GetCommandList());
#endif

// LIGHTING PASS
#ifdef USE_PIX
	PIXBeginEvent(context.GetCommandList(), PIX_COLOR_INDEX(2), L"Lighting Pass (TODO)");
#endif

	context.TransitionResource(*mViewportTexture, D3D12_RESOURCE_STATE_RENDER_TARGET);

	// LIGHTING PASS WIP - just render clear color for now
	float clearColor[4] = {0.0F, 0.0F, 0.0F, 1.0F};
	context.ClearColor(mViewportTexture->GetRTV(), clearColor);
	context.TransitionResource(*mViewportTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

#ifdef USE_PIX
	PIXEndEvent(context.GetCommandList());
#endif

#ifdef USE_PIX
	PIXEndEvent(context.GetCommandList()); // End Frame
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

std::shared_ptr<Mesh> Renderer::LoadMesh(const std::string& objPath)
{
	auto it = mMeshCache.find(objPath);
	if (it != mMeshCache.end())
	{
		mLogger->info("Using cached mesh: {}", objPath);
		return it->second;
	}

	mLogger->info("Loading mesh: {}", objPath);

	auto mesh = std::make_shared<Mesh>();

	if (!mesh->LoadFromOBJ(objPath))
	{
		mLogger->error("Failed to load mesh");
		return nullptr;
	}

	mesh->UploadToGPU();

	mMeshCache[objPath] = mesh;

	mLogger->info("Mesh loaded successfully");
	return mesh;
}

std::shared_ptr<Texture> Renderer::LoadTexture(const std::wstring& ddsPath)
{
	auto it = mTextureCache.find(ddsPath);
	if (it != mTextureCache.end())
	{
		mLogger->info("Using cached texture: {}", std::string(ddsPath.begin(), ddsPath.end()));
		return it->second;
	}

	mLogger->info("Loading texture: {}", std::string(ddsPath.begin(), ddsPath.end()));

	auto texture = std::make_shared<Texture>();

	if (!texture->LoadFromFile(ddsPath))
	{
		mLogger->error("Failed to load texture");
		return nullptr;
	}

	texture->UploadToGPU();

	DescriptorHandle textureHandle = mTextureHeap.Alloc(1);
	texture->CreateSRV(textureHandle.GetCpuHandle());
	texture->SetSRVHandles(textureHandle.GetCpuHandle(), textureHandle.GetGpuHandle());

	mTextureCache[ddsPath] = texture;

	mLogger->info("Texture loaded successfully");
	return texture;
}

MaterialAsset Renderer::LoadMaterialAsset(const std::string& materialName)
{
	// NOTE This is horribly hard coded in the asset folder.
	auto it = mMaterialLibrary.find(materialName);
	if (it != mMaterialLibrary.end())
	{
		mLogger->info("Using cached material: {}", materialName);
		return it->second;
	}

	mLogger->info("Loading material: {}", materialName);

	MaterialAsset mat;
	mat.name = materialName;

	std::string jsonPath = "assets/materials/" + materialName + "/material.json";
	std::ifstream file(jsonPath);

	if (!file.is_open())
	{
		mLogger->error("Failed to open material file: {}", jsonPath);
		return mat;
	}

	try
	{
		nlohmann::json j;
		file >> j;

		std::string basePath = "assets/materials/" + materialName + "/";

		if (j.contains("albedo") && !j["albedo"].get<std::string>().empty())
		{
			std::string albedoPath = j["albedo"].get<std::string>();
			mat.albedoTexture = LoadTexture(std::wstring(albedoPath.begin(), albedoPath.end()));
		}

		if (j.contains("normal") && !j["normal"].get<std::string>().empty())
		{
			std::string normalPath = j["normal"].get<std::string>();
			mat.normalTexture = LoadTexture(std::wstring(normalPath.begin(), normalPath.end()));
		}

		if (j.contains("metallic") && !j["metallic"].get<std::string>().empty())
		{
			std::string metallicPath = j["metallic"].get<std::string>();
			mat.metallicTexture =
				LoadTexture(std::wstring(metallicPath.begin(), metallicPath.end()));
		}

		if (j.contains("roughness") && !j["roughness"].get<std::string>().empty())
		{
			std::string roughnessPath = j["roughness"].get<std::string>();
			mat.roughnessTexture =
				LoadTexture(std::wstring(roughnessPath.begin(), roughnessPath.end()));
		}

		if (j.contains("ao") && !j["ao"].get<std::string>().empty())
		{
			std::string aoPath = j["ao"].get<std::string>();
			mat.aoTexture = LoadTexture(std::wstring(aoPath.begin(), aoPath.end()));
		}

		if (j.contains("emissive") && !j["emissive"].get<std::string>().empty())
		{
			std::string emissivePath = j["emissive"].get<std::string>();
			mat.emissiveTexture =
				LoadTexture(std::wstring(emissivePath.begin(), emissivePath.end()));
		}

		if (j.contains("albedoColor") && j["albedoColor"].is_array() &&
			j["albedoColor"].size() >= 3)
		{
			mat.albedoColor =
				Vector4(j["albedoColor"][0].get<float>(), j["albedoColor"][1].get<float>(),
						j["albedoColor"][2].get<float>(),
						j["albedoColor"].size() >= 4 ? j["albedoColor"][3].get<float>() : 1.0F);
		}

		if (j.contains("emissiveFactor") && j["emissiveFactor"].is_array() &&
			j["emissiveFactor"].size() >= 3)
		{
			mat.emissiveFactor = Vector3(j["emissiveFactor"][0].get<float>(),
										 j["emissiveFactor"][1].get<float>(),
										 j["emissiveFactor"][2].get<float>());
		}

		if (j.contains("metallicFactor"))
			mat.metallicFactor = j["metallicFactor"].get<float>();

		if (j.contains("roughnessFactor"))
			mat.roughnessFactor = j["roughnessFactor"].get<float>();

		if (j.contains("normalStrength"))
			mat.normalStrength = j["normalStrength"].get<float>();

		if (j.contains("aoStrength"))
			mat.aoStrength = j["aoStrength"].get<float>();

		mMaterialLibrary[materialName] = mat;
		mLogger->info("Material '{}' loaded successfully", materialName);
	}
	catch (const nlohmann::json::exception& e)
	{
		mLogger->error("Failed to parse material JSON: {}", e.what());
	}

	return mat;
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

void Renderer::ResizeViewport(uint32_t width, uint32_t height)
{
	if (width == mViewportWidth && height == mViewportHeight)
		return;

	if (width == 0 || height == 0)
		return;

	mViewportWidth = width;
	mViewportHeight = height;

	// Resetting the ColorBuffers since we have to recreate a new one.
	mViewportTexture.reset();
	mViewportDepth.reset();

	mViewportTexture = std::make_unique<ColorBuffer>();
	mViewportTexture->Create(L"ViewportTexture", mViewportWidth, mViewportHeight, 1,
							 DXGI_FORMAT_R8G8B8A8_UNORM);

	mViewportDepth = std::make_unique<DepthBuffer>();
	mViewportDepth->Create(L"ViewportDepth", mViewportWidth, mViewportHeight,
						   DXGI_FORMAT_D32_FLOAT);
	mViewportDepth->CreateView(Graphics::gDevice);

	mViewportTexture->CreateSRV(mViewportSRV.GetCpuHandle());

	if (mGBuffer)
	{
		mGBuffer->Resize(mViewportWidth, mViewportHeight);
		mLogger->info("GBuffer resized to: {}x{}", mViewportWidth, mViewportHeight);
	}

	SetViewport(width, height);

	mLogger->info("Viewport resized to: {}x{}", width, height);
}
