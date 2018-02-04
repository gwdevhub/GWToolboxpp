#include "ImGuiAddons.h"

#include <Windows.h>

#include <imgui.h>
#include <imgui_internal.h>

void ImGui::ShowHelp(const char* help) {
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(help);
	}
}

bool ImGui::MyCombo(const char* label, const char* preview_text, int* current_item, bool(*items_getter)(void*, int, const char**), 
	void* data, int items_count, int height_in_items) {

	ImGuiContext& g = *GImGui;
	const float word_building_delay = .5f; // after this many seconds, typing will make a new search

	if (*current_item >= 0 && *current_item < items_count) {
		items_getter(data, *current_item, &preview_text);
	}

	// this is actually shared between all combos. It's kinda ok because there is
	// only one combo open at any given time, however it causes a problem where
	// if you open combo -> keyboard select (but no enter) and close, the 
	// keyboard_selected will stay as-is when re-opening the combo, or even others.
	static int keyboard_selected = -1;

	if (!BeginCombo(label, preview_text)) {
		return false;
	}

	GetIO().WantTextInput = true;
	static char word[64] = "";
	static float time_since_last_update = 0.0f;
	time_since_last_update += g.IO.DeltaTime;
	bool update_keyboard_match = false;
	for (int n = 0; n < IM_ARRAYSIZE(g.IO.InputCharacters) && g.IO.InputCharacters[n]; n++) {
		if (unsigned int c = (unsigned int)g.IO.InputCharacters[n]) {
			if (c == ' '
				|| (c >= '0' && c <= '9')
				|| (c >= 'A' && c <= 'Z')
				|| (c >= 'a' && c <= 'z')) {

				// build temporary word
				if (time_since_last_update < word_building_delay) { // append
					const int i = strnlen(word, 64);
					if (i + 1 < 64) {
						word[i] = c;
						word[i + 1] = '\0';
					}
				} else { // reset
					word[0] = c;
					word[1] = '\0';
				}
				time_since_last_update = 0.0f;
				update_keyboard_match = true;
			}
		}
	}

	// find best keyboard match
	int best = -1;
	bool keyboard_selected_now = false;
	if (update_keyboard_match) {
		for (int i = 0; i < items_count; ++i) {
			const char* item_text;
			if (items_getter(data, i, &item_text)) {
				int j = 0;
				while (word[j] && item_text[j] && tolower(item_text[j]) == tolower(word[j])) {
					++j;
				}
				if (best < j) {
					best = j;
					keyboard_selected = i;
					keyboard_selected_now = true;
				}
			}
		}
	}

	if (IsKeyPressed(VK_RETURN) && keyboard_selected >= 0) {
		*current_item = keyboard_selected;
		keyboard_selected = -1;
		CloseCurrentPopup();
		EndCombo();
		return true;
	}
	if (IsKeyPressed(VK_UP) && keyboard_selected > 0) {
		--keyboard_selected;
		keyboard_selected_now = true;
	}
	if (IsKeyPressed(VK_DOWN) && keyboard_selected < items_count - 1) {
		++keyboard_selected;
		keyboard_selected_now = true;
	}

	// Display items
	bool value_changed = false;
	for (int i = 0; i < items_count; i++) {
		PushID((void*)(intptr_t)i);
		const bool item_selected = (i == *current_item);
		const bool item_keyboard_selected = (i == keyboard_selected);
		const char* item_text;
		if (!items_getter(data, i, &item_text)) {
			item_text = "*Unknown item*";
		}
		if (Selectable(item_text, item_selected || item_keyboard_selected)) {
			value_changed = true;
			*current_item = i;
			keyboard_selected = -1;
		}
		if (item_selected && IsWindowAppearing()) {
			SetScrollHere();
		}
		if (item_keyboard_selected && keyboard_selected_now) {
			SetScrollHere();
		}
		PopID();
	}

	EndCombo();
	return value_changed;
}
