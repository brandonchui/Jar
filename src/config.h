#pragma once

#include <filesystem>
#include <string>
#include <optional>
#include "nlohmann/json.hpp"

struct ConfigSettings
{
	// Window settings
	uint32_t windowWidth = 1280;
	uint32_t windowHeight = 720;

	// Graphics settings
	uint32_t heapSize = 1000000; // Almost always going to use 1m descriptors

	// Asset paths
	std::filesystem::path assetPath = "assets";

	// Max limits
	uint32_t maxEntities = 10000;
	uint32_t maxMaterials = 1000;
	uint32_t maxLights = 100;

	// Serialize to JSON
	nlohmann::json ToJson() const;

	// Deserialize from JSON
	static ConfigSettings FromJson(const nlohmann::json& json);
};

class ConfigManager
{
public:
	ConfigManager();
	~ConfigManager();

	/// Load settings from AppData/Jar/settings.json
	bool Load();

	/// Save current settings to AppData/Jar/settings.json
	bool Save() const;

	/// Get the read only settings
	const ConfigSettings& GetSettings() const { return mSettings; }

	/// Get mutable settings
	ConfigSettings& GetMutableSettings() { return mSettings; }

	/// Path helper getters
	std::filesystem::path GetSettingsPath() const;
	std::filesystem::path GetAppDataPath() const;

private:
	ConfigSettings mSettings;
	std::filesystem::path mAppDataPath;

	void InitializeAppDataPath();
	bool EnsureDirectoryExists() const;
};
