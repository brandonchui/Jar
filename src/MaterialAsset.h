#pragma once
#include <string>
#include <memory>
#include "vectormath.hpp"
#include "Lighting.h"

class Texture;

/// For use in the Renderer to get the data needed for
/// some material, e.g. LoadMaterialAsset(std::string s)
struct MaterialAsset
{
	std::string name;

	std::shared_ptr<Texture> albedoTexture;
	std::shared_ptr<Texture> normalTexture;
	std::shared_ptr<Texture> metallicTexture;
	std::shared_ptr<Texture> roughnessTexture;
	std::shared_ptr<Texture> aoTexture;
	std::shared_ptr<Texture> emissiveTexture;

	Vector4 albedoColor = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	Float3 emissiveFactor = Float3(0.0f, 0.0f, 0.0f);
	float metallicFactor = 0.0f;
	float roughnessFactor = 1.0f;
	float normalStrength = 1.0f;
	float aoStrength = 1.0f;
};
