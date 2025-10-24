#include "Theme.h"
#include <imgui.h>
#include <fstream>

namespace UI
{
	// Custom application colors (not part of ImGui's standard color scheme)
	namespace AppColors
	{
		// Titlebar: Blue-gray to distinguish from main content area
		const ImVec4 TITLEBAR_BG = ImVec4(0.180F, 0.200F, 0.227F, 1.0F); // Blue-gray #2E333A
		const ImVec4 CLOSE_BUTTON_HOVER = ImVec4(0.8F, 0.0F, 0.0F, 1.0F); // Red on hover
		const ImVec4 CLOSE_BUTTON_ACTIVE = ImVec4(0.6F, 0.0F, 0.0F, 1.0F); // Darker red on click

		// Viewport placeholder (dark blue when no texture is loaded)
		const ImVec4 VIEWPORT_PLACEHOLDER = ImVec4(0.078F, 0.118F, 0.196F, 1.0F); // Dark blue (20, 30, 50)
	} // namespace AppColors
	void ApplyMaterialTheme()
	{
		ImGuiStyle& style = ImGui::GetStyle();
		ImVec4* colors = style.Colors;

		// Primary: Dark backgrounds
		// Accent: Brown #6F5235

		// #1B1B1B for main background
		const ImVec4 DARK_GRAY = ImVec4(0.106F, 0.106F, 0.106F, 1.00F);

		// #2E2E2E neutral dark gray
		const ImVec4 PRIMARY_DARK = ImVec4(0.18F, 0.18F, 0.18F, 1.00F);

		// #2E2E2E
		const ImVec4 PRIMARY_MID = ImVec4(0.18F, 0.18F, 0.18F, 1.00F);

		//  Lighter gray # 404040
		const ImVec4 PRIMARY_LIGHT = ImVec4(0.25F, 0.25F, 0.25F, 1.00F);

		// Brown #6F5235
		const ImVec4 ACCENT = ImVec4(0.435F, 0.322F, 0.208F, 1.00F);

		// Lighter brown
		const ImVec4 ACCENT_HOVER = ImVec4(0.535F, 0.422F, 0.308F, 1.00F);

		// Darker brown
		const ImVec4 ACCENT_ACTIVE = ImVec4(0.335F, 0.222F, 0.108F, 1.00F);

		// Brown border
		const ImVec4 BORDER = ImVec4(0.435F, 0.322F, 0.208F, 0.8F);

		const ImVec4 TEXT = ImVec4(0.95F, 0.95F, 0.95F, 1.00F);
		const ImVec4 TEXT_DISABLED = ImVec4(0.50F, 0.50F, 0.50F, 1.00F);

		// Window

		// #1B1B1B to match overall background (used by splitters!)
		colors[ImGuiCol_WindowBg] = DARK_GRAY;

		// #1B1B1B for child windows/widgets
		colors[ImGuiCol_ChildBg] = DARK_GRAY;

		// #1B1B1B for popups/menus
		colors[ImGuiCol_PopupBg] = DARK_GRAY;

		// Light gray borders For clear widget frames
		colors[ImGuiCol_Border] = ImVec4(0.7F, 0.7F, 0.7F, 1.0F);

		// Add subtle shadow to borders
		colors[ImGuiCol_BorderShadow] = ImVec4(0.0F, 0.0F, 0.0F, 0.5F);

		// Frame
		// #1B1B1B for input fields
		colors[ImGuiCol_FrameBg] = DARK_GRAY;

		// Slightly lighter on hover
		colors[ImGuiCol_FrameBgHovered] = ImVec4(0.15F, 0.15F, 0.15F, 1.00F);

		// Lighter when active
		colors[ImGuiCol_FrameBgActive] = ImVec4(0.2F, 0.2F, 0.2F, 1.00F);

		// Title bg
		// #1B1B1B title/tab bar
		colors[ImGuiCol_TitleBg] = DARK_GRAY;
		colors[ImGuiCol_TitleBgActive] = DARK_GRAY;
		colors[ImGuiCol_TitleBgCollapsed] = DARK_GRAY;

		// Menu
		// #1B1B1B menu bar
		colors[ImGuiCol_MenuBarBg] = DARK_GRAY;

		// Scrollbar
		// #1B1B1B for scrollbar track
		colors[ImGuiCol_ScrollbarBg] = DARK_GRAY;
		colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.30F, 0.30F, 0.35F, 1.00F);
		colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40F, 0.40F, 0.45F, 1.00F);
		colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.50F, 0.50F, 0.55F, 1.00F);

		// Checkbox
		colors[ImGuiCol_CheckMark] = ACCENT;

		// Slider
		colors[ImGuiCol_SliderGrab] = ACCENT;
		colors[ImGuiCol_SliderGrabActive] = ACCENT_ACTIVE;

		// Button
		colors[ImGuiCol_Button] = ACCENT;
		colors[ImGuiCol_ButtonHovered] = ACCENT_HOVER;
		colors[ImGuiCol_ButtonActive] = ACCENT_ACTIVE;

		// Header (collapsing header, tree node)
		//
		// Slightly lighter than #1B1B1B for visibilitu
		colors[ImGuiCol_Header] = ImVec4(0.15F, 0.15F, 0.15F, 1.00F);
		colors[ImGuiCol_HeaderHovered] = ACCENT;
		colors[ImGuiCol_HeaderActive] = ACCENT_ACTIVE;

		// Separator for widget separators, docking splitters are handled in ImGui source
		colors[ImGuiCol_Separator] = DARK_GRAY;
		colors[ImGuiCol_SeparatorHovered] = ACCENT;
		colors[ImGuiCol_SeparatorActive] = ACCENT_ACTIVE;

		// Resize grip, only shows on hover
		// Invisible
		colors[ImGuiCol_ResizeGrip] = ImVec4(0.0F, 0.0F, 0.0F, 0.0F);

		// Bright red on hover (TESTING)
		colors[ImGuiCol_ResizeGripHovered] = ImVec4(1.0F, 0.0F, 0.0F, 1.0F);

		// Darker red when active
		colors[ImGuiCol_ResizeGripActive] = ImVec4(0.6F, 0.0F, 0.0F, 1.0F);

		// Tab
		// Neutral gray For unselected tabs
		colors[ImGuiCol_Tab] = ImVec4(0.22F, 0.22F, 0.22F, 1.00F);
		colors[ImGuiCol_TabSelected] = ACCENT;
		colors[ImGuiCol_TabHovered] = ACCENT_HOVER;
		colors[ImGuiCol_TabSelectedOverline] = ImVec4(0.0F, 0.0F, 0.0F, 0.0F);

		// Dimmed
		// Neutral gray
		colors[ImGuiCol_TabDimmed] = ImVec4(0.22F, 0.22F, 0.22F, 1.00F);

		colors[ImGuiCol_TabDimmedSelected] = ACCENT;
		colors[ImGuiCol_TabDimmedSelectedOverline] = ImVec4(0.0F, 0.0F, 0.0F, 0.0F);

		// Old names
		// Brown for active tab
		colors[ImGuiCol_TabActive] = ACCENT;
		colors[ImGuiCol_TabUnfocused] = ImVec4(0.22F, 0.22F, 0.22F, 1.00F);
		colors[ImGuiCol_TabUnfocusedActive] = ACCENT;

		// Docking
		// Brown docking preview
		colors[ImGuiCol_DockingPreview] = ImVec4(0.435F, 0.322F, 0.208F, 0.50F);

		// #1B1B1B background for empty dockspace
		colors[ImGuiCol_DockingEmptyBg] = DARK_GRAY;

		// Text
		colors[ImGuiCol_Text] = TEXT;
		colors[ImGuiCol_TextDisabled] = TEXT_DISABLED;

		// Brown text selection
		colors[ImGuiCol_TextSelectedBg] = ImVec4(0.435F, 0.322F, 0.208F, 0.35F);

		// Table
		colors[ImGuiCol_TableHeaderBg] = DARK_GRAY;

		// Visible table borders
		colors[ImGuiCol_TableBorderStrong] = ImVec4(0.5F, 0.5F, 0.5F, 1.0F);

		// Lighter inner table borders
		colors[ImGuiCol_TableBorderLight] = ImVec4(0.4F, 0.4F, 0.4F, 1.0F);

		colors[ImGuiCol_TableRowBg] = ImVec4(0.00F, 0.00F, 0.00F, 0.00F);
		colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00F, 1.00F, 1.00F, 0.03F);

		// Style adjustments for clear rectangular frames

		// Less rounding for more rectangular appearance
		style.WindowRounding = 4.0F;

		// Less rounding for widget containers
		style.ChildRounding = 4.0F;

		// Subtle rounding on inputs
		style.FrameRounding = 3.0F;

		// Less rounding on popups
		style.PopupRounding = 4.0F;

		// Less rounding on scrollbars
		style.ScrollbarRounding = 4.0F;

		// Less rounding on grab handles
		style.GrabRounding = 3.0F;

		// Less rounding on tabs
		style.TabRounding = 4.0F;

		// Thick border around docked windows for clear frames
		style.WindowBorderSize = 2.0F;

		// Thick border around child windows for widget containers
		style.ChildBorderSize = 2.0F;

		// No frame borders to eliminate any separator lines
		style.FrameBorderSize = 0.0F;

		// Thick border around popup menus
		style.PopupBorderSize = 2.0F;

		// No border on individual tabs
		style.TabBorderSize = 0.0F;

		// No border on tab bar to remove brown line
		style.TabBarBorderSize = 0.0F;

		// Padding for UI spacing
		style.WindowPadding = ImVec2(12.0F, 12.0F);

		// Tab/button padding
		style.FramePadding = ImVec2(8.0F, 7.0F);
		style.ItemSpacing = ImVec2(8.0F, 6.0F);
		style.ItemInnerSpacing = ImVec2(6.0F, 4.0F);
		style.IndentSpacing = 20.0F;

		style.ScrollbarSize = 14.0F;
		style.GrabMinSize = 10.0F;

		// Docking separator size aka hitzone for drags
		// Medium separator size for better grabbing
		style.DockingSeparatorSize = 4.0F;

		// Hide the dropdown arrow in tab bars
		style.WindowMenuButtonPosition = ImGuiDir_None;

		// Anti-aliasing
		style.AntiAliasedLines = true;
		style.AntiAliasedFill = true;

		// Modest UI scaling
		style.ScaleAllSizes(1.05F);
	}

	void ApplyDefaultDarkTheme()
	{
		ImGui::StyleColorsDark();
	}

	void ApplyDefaultLightTheme()
	{
		ImGui::StyleColorsLight();
	}

	void LoadCustomFont()
	{
		ImGuiIO& io = ImGui::GetIO();

		ImFontConfig fontConfig;
		fontConfig.OversampleH = 3;
		fontConfig.OversampleV = 3;
		fontConfig.PixelSnapH = false;
		fontConfig.RasterizerMultiply = 1.2F;

		const char* fontPath = "assets/Roboto/static/Roboto-Regular.ttf";

		std::ifstream fontFile(fontPath);
		bool fontExists = fontFile.good();
		fontFile.close();

		if (fontExists)
		{
			ImFont* font = io.Fonts->AddFontFromFileTTF(fontPath, 16.0F, &fontConfig);

			if (font != nullptr)
			{
				io.FontDefault = font;
			}
			else
			{
				io.Fonts->AddFontDefault(&fontConfig);
			}
		}
		else
		{
			io.Fonts->AddFontDefault(&fontConfig);
		}

		io.FontGlobalScale = 1.0F;
	}

} // namespace UI
