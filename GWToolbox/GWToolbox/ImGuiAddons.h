#pragma once

#include <imgui.h>

namespace ImGui {
	// Shows '(?)' and the helptext when hovered
	IMGUI_API void ShowHelp(const char* help);

	IMGUI_API bool MyCombo(const char* label, const char* preview_text, int* current_item, 
		bool(*items_getter)(void* data, int idx, const char** out_text), void* data, int items_count, int height_in_items = -1);
}
