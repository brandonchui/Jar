#pragma once

#include <imgui.h>

namespace UI
{
	/// NOTE: Before initializing imgui, we need to:
	/// - Apply theme
	/// - Then apply font

	/// Applys the custom theme.
	void ApplyMaterialTheme(float dpiScale = 1.0f);

	/// Applys the default dark theme.
	void ApplyDefaultDarkTheme();

	/// Applys the default light theme.
	void ApplyDefaultLightTheme();

	/// Load custom font.
	void LoadCustomFont(float dpiScale = 1.0f);

	/// Get bold font for titles
	ImFont* GetBoldFont();

	/// Get custom application colors
	namespace AppColors
	{
		extern const ImVec4 TITLEBAR_BG;
		extern const ImVec4 CLOSE_BUTTON_HOVER;
		extern const ImVec4 CLOSE_BUTTON_ACTIVE;

		extern const ImVec4 VIEWPORT_PLACEHOLDER;
	} // namespace AppColors

	/// Some default font sizes that are comfortable to read.
	const float REGULAR_FONT_SIZE = 16.0F;
	const float BOLD_FONT_SIZE = 18.0F;

} // namespace UI
