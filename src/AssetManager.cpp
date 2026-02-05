#include "AssetManager.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include <fstream>
#include <nlohmann/json.hpp>

void AssetManager::InitLogger()
{
	if (!mLogger)
	{
		mLogger = spdlog::get("AssetManager");
		if (!mLogger)
		{
			mLogger = spdlog::stdout_color_mt("AssetManager");
			mLogger->set_pattern("[%H:%M:%S] [%^%l%$] [%n] %v");
			mLogger->set_level(spdlog::level::debug);
		}
	}
}

void AssetManager::Initialize(DescriptorHeap* textureHeap)
{
	InitLogger();
	mTextureHeap = textureHeap;
	mLogger->info("AssetManager initialized");
}

std::shared_ptr<Mesh> AssetManager::LoadMesh(const std::string& objPath)
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

	mLogger->info("Mesh loaded successfully");
	return mMeshCache[objPath] = mesh;
}

std::shared_ptr<Texture> AssetManager::LoadTexture(const std::wstring& ddsPath)
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

	if (mTextureHeap)
	{
		DescriptorHandle textureHandle = mTextureHeap->Alloc(1);
		texture->CreateSRV(textureHandle.GetCpuHandle());
		texture->SetSRVHandles(textureHandle.GetCpuHandle(), textureHandle.GetGpuHandle());
	}

	mLogger->info("Texture loaded successfully");
	return mTextureCache[ddsPath] = texture;
}

MaterialAsset AssetManager::LoadMaterialAsset(const std::string& materialName)
{
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

Mesh* AssetManager::GetMesh(const std::string& path)
{
	auto it = mMeshCache.find(path);
	return it != mMeshCache.end() ? it->second.get() : nullptr;
}

Texture* AssetManager::GetTexture(const std::wstring& path)
{
	auto it = mTextureCache.find(path);
	return it != mTextureCache.end() ? it->second.get() : nullptr;
}

MaterialAsset* AssetManager::GetMaterial(const std::string& name)
{
	auto it = mMaterialLibrary.find(name);
	return it != mMaterialLibrary.end() ? &it->second : nullptr;
}
