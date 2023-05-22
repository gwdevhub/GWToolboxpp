#pragma once

typedef ImU32 Color;

constexpr uint32_t ImGuiButtonFlags_AlignTextLeft = 1 << 20;

namespace ImGui {
    // Shorthand for ImGui::GetIO().GlobalFontScale
    IMGUI_API const float& FontScale();
    // Initialise available width etc for adding spaced elements. Must be called before calling NextSpacedElement()
    IMGUI_API void StartSpacedElements(float width, bool include_font_scaling = true);
    // Called before adding an imgui control that needs to be spaced. Call StartSpacedElements() before this.
    IMGUI_API void NextSpacedElement();

    // Shows '(?)' and the helptext when hovered
    IMGUI_API void ShowHelp(const char* help);
    // Shows current text with a drop shadow
    IMGUI_API void TextShadowed(const char* label, ImVec2 offset = { 1, 1 }, ImVec4 shadow_color = { 0, 0, 0, 1 });

    IMGUI_API void SetNextWindowCenter(ImGuiWindowFlags flags);

    IMGUI_API bool MyCombo(const char* label, const char* preview_text, int* current_item,
        bool(*items_getter)(void* data, int idx, const char** out_text), void* data, int items_count);

    IMGUI_API bool SmallConfirmButton(const char* label, bool* confirm_bool, const char* confirm_content = "Are you sure you want to continue?");

    IMGUI_API bool ConfirmButton(const char* label, bool* confirm_bool, const char* confirm_content = "Are you sure you want to continue?");

    IMGUI_API bool IconButton(const char* label, ImTextureID icon, const ImVec2& size, ImGuiButtonFlags flags = ImGuiButtonFlags_None, const ImVec2& icon_size = { 0.f, 0.f });

    IMGUI_API bool ColorButtonPicker(const char*, Color*, ImGuiColorEditFlags = 0);
    // Add cropped image to current window
    IMGUI_API void ImageCropped(ImTextureID user_texture_id, const ImVec2& size);
    // Add cropped image to window draw list
    IMGUI_API void AddImageCropped(ImTextureID user_texture_id, const ImVec2& top_left, const ImVec2& bottom_right);
    // Calculate the end position of a crop box for the given texture to fit into the given size
    IMGUI_API ImVec2 CalculateUvCrop(ImTextureID user_texture_id, const ImVec2& size);
    
    IMGUI_API bool ColorPalette(const char* label, size_t* palette_index, const ImVec4* palette, size_t count, size_t max_per_line, ImGuiColorEditFlags flags);
}
