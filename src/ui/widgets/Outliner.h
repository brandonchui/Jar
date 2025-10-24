#pragma once

#include <SDL3/SDL.h>
#include <functional>
#include <string>
#include <vector>

namespace UI
{
	/// Data for each item on the outliner.
	struct OutlinerItem
	{
		uint32_t mEntityId;
		std::string mName;
		bool mVisible;
		bool mSelected;
	};

	/// We are using callbacks to dictate the functionality of the outliner
	/// and to keep it more generic in case I use it again elsewhere.
	struct OutlinerCallbacks
	{
		std::function<std::string(uint32_t)> mGetMeshName;
		std::function<bool(uint32_t)> mIsVisible;
		std::function<void(uint32_t, bool)> mSetVisible;
		std::function<void(uint32_t)> mOnSelect;
		std::function<void(uint32_t)> mOnDelete;
	};

	void ShowOutliner(bool* pOpen, const std::vector<OutlinerItem>& items,
					  const OutlinerCallbacks& callbacks);

} // namespace UI
