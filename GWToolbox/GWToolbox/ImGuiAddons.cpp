#include "ImGuiAddons.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <time.h>
#include <ctype.h>

#include <Key.h>
#include <Timer.h>

// copies of imgui funcs that are not exposed here
// It's ugly I know but I'd rather not change ImGui files
static bool IsPopupOpen(ImGuiID id) {
	ImGuiContext& g = *GImGui;
	return g.OpenPopupStack.Size > g.CurrentPopupStack.Size && g.OpenPopupStack[g.CurrentPopupStack.Size].PopupId == id;
}
static void ClosePopupToLevel(int remaining) {
	ImGuiContext& g = *GImGui;
	if (remaining > 0)
		ImGui::FocusWindow(g.OpenPopupStack[remaining - 1].Window);
	else
		ImGui::FocusWindow(g.OpenPopupStack[0].ParentWindow);
	g.OpenPopupStack.resize(remaining);
}
static void ClosePopup(ImGuiID id) {
	if (!IsPopupOpen(id))
		return;
	ImGuiContext& g = *GImGui;
	ClosePopupToLevel(g.OpenPopupStack.Size - 1);
}
static inline void ClearSetNextWindowData() {
	ImGuiContext& g = *GImGui;
	g.SetNextWindowPosCond = g.SetNextWindowSizeCond = g.SetNextWindowContentSizeCond = g.SetNextWindowCollapsedCond = 0;
	g.SetNextWindowSizeConstraint = g.SetNextWindowFocus = false;
}
static bool BeginPopupEx(const char* str_id, ImGuiWindowFlags extra_flags) {
	ImGuiContext& g = *GImGui;
	ImGuiWindow* window = g.CurrentWindow;
	const ImGuiID id = window->GetID(str_id);
	if (!IsPopupOpen(id)) {
		ClearSetNextWindowData(); // We behave like Begin() and need to consume those values
		return false;
	}

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGuiWindowFlags flags = extra_flags | ImGuiWindowFlags_Popup | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize;

	char name[20];
	if (flags & ImGuiWindowFlags_ChildMenu)
		ImFormatString(name, IM_ARRAYSIZE(name), "##menu_%d", g.CurrentPopupStack.Size);    // Recycle windows based on depth
	else
		ImFormatString(name, IM_ARRAYSIZE(name), "##popup_%08x", id); // Not recycling, so we can close/open during the same frame

	bool is_open = ImGui::Begin(name, NULL, flags);
	if (!(window->Flags & ImGuiWindowFlags_ShowBorders))
		g.CurrentWindow->Flags &= ~ImGuiWindowFlags_ShowBorders;
	if (!is_open) // NB: is_open can be 'false' when the popup is completely clipped (e.g. zero size display)
		ImGui::EndPopup();

	return is_open;
}
// ===========================================

void ImGui::ShowHelp(const char* help) {
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(help);
	}
}

