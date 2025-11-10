#include "Config.h"
#include <fstream>
#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <Windows.h>
#include <shlobj.h>
#endif

using json = nlohmann::json;

// Serialize
nlohmann::json ConfigSettings::ToJson() const
{
	json j;
	j["windowWidth"] = windowWidth;
	j["windowHeight"] = windowHeight;
	j["heapSize"] = heapSize;
	j["assetPath"] = assetPath.string();
	j["maxEntities"] = maxEntities;
	j["maxMaterials"] = maxMaterials;
	j["maxLights"] = maxLights;
	return j;
}

// Deserialize
ConfigSettings ConfigSettings::FromJson(const nlohmann::json& json)
{
	ConfigSettings settings;

	if (json.contains("windowWidth"))
		settings.windowWidth = json["windowWidth"].get<uint32_t>();
	if (json.contains("windowHeight"))
		settings.windowHeight = json["windowHeight"].get<uint32_t>();
	if (json.contains("heapSize"))
		settings.heapSize = json["heapSize"].get<uint32_t>();
	if (json.contains("assetPath"))
		settings.assetPath = json["assetPath"].get<std::string>();
	if (json.contains("maxEntities"))
		settings.maxEntities = json["maxEntities"].get<uint32_t>();
	if (json.contains("maxMaterials"))
		settings.maxMaterials = json["maxMaterials"].get<uint32_t>();
	if (json.contains("maxLights"))
		settings.maxLights = json["maxLights"].get<uint32_t>();

	return settings;
}

ConfigManager::ConfigManager()
{
	InitializeAppDataPath();
}

ConfigManager::~ConfigManager() = default;

void ConfigManager::InitializeAppDataPath()
{
#ifdef _WIN32
	// using %APPDATA% on Windows
	char* appDataEnv = nullptr;
	size_t len = 0;
	errno_t err = _dupenv_s(&appDataEnv, &len, "APPDATA");

	if (err == 0 && appDataEnv != nullptr)
	{
		mAppDataPath = std::filesystem::path(appDataEnv) / "Jar";
		free(appDataEnv);
	}
	else
	{
		// Backup if for whatever reason the API fails
		char path[MAX_PATH];
		if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, path)))
		{
			mAppDataPath = std::filesystem::path(path) / "Jar";
		}
		else
		{
			// All else fails, so just create it in current directory
			spdlog::warn("Failed to get AppData path, using current directory");
			mAppDataPath = std::filesystem::current_path() / "config";
		}
	}
#else
	// MacOS/Linux support?
#endif

	spdlog::info("AppData path: {}", mAppDataPath.string());
}

bool ConfigManager::EnsureDirectoryExists() const
{
	try
	{
		if (!std::filesystem::exists(mAppDataPath))
		{
			std::filesystem::create_directories(mAppDataPath);
			spdlog::info("Created AppData directory: {}", mAppDataPath.string());
		}
		return true;
	}
	catch (const std::filesystem::filesystem_error& e)
	{
		spdlog::error("Failed to create AppData directory: {}", e.what());
		return false;
	}
}

std::filesystem::path ConfigManager::GetAppDataPath() const
{
	return mAppDataPath;
}

std::filesystem::path ConfigManager::GetSettingsPath() const
{
	return mAppDataPath / "settings.json";
}

bool ConfigManager::Load()
{
	std::filesystem::path settingsPath = GetSettingsPath();

	if (!std::filesystem::exists(settingsPath))
	{
		spdlog::info("Settings file not found, using defaults: {}", settingsPath.string());
		return Save();
	}

	// Try to load the file
	try
	{
		std::ifstream file(settingsPath);
		if (!file.is_open())
		{
			spdlog::error("Failed to open settings file: {}", settingsPath.string());
			return false;
		}

		json j;
		file >> j;
		mSettings = ConfigSettings::FromJson(j);

		spdlog::info("Settings loaded successfully from: {}", settingsPath.string());
		spdlog::info("  Window size: {}x{}", mSettings.windowWidth, mSettings.windowHeight);
		return true;
	}
	catch (const json::exception& e)
	{
		spdlog::error("Failed to parse settings JSON: {}", e.what());
		return false;
	}
	catch (const std::exception& e)
	{
		spdlog::error("Failed to load settings: {}", e.what());
		return false;
	}
}

bool ConfigManager::Save() const
{
	if (!EnsureDirectoryExists())
	{
		return false;
	}

	std::filesystem::path settingsPath = GetSettingsPath();

	try
	{
		std::ofstream file(settingsPath);
		if (!file.is_open())
		{
			spdlog::error("Failed to create settings file: {}", settingsPath.string());
			return false;
		}

		json j = mSettings.ToJson();
		file << j.dump(4);

		spdlog::info("Settings saved successfully to: {}", settingsPath.string());
		return true;
	}
	catch (const std::exception& e)
	{
		spdlog::error("Failed to save settings: {}", e.what());
		return false;
	}
}
