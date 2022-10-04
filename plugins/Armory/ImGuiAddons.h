#pragma once
#include <imgui.h>

namespace ImGui {
    // Shows '(?)' and the helptext when hovered
    IMGUI_API void ShowHelp(const char* help);
    // Shows current text with a drop shadow
    IMGUI_API void TextShadowed(const char* label, ImVec2 offset = { 1, 1 }, ImVec4 shadow_color = { 0, 0, 0, 1 });

    IMGUI_API void SetNextWindowCenter(ImGuiWindowFlags flags);

    IMGUI_API bool MyCombo(const char* label, const char* preview_text, int* current_item,
        bool(*items_getter)(void* data, int idx, const char** out_text), void* data, int items_count, int height_in_items = -1);

    IMGUI_API bool SmallConfirmButton(const char* label, bool* confirm_bool, const char* confirm_content = "Are you sure you want to continue?");

    IMGUI_API bool ConfirmButton(const char* label, bool* confirm_bool, const char* confirm_content = "Are you sure you want to continue?");

    IMGUI_API bool IconButton(const char *str_id, ImTextureID user_texture_id, const ImVec2 &size);

    IMGUI_API bool ColorButtonPicker(const char*, ImU32*, ImGuiColorEditFlags = 0);

    IMGUI_API bool ColorPalette(const char* label, size_t* palette_index,
        ImVec4* palette, size_t count, size_t max_per_line, ImGuiColorEditFlags flags);
}
