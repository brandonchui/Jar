#include "Outliner.h"
#include <imgui.h>

namespace UI
{

	void ShowOutliner(bool* pOpen, const std::vector<OutlinerItem>& items,
					  const OutlinerCallbacks& callbacks)
	{

		if (!ImGui::Begin("Outliner", pOpen))
		{
			ImGui::End();
			return;
		}

		// Scrollable mesh list.
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0F, 0.0F));
		ImGui::BeginChild("MeshList", ImVec2(0, 0), 0, ImGuiWindowFlags_None);
		ImGui::PopStyleVar();

		for (size_t i = 0; i < items.size(); ++i)
		{
			const OutlinerItem& item = items[i];

			// Alternating row background colors.
			if (i % 2 == 0)
			{
				ImGuiStyle& style = ImGui::GetStyle();
				ImVec4 altRowColor = style.Colors[ImGuiCol_TableRowBgAlt];

				ImVec2 rowMin = ImGui::GetCursorScreenPos();
				ImVec2 rowMax = ImVec2(rowMin.x + ImGui::GetContentRegionAvail().x,
									   rowMin.y + ImGui::GetFrameHeight());
				ImGui::GetWindowDrawList()->AddRectFilled(
					rowMin, rowMax, ImGui::ColorConvertFloat4ToU32(altRowColor));
			}

			ImGui::PushID(static_cast<int>(item.mEntityId));
			ImGui::Indent(8.0F);

			const char* eyeIcon = item.mVisible ? "x" : "  ";

			if (ImGui::SmallButton(eyeIcon))
			{
				if (callbacks.mSetVisible)
				{
					callbacks.mSetVisible(item.mEntityId, !item.mVisible);
				}
			}

			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Toggle visibility");
			}

			ImGui::SameLine();

			ImGuiSelectableFlags flags = ImGuiSelectableFlags_SpanAllColumns |
										 ImGuiSelectableFlags_AllowItemOverlap;

			// If selected, use this colors.
			if (item.mSelected)
			{
				ImGuiStyle& style = ImGui::GetStyle();
				ImGui::PushStyleColor(ImGuiCol_Header, style.Colors[ImGuiCol_HeaderHovered]);
				ImGui::PushStyleColor(ImGuiCol_HeaderHovered, style.Colors[ImGuiCol_HeaderHovered]);
				ImGui::PushStyleColor(ImGuiCol_HeaderActive, style.Colors[ImGuiCol_HeaderActive]);
			}

			if (ImGui::Selectable(item.mName.c_str(), item.mSelected, flags))
			{
				if (callbacks.mOnSelect)
				{
					callbacks.mOnSelect(item.mEntityId);
				}
			}

			if (item.mSelected)
			{
				ImGui::PopStyleColor(3);
			}

			// Context Menu.
			if (ImGui::BeginPopupContextItem())
			{
				ImGui::Text("Mesh: %s", item.mName.c_str());
				ImGui::Separator();

				if (ImGui::MenuItem("Rename"))
				{
					// TODO: Implement rename
				}

				if (ImGui::MenuItem("Duplicate"))
				{
					// TODO: Implement duplicate
				}

				ImGui::Separator();

				if (ImGui::MenuItem("Delete", "Del"))
				{
					if (callbacks.mOnDelete)
					{
						callbacks.mOnDelete(item.mEntityId);
					}
				}

				ImGui::EndPopup();
			}

			ImGui::Unindent(8.0F);
			ImGui::PopID();
		}

		if (items.empty())
		{
			ImGuiStyle& style = ImGui::GetStyle();
			ImGui::PushStyleColor(ImGuiCol_Text, style.Colors[ImGuiCol_TextDisabled]);
			ImGui::TextWrapped("No meshes in scene");
			ImGui::PopStyleColor();
		}

		ImGui::EndChild();

		ImGui::End();
	}

} // namespace UI
