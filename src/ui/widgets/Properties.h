#pragma once

#include <SDL3/SDL.h>
#include <imgui.h>
#include <functional>

// Forward declarations
struct SpotLight;

namespace UI
{

	/// Transform properties
	struct TransformProperties
	{
		float position[3];
		float rotation[3];
		float scale[3];
	};

	struct PropertiesCallbacks
	{
		std::function<void(const TransformProperties&)> onTransformChanged;
		std::function<void()> onSpotLightChanged;
		std::function<void(float)> onBlurIntensityChanged;
	};

	void ShowProperties(bool* pOpen, const char* selectedObjectName, TransformProperties& transform,
						const PropertiesCallbacks& callbacks, SpotLight* spotLight = nullptr,
						float* blurIntensity = nullptr);

} // namespace UI
