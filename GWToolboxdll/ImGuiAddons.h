#pragma once

using Color = ImU32;

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
    IMGUI_API void TextShadowed(const char* label, ImVec2 offset = {1, 1}, const ImVec4& shadow_color = {0, 0, 0, 1});

    IMGUI_API void SetNextWindowCenter(ImGuiWindowFlags flags);

    IMGUI_API const std::vector<ImGuiKey>& GetPressedKeys();

    IMGUI_API bool MyCombo(const char* label, const char* preview_text, int* current_item,
                           bool (*items_getter)(void* data, int idx, const char** out_text), void* data, int items_count);

    // Show a popup on-screen with a message and yes/no buttons. Returns true if an option has been chosen, with *result as true/false for yes/no
    IMGUI_API bool ConfirmDialog(const char* message, bool* result);

    IMGUI_API bool SmallConfirmButton(const char* label, bool* confirm_bool, const char* confirm_content = "Are you sure you want to continue?");
    IMGUI_API bool ChooseKey(const char* label, char* buf, size_t buf_len, long* key_code);

    IMGUI_API bool ConfirmButton(const char* label, bool* confirm_bool, const char* confirm_content = "Are you sure you want to continue?");

    // Button with single icon texture
    IMGUI_API bool IconButton(const char* label, ImTextureID icon, const ImVec2& size, ImGuiButtonFlags flags = ImGuiButtonFlags_None, const ImVec2& icon_size = {0.f, 0.f});

    IMGUI_API void ClosePopup(const char* popup_id);

    // Button with 1 or more icon textures overlaid
    IMGUI_API bool CompositeIconButton(const char* label, const ImTextureID* icons, size_t icons_len, const ImVec2& size, ImGuiButtonFlags flags = ImGuiButtonFlags_None, const ImVec2& icon_size = {0.f, 0.f}, const ImVec2& uv0 = {0.f, 0.f}, ImVec2 uv1 = {0.f, 0.f});

    IMGUI_API bool ColorButtonPicker(const char*, Color*, ImGuiColorEditFlags = 0);
    // Add cropped image to current window
    IMGUI_API void ImageCropped(ImTextureID user_texture_id, const ImVec2& size);

    // Window/context independent check
    IMGUI_API bool IsMouseInRect(const ImVec2& top_left, const ImVec2& bottom_right);
    // Add cropped image to window draw list
    IMGUI_API void AddImageCropped(ImTextureID user_texture_id, const ImVec2& top_left, const ImVec2& bottom_right);
    // Calculate the end position of a crop box for the given texture to fit into the given size
    IMGUI_API ImVec2 CalculateUvCrop(ImTextureID user_texture_id, const ImVec2& size);

    IMGUI_API bool ColorPalette(const char* label, size_t* palette_index, const ImVec4* palette, size_t count, size_t max_per_line, ImGuiColorEditFlags flags);

    // call before ImGui::Render() to clamp all windows to screen - pass false to restore original positions
    // e.g. before saving, or if the user doesn't want the windows clamped
    IMGUI_API void ClampAllWindowsToScreen(bool clamp);
}
