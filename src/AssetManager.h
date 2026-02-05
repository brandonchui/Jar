#pragma once

#include "Mesh.h"
#include "MaterialAsset.h"
#include "graphics/Texture.h"
#include "graphics/DescriptorHeap.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <spdlog/spdlog.h>

class AssetManager
{
public:
	AssetManager() = default;
	~AssetManager() = default;

	void Initialize(DescriptorHeap* textureHeap);

	std::shared_ptr<Mesh> LoadMesh(const std::string& objPath);
	std::shared_ptr<Texture> LoadTexture(const std::wstring& ddsPath);
	MaterialAsset LoadMaterialAsset(const std::string& materialName);

	Mesh* GetMesh(const std::string& path);
	Texture* GetTexture(const std::wstring& path);
	MaterialAsset* GetMaterial(const std::string& name);

private:
	void InitLogger();

	DescriptorHeap* mTextureHeap = nullptr;

	std::unordered_map<std::string, std::shared_ptr<Mesh>> mMeshCache;
	std::unordered_map<std::wstring, std::shared_ptr<Texture>> mTextureCache;
	std::unordered_map<std::string, MaterialAsset> mMaterialLibrary;

	std::shared_ptr<spdlog::logger> mLogger;
};
