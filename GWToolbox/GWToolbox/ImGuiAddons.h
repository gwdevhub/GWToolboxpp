#pragma once

#include <imgui.h>

namespace ImGui {
	// Shows '(?)' and the helptext when hovered
	void ShowHelp(const char* help);

	bool MyCombo(const char* label, int* current_item, bool(*items_getter)(void*, int, const char**),
		void* data, int items_count, int height_in_items = -1);
}
