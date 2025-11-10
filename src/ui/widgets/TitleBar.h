#pragma once

#include <SDL3/SDL.h>
#include <imgui.h>

namespace UI
{

	enum class TitleBarAction
	{
		None,
		Close,
		OpenPreferences
	};

	struct TitleBarState
	{
		TitleBarAction action;
		bool isDragging;
		bool isResizing;
	};

	/// Show custom title bar with window dragging and resizing, must be the first
	/// widget that gets called.
	TitleBarState ShowTitleBar(SDL_Window* window, const char* title, bool& isDragging,
							   ImVec2& dragOffset, bool& isResizing, int& resizeEdge,
							   ImVec2& resizeStartMousePos, ImVec2& resizeStartWindowPos,
							   ImVec2& resizeStartWindowSize, SDL_Cursor* cursorDefault,
							   SDL_Cursor* cursorNwse, SDL_Cursor* cursorNesw, SDL_Cursor* cursorWe,
							   SDL_Cursor* cursorNs, float dpiScale = 1.0f);

} // namespace UI
