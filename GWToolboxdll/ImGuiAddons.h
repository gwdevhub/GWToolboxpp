#pragma once

typedef ImU32 Color;

namespace ImGui {
    // Shows '(?)' and the helptext when hovered
    IMGUI_API void ShowHelp(const char* help);

    IMGUI_API bool MyCombo(const char* label, const char* preview_text, int* current_item, 
        bool(*items_getter)(void* data, int idx, const char** out_text), void* data, int items_count, int height_in_items = -1);

    IMGUI_API bool IconButton(const char *str_id, ImTextureID user_texture_id, const ImVec2 &size);

    IMGUI_API bool ColorButtonPicker(const char*, Color*, ImGuiColorEditFlags = 0);
}
