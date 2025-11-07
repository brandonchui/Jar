#include "TitleBar.h"
#include "../Theme.h"
#include <imgui_internal.h>
#include <cmath>

#include <algorithm>

namespace UI
{

	TitleBarState ShowTitleBar(SDL_Window* window, const char* title, bool& isDragging,
							   ImVec2& dragOffset, bool& isResizing, int& resizeEdge,
							   ImVec2& resizeStartMousePos, ImVec2& resizeStartWindowPos,
							   ImVec2& resizeStartWindowSize, SDL_Cursor* cursorDefault,
							   SDL_Cursor* cursorNwse, SDL_Cursor* cursorNesw, SDL_Cursor* cursorWe,
							   SDL_Cursor* cursorNs, float dpiScale)
	{
		TitleBarState state;
		state.action = TitleBarAction::None;
		state.isDragging = isDragging;
		state.isResizing = isResizing;

		const float TITLE_BAR_HEIGHT = 32.0F * dpiScale;
		const float BUTTON_WIDTH = 46.0F * dpiScale;

		// Title bar.
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->Pos);
		ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, TITLE_BAR_HEIGHT));
		ImGui::SetNextWindowViewport(viewport->ID);

		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0F);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0F, 0.0F));
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0F);
		ImGui::PushStyleColor(ImGuiCol_WindowBg, AppColors::TITLEBAR_BG);
		ImGui::PushStyleColor(ImGuiCol_MenuBarBg, AppColors::TITLEBAR_BG);

		ImGuiWindowFlags titleBarFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
										 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
										 ImGuiWindowFlags_NoSavedSettings |
										 ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoNav |
										 ImGuiWindowFlags_MenuBar;

		bool titleBarOpen = true;
		ImGui::Begin("##CustomTitleBar", &titleBarOpen, titleBarFlags);

		// Menu bar
		if (ImGui::BeginMenuBar())
		{
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 6.0F);

			ImFont* boldFont = UI::GetBoldFont();
			if (boldFont != nullptr)
			{
				ImGui::PushFont(boldFont);
			}

			ImGui::Text("%s", title);

			if (boldFont != nullptr)
			{
				ImGui::PopFont();
			}

			// Add spacing to differentiate menu from title
			ImGui::Spacing();
			ImGui::Spacing();

			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0F, 12.0F));

			// Style for dropdown menus
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0F, 10.0F));
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0F, 8.0F));

			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("Load Model"))
				{
					// TODO action
				}
				if (ImGui::MenuItem("Exit"))
				{
					// TODO action
				}
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Help"))
			{
				if (ImGui::MenuItem("About"))
				{
					// TODO action
				}
				ImGui::EndMenu();
			}

			ImGui::PopStyleVar(3);

			// Push close button to the right side of menu bar
			float menuBarHeight = ImGui::GetCurrentWindow()->TitleBarHeight;
			ImGui::SetCursorPosX(ImGui::GetWindowWidth() - BUTTON_WIDTH);

			// Transparent
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, AppColors::CLOSE_BUTTON_HOVER);
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, AppColors::CLOSE_BUTTON_ACTIVE);

			if (ImGui::Button("X", ImVec2(BUTTON_WIDTH, menuBarHeight)))
			{
				state.action = TitleBarAction::Close;
			}

			ImGui::PopStyleColor(3);

			ImGui::EndMenuBar();
		}

		// Drag title bar.
		bool titleBarHovered = ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered();

		// Check if we are in the resize zone.
		const float RESIZE_BORDER_SIZE = 8.0F * dpiScale;
		ImVec2 viewportPos = ImGui::GetMainViewport()->Pos;
		ImVec2 viewportSize = ImGui::GetMainViewport()->Size;
		ImVec2 mousePos = ImGui::GetMousePos();
		bool inResizeZone = false;

		if (mousePos.x - viewportPos.x < RESIZE_BORDER_SIZE)
			inResizeZone = true;
		if (viewportPos.x + viewportSize.x - mousePos.x < RESIZE_BORDER_SIZE)
			inResizeZone = true;
		if (mousePos.y - viewportPos.y < RESIZE_BORDER_SIZE)
			inResizeZone = true;
		if (viewportPos.y + viewportSize.y - mousePos.y < RESIZE_BORDER_SIZE)
			inResizeZone = true;

		if (titleBarHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !inResizeZone)
		{
			isDragging = true;
			int windowX = 0;
			int windowY = 0;
			SDL_GetWindowPosition(window, &windowX, &windowY);
			float mouseX = NAN;
			float mouseY = NAN;
			SDL_GetGlobalMouseState(&mouseX, &mouseY);
			dragOffset = ImVec2(mouseX - static_cast<float>(windowX),
								mouseY - static_cast<float>(windowY));
		}

		if (isDragging)
		{
			if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
			{
				float mouseX = NAN;
				float mouseY = NAN;
				SDL_GetGlobalMouseState(&mouseX, &mouseY);
				SDL_SetWindowPosition(window, static_cast<int>(mouseX - dragOffset.x),
									  static_cast<int>(mouseY - dragOffset.y));
			}
			else
			{
				isDragging = false;
			}
		}

		ImGui::End();
		ImGui::PopStyleColor(2);
		ImGui::PopStyleVar(4);

		// Resizing the window.
		{
			ImVec2 windowPos = ImGui::GetMainViewport()->Pos;
			ImVec2 windowSize = ImGui::GetMainViewport()->Size;
			/*ImVec2*/ mousePos = ImGui::GetMousePos();

			// Where is the cursor at?
			int hoverEdge = 0;
			if (!isDragging && !isResizing)
			{
				if (mousePos.x - windowPos.x < RESIZE_BORDER_SIZE)
					hoverEdge |= 1;
				if (windowPos.x + windowSize.x - mousePos.x < RESIZE_BORDER_SIZE)
					hoverEdge |= 2;
				if (mousePos.y - windowPos.y < RESIZE_BORDER_SIZE)
					hoverEdge |= 4;
				if (windowPos.y + windowSize.y - mousePos.y < RESIZE_BORDER_SIZE)
					hoverEdge |= 8;

				if (hoverEdge == 5 || hoverEdge == 10)
					SDL_SetCursor(cursorNwse);
				else if (hoverEdge == 6 || hoverEdge == 9)
					SDL_SetCursor(cursorNesw);
				else if (hoverEdge & 3)
					SDL_SetCursor(cursorWe);
				else if (hoverEdge & 12)
					SDL_SetCursor(cursorNs);
				else
					SDL_SetCursor(cursorDefault);
			}

			// Start resizing.
			if (hoverEdge != 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
			{
				isResizing = true;
				resizeEdge = hoverEdge;

				SDL_GetGlobalMouseState(&resizeStartMousePos.x, &resizeStartMousePos.y);
				int wx = 0;
				int wy = 0;
				int ww = 0;
				int wh = 0;
				SDL_GetWindowPosition(window, &wx, &wy);
				SDL_GetWindowSize(window, &ww, &wh);
				resizeStartWindowPos = ImVec2(static_cast<float>(wx), static_cast<float>(wy));
				resizeStartWindowSize = ImVec2(static_cast<float>(ww), static_cast<float>(wh));
			}

			if (isResizing && ImGui::IsMouseDown(ImGuiMouseButton_Left))
			{
				float mouseX = NAN;
				float mouseY = NAN;
				SDL_GetGlobalMouseState(&mouseX, &mouseY);

				float deltaX = mouseX - resizeStartMousePos.x;
				float deltaY = mouseY - resizeStartMousePos.y;

				float newX = resizeStartWindowPos.x;
				float newY = resizeStartWindowPos.y;
				float newW = resizeStartWindowSize.x;
				float newH = resizeStartWindowSize.y;

				const int MIN_WIDTH = 640;
				const int MIN_HEIGHT = 480;

				if (resizeEdge & 1)
				{
					// Left edge.
					newX = resizeStartWindowPos.x + deltaX;
					newW = resizeStartWindowSize.x - deltaX;

					if (newW < MIN_WIDTH)
					{
						newX = resizeStartWindowPos.x + resizeStartWindowSize.x - MIN_WIDTH;
						newW = MIN_WIDTH;
					}
				}
				if (resizeEdge & 2)
				{
					// Right edge.
					newW = resizeStartWindowSize.x + deltaX;
					newW = std::max<float>(newW, MIN_WIDTH);
				}
				if (resizeEdge & 4)
				{
					// Top edge.
					newY = resizeStartWindowPos.y + deltaY;
					newH = resizeStartWindowSize.y - deltaY;

					if (newH < MIN_HEIGHT)
					{
						newY = resizeStartWindowPos.y + resizeStartWindowSize.y - MIN_HEIGHT;
						newH = MIN_HEIGHT;
					}
				}
				if (resizeEdge & 8)
				{
					// Bottom edge.
					newH = resizeStartWindowSize.y + deltaY;

					newH = std::max<float>(newH, MIN_HEIGHT);
				}

				SDL_SetWindowPosition(window, static_cast<int>(newX), static_cast<int>(newY));
				SDL_SetWindowSize(window, static_cast<int>(newW), static_cast<int>(newH));
			}

			// Stop.
			if (isResizing && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
			{
				isResizing = false;
				resizeEdge = 0;
			}
		}

		state.isDragging = isDragging;
		state.isResizing = isResizing;

		return state;
	}

} // namespace UI
