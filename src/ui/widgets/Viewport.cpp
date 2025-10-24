#include "Viewport.h"
#include "../Theme.h"
#include <imgui.h>

namespace UI
{

	ViewportState ShowViewport(bool* pOpen, D3D12_GPU_DESCRIPTOR_HANDLE viewportSrv)
	{
		ViewportState state = {};
		state.size = ImVec2(0, 0);
		state.isHovered = false;
		state.isFocused = false;

		// The viewport widget  should be edge to edge given the offscreen texture.
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0F, 0.0F));

		if (!ImGui::Begin("Viewport", pOpen))
		{
			ImGui::PopStyleVar();
			ImGui::End();
			return state;
		}

		state.size = ImGui::GetContentRegionAvail();

		state.isHovered = ImGui::IsWindowHovered();
		state.isFocused = ImGui::IsWindowFocused();

		if (viewportSrv.ptr != 0)
		{
			// Render the viewport texture.
			ImGui::Image(static_cast<ImTextureID>(viewportSrv.ptr), state.size);
		}
		else
		{
			// Placeholder for when the viewport texture fails to render.
			ImVec2 contentMin = ImGui::GetWindowContentRegionMin();
			ImVec2 contentMax = ImGui::GetWindowContentRegionMax();
			ImVec2 windowPos = ImGui::GetWindowPos();

			contentMin.x += windowPos.x;
			contentMin.y += windowPos.y;
			contentMax.x += windowPos.x;
			contentMax.y += windowPos.y;

			ImDrawList* drawList = ImGui::GetWindowDrawList();
			ImU32 placeholderColor =
				ImGui::ColorConvertFloat4ToU32(AppColors::VIEWPORT_PLACEHOLDER);
			drawList->AddRectFilled(contentMin, contentMax, placeholderColor);
		}

		ImGui::End();
		ImGui::PopStyleVar();

		return state;
	}

} // namespace UI
