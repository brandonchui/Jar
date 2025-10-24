#include "Properties.h"
#include "../../Lighting.h"
#include <imgui.h>
#include <cmath>
#include <numbers>

namespace UI
{

	void ShowProperties(bool* pOpen, const char* selectedObjectName, TransformProperties& transform,
						const PropertiesCallbacks& callbacks, SpotLight* spotLight)
	{

		if (!ImGui::Begin("Properties", pOpen))
		{
			ImGui::End();
			return;
		}

		// Debugging the selected objects, though it will probably be better to have it
		// end up in the outliner instead.
		ImGuiStyle& style = ImGui::GetStyle();
		ImGui::PushStyleColor(ImGuiCol_Text, style.Colors[ImGuiCol_HeaderHovered]);
		ImGui::Text("Selected: %s", selectedObjectName);
		ImGui::PopStyleColor();
		ImGui::Separator();
		ImGui::Spacing();

		// Transform Section
		if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Indent();

			bool transformChanged = false;

			ImGui::Text("Position");
			if (ImGui::DragFloat3("##Position", transform.position, 0.1F, -100.0F, 100.0F, "%.2f"))
			{
				transformChanged = true;
			}

			ImGui::Spacing();
			ImGui::Text("Rotation");
			if (ImGui::DragFloat3("##Rotation", transform.rotation, 1.0F, -180.0F, 180.0F, "%.1f°"))
			{
				transformChanged = true;
			}

			ImGui::Spacing();
			ImGui::Text("Scale");
			if (ImGui::DragFloat3("##Scale", transform.scale, 0.01F, 0.01F, 10.0F, "%.2f"))
			{
				transformChanged = true;
			}

			if (transformChanged && callbacks.onTransformChanged)
			{
				callbacks.onTransformChanged(transform);
			}

			ImGui::Unindent();
			ImGui::Spacing();
		}

		// Frame stats.
		if (ImGui::CollapsingHeader("Render Stats"))
		{
			ImGui::Indent();

			ImGui::Text("FPS: %.1f", static_cast<double>(ImGui::GetIO().Framerate));
			ImGui::Text("Frame Time: %.3f ms",
						static_cast<double>(1000.0F / ImGui::GetIO().Framerate));

			ImGui::Unindent();
			ImGui::Spacing();
		}

		// Spotlight controls
		if (spotLight && ImGui::CollapsingHeader("Spotlight", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Indent();

			bool lightChanged = false;

			ImGui::Text("Position");
			float pos[3] = {spotLight->position.x, spotLight->position.y, spotLight->position.z};
			if (ImGui::DragFloat3("##LightPos", pos, 0.5F, -100.0F, 100.0F, "%.1f"))
			{
				spotLight->position.x = pos[0];
				spotLight->position.y = pos[1];
				spotLight->position.z = pos[2];
				lightChanged = true;
			}

			ImGui::Spacing();
			ImGui::Text("Direction");
			float dir[3] = {spotLight->direction.x, spotLight->direction.y, spotLight->direction.z};
			if (ImGui::DragFloat3("##LightDir", dir, 0.01F, -1.0F, 1.0F, "%.2f"))
			{
				// Normalize direction
				float len = std::sqrt(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
				if (len > 0.001F)
				{
					spotLight->direction.x = dir[0] / len;
					spotLight->direction.y = dir[1] / len;
					spotLight->direction.z = dir[2] / len;
				}
				lightChanged = true;
			}

			ImGui::Spacing();
			ImGui::Text("Color");
			float col[3] = {spotLight->color.x, spotLight->color.y, spotLight->color.z};
			if (ImGui::ColorEdit3("##LightColor", col))
			{
				spotLight->color.x = col[0];
				spotLight->color.y = col[1];
				spotLight->color.z = col[2];
				lightChanged = true;
			}

			ImGui::Spacing();
			ImGui::Text("Intensity");
			if (ImGui::DragFloat("##Intensity", &spotLight->intensity, 0.1F, 0.0F, 100.0F, "%.1f"))
			{
				lightChanged = true;
			}

			ImGui::Spacing();
			ImGui::Text("Range");
			if (ImGui::DragFloat("##Range", &spotLight->range, 1.0F, 1.0F, 500.0F, "%.1f"))
			{
				lightChanged = true;
			}

			ImGui::Spacing();
			ImGui::Text("Inner Cone Angle (deg)");
			float innerDeg = spotLight->innerConeAngle * (180.0F / std::numbers::pi_v<float>);
			if (ImGui::SliderFloat("##InnerCone", &innerDeg, 0.0F, 89.0F, "%.1f°"))
			{
				spotLight->innerConeAngle = innerDeg * (std::numbers::pi_v<float> / 180.0F);
				lightChanged = true;
			}

			ImGui::Spacing();
			ImGui::Text("Outer Cone Angle (deg)");
			float outerDeg = spotLight->outerConeAngle * (180.0F / std::numbers::pi_v<float>);
			if (ImGui::SliderFloat("##OuterCone", &outerDeg, 0.0F, 90.0F, "%.1f°"))
			{
				spotLight->outerConeAngle = outerDeg * (std::numbers::pi_v<float> / 180.0F);
				lightChanged = true;
			}

			ImGui::Spacing();
			ImGui::Text("Falloff");
			if (ImGui::DragFloat("##Falloff", &spotLight->falloff, 0.00001F, 0.0F, 1.0F, "%.5f"))
			{
				lightChanged = true;
			}

			if (lightChanged && callbacks.onSpotLightChanged)
			{
				callbacks.onSpotLightChanged();
			}

			ImGui::Unindent();
			ImGui::Spacing();
		}

		ImGui::End();
	}

} // namespace UI
