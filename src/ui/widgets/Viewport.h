#pragma once

#include <SDL3/SDL.h>
#include <imgui.h>
#include <d3d12.h>

namespace UI
{
	struct ViewportState
	{
		ImVec2 size;
		bool isHovered;
		bool isFocused;
	};

	ViewportState ShowViewport(bool* pOpen, D3D12_GPU_DESCRIPTOR_HANDLE viewportSrv);

} // namespace UI