bool ImGui::MyCombo(const char* label, int* current_item, bool(*items_getter)(void*, int, const char**), 
	void* data, int items_count, int height_in_items) {

	ImGuiWindow* window = GetCurrentWindow();
	if (window->SkipItems)
		return false;

	ImGuiContext& g = *GImGui;
	const ImGuiStyle& style = g.Style;
	const ImGuiIO& io = g.IO;
	const ImGuiID id = window->GetID(label);
	const float w = CalcItemWidth();

	const ImVec2 label_size = CalcTextSize(label, NULL, true);
	const ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + ImVec2(w, label_size.y + style.FramePadding.y*2.0f));
	const ImRect total_bb(frame_bb.Min, frame_bb.Max);
	ItemSize(total_bb, style.FramePadding.y);
	if (!ItemAdd(total_bb, &id)) return false;

	const float arrow_size = (g.FontSize + style.FramePadding.x * 2.0f);
	const bool hovered = IsHovered(frame_bb, id);
	bool popup_open = IsPopupOpen(id);
	bool popup_opened_now = false;

	const ImRect value_bb(frame_bb.Min, frame_bb.Max - ImVec2(arrow_size, 0.0f));
	RenderFrame(frame_bb.Min, frame_bb.Max, GetColorU32(ImGuiCol_FrameBg), true, style.FrameRounding);
	RenderFrame(ImVec2(frame_bb.Max.x - arrow_size, frame_bb.Min.y), frame_bb.Max, GetColorU32(popup_open || hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button), true, style.FrameRounding); // FIXME-ROUNDING
	RenderCollapseTriangle(ImVec2(frame_bb.Max.x - arrow_size, frame_bb.Min.y) + style.FramePadding, true);
	
	if (*current_item >= 0 && *current_item < items_count) {
		const char* item_text;
		if (items_getter(data, *current_item, &item_text))
			RenderTextClipped(frame_bb.Min + style.FramePadding, value_bb.Max, item_text, NULL, NULL, ImVec2(0.0f, 0.0f));
	} else {
		RenderTextClipped(frame_bb.Min + style.FramePadding, value_bb.Max, label, NULL, NULL, ImVec2(0.0f, 0.0f));
	}

	static int keyboard_selected = -1;
	bool keyboard_selected_now = false;

	if (hovered) {
		SetHoveredID(id);
		if (g.IO.MouseClicked[0]) {
			ClearActiveID();
			if (IsPopupOpen(id)) {
				ClosePopup(id);
			} else {
				FocusWindow(window);
				OpenPopup(label);
				popup_open = popup_opened_now = true;
			}
			keyboard_selected = -1;
		}
	}

	bool value_changed = false;
	if (IsPopupOpen(id)) {
		GetIO().WantTextInput = true;
		for (int n = 0; n < IM_ARRAYSIZE(io.InputCharacters) && io.InputCharacters[n]; n++) {
			if (unsigned int c = (unsigned int)io.InputCharacters[n]) {
				if (c == ' ' 
					|| (c >= '0' && c <= '9')
					|| (c >= 'A' && c <= 'Z')
					|| (c >= 'a' && c <= 'z')) {
					
					static char word[64] = "";
					static time_t timer = clock();

					if (clock() - timer < 500) { // append
						const int i = strnlen(word, 64);
						if (i + 1 < 64) {
							word[i] = c;
							word[i + 1] = '\0';
						}
					} else { // reset
						word[0] = c;
						word[1] = '\0';
					}
					timer = clock();
					
					// find best fit
					int best = -1;
					for (int i = 0; i < items_count; i++) {
						const char* item_text;
						if (items_getter(data, i, &item_text)) {
							int j = 0;
							while (word[j] 
								&& item_text[j] 
								&& tolower(item_text[j]) == tolower(word[j])) ++j;
							if (best < j) {
								best = j;
								keyboard_selected = i;
								keyboard_selected_now = true;
							}
						}
					}
				}
			}
		}
		if (IsKeyPressed(VK_RETURN) && keyboard_selected >= 0) {
			*current_item = keyboard_selected;
			value_changed = true;
		}
		if (IsKeyPressed(VK_UP) && keyboard_selected >= 0 && keyboard_selected < items_count - 1) {
			--keyboard_selected;
			keyboard_selected_now = true;
		}
		if (IsKeyPressed(VK_DOWN) && keyboard_selected >= 1) {
			++keyboard_selected;
			keyboard_selected_now = true;
		}

		// Size default to hold ~7 items
		if (height_in_items < 0)
			height_in_items = 7;

		float popup_height = (label_size.y + style.ItemSpacing.y) * ImMin(items_count, height_in_items) + (style.FramePadding.y * 3);
		float popup_y1 = frame_bb.Max.y;
		float popup_y2 = ImClamp(popup_y1 + popup_height, popup_y1, g.IO.DisplaySize.y - style.DisplaySafeAreaPadding.y);
		if ((popup_y2 - popup_y1) < ImMin(popup_height, frame_bb.Min.y - style.DisplaySafeAreaPadding.y)) {
			// Position our combo ABOVE because there's more space to fit! (FIXME: Handle in Begin() or use a shared helper. We have similar code in Begin() for popup placement)
			popup_y1 = ImClamp(frame_bb.Min.y - popup_height, style.DisplaySafeAreaPadding.y, frame_bb.Min.y);
			popup_y2 = frame_bb.Min.y;
		}
		ImRect popup_rect(ImVec2(frame_bb.Min.x, popup_y1), ImVec2(frame_bb.Max.x, popup_y2));
		SetNextWindowPos(popup_rect.Min);
		SetNextWindowSize(popup_rect.GetSize());
		PushStyleVar(ImGuiStyleVar_WindowPadding, style.FramePadding);

		const ImGuiWindowFlags flags = ImGuiWindowFlags_ComboBox | ((window->Flags & ImGuiWindowFlags_ShowBorders) ? ImGuiWindowFlags_ShowBorders : 0);
		if (BeginPopupEx(label, flags)) {
			// Display items
			Spacing();
			for (int i = 0; i < items_count; i++) {
				PushID((void*)(intptr_t)i);
				const bool item_selected = (i == *current_item);
				const bool item_keyboard_selected = (i == keyboard_selected);
				const char* item_text;
				if (!items_getter(data, i, &item_text))
					item_text = "*Unknown item*";
				if (Selectable(item_text, item_selected || item_keyboard_selected)) {
					ClearActiveID();
					value_changed = true;
					*current_item = i;
				}
				if (item_selected && popup_opened_now)
					SetScrollHere();
				if (item_keyboard_selected && keyboard_selected_now) {
					SetScrollHere();
				}
				PopID();
			}
			EndPopup();
		}
		PopStyleVar();
	}
	return value_changed;
}
