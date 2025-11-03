#pragma once
#include "Scene.h"
#include "MaterialAsset.h"
#include "graphics/Constants.h"
#include "graphics/UploadBuffer.h"
#include "graphics/StructuredBuffer.h"
#include "graphics/ColorBuffer.h"
#include "graphics/DepthBuffer.h"
#include "graphics/Core.h"
#include "graphics/Texture.h"
#include "graphics/DescriptorHeap.h"
#include "graphics/GBuffer.h"
#include "Mesh.h"
#include "Lighting.h"
#include "ICamera.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <spdlog/spdlog.h>

namespace Graphics
{
	class GraphicsContext;
}

class UISystem;

/*
TODO
[ ] Deferred
[ ] Clustered
[x] Create Scene
		- only supports a single mesh and texture for rendering
[ ] Move lighting system
*/

/// THe Renderer holds the data that is needed to complete
/// a frame.
/// - High level: Mesh, Texture, Lighting
/// - Low level: Samplers, UploadBuffers
class Renderer
{
public:
	Renderer() = default;
	~Renderer() = default;

	/// Initializes:
	/// - UploadBuffers for cbuffer
	/// - Lighting cbuffer
	/// - Load the single the mesh/texture
	/// NOTE Not sure if I like calling the Initialize with a
	/// UI system parameter, but it does need the SRV so not
	/// sure on a workaround yet.
	void Initialize(UISystem* uiSystem);

	/// Updates constants in preperation before the Render().
	void Update(float deltaTime);

	/// The commands for each rendered frame go in Render(), but
	/// it *NOT* present. It will just fill the command list
	/// for the provided GraphicsContext paramter.
	void Render(Graphics::GraphicsContext& context);

	void SetViewport(UINT width, UINT height);

	std::shared_ptr<Mesh> LoadMesh(const std::string& objPath);
	std::shared_ptr<Texture> LoadTexture(const std::wstring& ddsPath);

	/// The JSON loader for some material that we defined as the
	/// material .json metadata (in Assets/Material folder).
	MaterialAsset LoadMaterialAsset(const std::string& materialName);

	void AddSpotLight(const SpotLight& light);

	Scene* GetScene() const { return mScene.get(); }

	ICamera* GetCamera() const { return mCamera.get(); }

	/// Access the single spotlight at index 0 for now, mainly using
	/// for the UI so I can move the spotlight interactively.
	/// NOTE Will remove soon.
	SpotLight* GetSpotLight() { return mSpotLights.empty() ? nullptr : &mSpotLights[0]; }
	void UpdateSpotLight()
	{
		if (!mSpotLights.empty())
			mLightBuffer->Upload(mSpotLights.data(), mSpotLights.size() * sizeof(SpotLight));
	}

	D3D12_GPU_DESCRIPTOR_HANDLE GetViewportSRV() const { return mViewportSRV.GetGpuHandle(); }

	/// ResizeViewport recreates the offscreen render target to match
	/// the ImGui viewport widget's size. The 3d scene is rendered to
	/// this texture at its native resolution (no scaling), then ImGui
	/// displays it 1:1 in the viewport widget wherever it's docked.
	void ResizeViewport(uint32_t width, uint32_t height);

private:
	void InitLogger();

	std::unique_ptr<Scene> mScene;
	std::unordered_map<std::string, std::shared_ptr<Mesh>> mMeshCache;
	std::unordered_map<std::wstring, std::shared_ptr<Texture>> mTextureCache;
	std::unordered_map<std::string, MaterialAsset> mMaterialLibrary;

	DescriptorHeap mTextureHeap;
	DescriptorHeap mSamplerHeap;
	DescriptorHandle mSamplerHandle;

	UploadBuffer mConstantUploadBuffer;
	UploadBuffer mMaterialUploadBuffer;
	UploadBuffer mLightingUploadBuffer;

	Transform mConstants;

	static constexpr uint32_t MAX_MATERIALS = 64;
	DescriptorHandle mMaterialCBVStart;

	// 4 SRVs per entity - albedo, normal, metallic, roughness
	DescriptorHandle mMaterialTextureSRVStart;

	std::vector<SpotLight> mSpotLights;
	std::unique_ptr<Graphics::StructuredBuffer> mLightBuffer;

	LightingConstants mLightingConstants;

	D3D12_VIEWPORT mViewport{};
	D3D12_RECT mScissorRect{};

	std::unique_ptr<ICamera> mCamera;

	/// Viewport offscreen rendering.
	std::unique_ptr<ColorBuffer> mViewportTexture;
	std::unique_ptr<DepthBuffer> mViewportDepth;
	DescriptorHandle mViewportSRV;

	std::unique_ptr<GBuffer> mGBuffer;

	/// Probably misleading naming, the this viewport width and height
	/// are for the actual texture generated for the viewport widget.
	/// Will get overriden anyway immediately.
	uint32_t mViewportWidth = 1280;
	uint32_t mViewportHeight = 800;

	std::shared_ptr<spdlog::logger> mLogger;
};
