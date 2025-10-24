#pragma once

#include <imgui.h>

namespace UI
{
	/// NOTE: Before initializing imgui, we need to:
	/// - Apply theme
	/// - Then apply font

	/// Applys the custom theme.
	void ApplyMaterialTheme();

	/// Applys the default dark theme.
	void ApplyDefaultDarkTheme();

	/// Applys the default light theme.
	void ApplyDefaultLightTheme();

	/// Load custom font.
	void LoadCustomFont();

	/// Get custom application colors
	namespace AppColors
	{
		extern const ImVec4 TITLEBAR_BG;
		extern const ImVec4 CLOSE_BUTTON_HOVER;
		extern const ImVec4 CLOSE_BUTTON_ACTIVE;

		extern const ImVec4 VIEWPORT_PLACEHOLDER;
	} // namespace AppColors

} // namespace UI
