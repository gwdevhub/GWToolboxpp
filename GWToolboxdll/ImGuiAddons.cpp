#include "Color.h"
#include "stdafx.h"

#include <ImGuiAddons.h>
#include <string>

using namespace std::string_literals;

namespace ImGui {
    float element_spacing_width = 0.f;
    int element_spacing_cols = 1;
    int element_spacing_col_idx = 0;
    float* element_spacing_indent = 0;
    const float& FontScale() {
        return GetIO().FontGlobalScale;
    }
    void StartSpacedElements(float width, bool include_font_scaling) {
        element_spacing_width = width;
        if (include_font_scaling)
            element_spacing_width *= FontScale();
        element_spacing_cols = static_cast<int>(std::floor(GetContentRegionAvail().x / element_spacing_width));
        element_spacing_col_idx = 0;
        element_spacing_indent = &(GetCurrentWindow()->DC.Indent.x);
    }
    void NextSpacedElement() {
        if (element_spacing_col_idx) {
            if (element_spacing_col_idx < element_spacing_cols) {
                SameLine((element_spacing_width * element_spacing_col_idx) + *element_spacing_indent);
            }
            else {
                element_spacing_col_idx = 0;
            }
        }
        element_spacing_col_idx++;
    }


    void ShowHelp(const char* help) {
        SameLine();
        TextDisabled("%s","(?)");
        if (IsItemHovered()) {
            SetTooltip("%s",help);
        }
    }
    void TextShadowed(const char* label, ImVec2 offset, ImVec4 shadow_color) {
        const ImVec2 pos = GetCursorPos();
        SetCursorPos(ImVec2(pos.x + offset.x, pos.y + offset.y));
        TextColored(shadow_color, "%s", label);
        SetCursorPos(pos);
        TextUnformatted(label);
    }
    void SetNextWindowCenter(ImGuiWindowFlags flags) {
        const auto& io = GetIO();
        SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), flags, ImVec2(0.5f, 0.5f));
    }
    bool SmallConfirmButton(const char* label, bool* confirm_bool, const char* confirm_content) {
        static char id_buf[128];
        snprintf(id_buf, sizeof(id_buf), "%s##confirm_popup%p", label, confirm_bool);
        if (SmallButton(label)) {
            OpenPopup(id_buf);
        }
        if (BeginPopupModal(id_buf, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            Text(confirm_content);
            if (Button("OK", ImVec2(120, 0))) {
                *confirm_bool = true;
                CloseCurrentPopup();
            }
            SameLine();
            if (Button("Cancel", ImVec2(120, 0))) {
                CloseCurrentPopup();
            }
            EndPopup();
        }
        return *confirm_bool;
    }
    bool ConfirmButton(const char* label, bool* confirm_bool, const char* confirm_content) {
        static char id_buf[128];
        snprintf(id_buf, sizeof(id_buf), "%s##confirm_popup%p", label, confirm_bool);
        if (Button(label)) {
            OpenPopup(id_buf);
        }
        if (BeginPopupModal(id_buf, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            Text(confirm_content);
            if (Button("OK", ImVec2(120, 0))) {
                *confirm_bool = true;
                CloseCurrentPopup();
            }
            SameLine();
            if (Button("Cancel", ImVec2(120, 0))) {
                CloseCurrentPopup();
            }
            EndPopup();
        }
        return *confirm_bool;
    }

    bool IconButton(const char* label, ImTextureID icon, const ImVec2& size, ImGuiButtonFlags flags, const ImVec2& icon_size)
    {
        char button_id[128];
        sprintf(button_id, "###icon_button_%s", label);
        const ImVec2& pos = GetCursorScreenPos();
        const ImVec2& textsize = CalcTextSize(label);
        const bool clicked = ButtonEx(button_id, size, flags);

        const ImVec2& button_size = GetItemRectSize();
        ImVec2 img_size = icon_size;
        if (!icon) {
            img_size = { 0.f, 0.f };
        }
        else {
            if (icon_size.x > 0.f)
                img_size.x = icon_size.x;
            if (icon_size.y > 0.f)
                img_size.y = icon_size.y;
            if (img_size.y == 0.f)
                img_size.y = button_size.y - 2.f;
            if (img_size.x == 0.f)
                img_size.x = img_size.y;
        }
        const ImGuiStyle& style = GetStyle();
        const float content_width = img_size.x + textsize.x + (style.FramePadding.x * 2.f);
        float content_x = pos.x + style.FramePadding.x;
        if (content_width < button_size.x) {
            const float avail_space = button_size.x - content_width;
            content_x += (avail_space * style.ButtonTextAlign.x);
        }
        const float img_x = content_x;
        const float img_y = pos.y + ((button_size.y - img_size.y) / 2.f);
        const float text_x = img_x + img_size.x + 3.f;
        const float text_y = pos.y + (button_size.y - textsize.y) * style.ButtonTextAlign.y;
        if (img_size.x) {
            ImGui::AddImageCropped(icon, ImVec2(img_x, img_y), ImVec2(img_x + img_size.x, img_y + img_size.y));
        }
        if (label)
            GetWindowDrawList()->AddText(ImVec2(text_x, text_y), ImColor(GetStyle().Colors[ImGuiCol_Text]), label);
        return clicked;
    }
    bool ColorButtonPicker(const char* label, Color* imcol, const ImGuiColorEditFlags flags)
    {
        return Colors::DrawSettingHueWheel(label, imcol,
            flags | ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_NoLabel |
            ImGuiColorEditFlags_NoInputs);
    }
    bool MyCombo(const char* label, const char* preview_text, int* current_item, bool(*items_getter)(void*, int, const char**),
        void* data, int items_count) {

        ImGuiContext& g = *GImGui;
        constexpr float word_building_delay = .5f; // after this many seconds, typing will make a new search

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
        for (int n = 0; n < g.IO.InputQueueCharacters.size() && g.IO.InputQueueCharacters[n]; n++) {
            if (const auto c = static_cast<unsigned int>(g.IO.InputQueueCharacters[n])) {
                if (c == ' '
                    || (c >= '0' && c <= '9')
                    || (c >= 'A' && c <= 'Z')
                    || (c >= 'a' && c <= 'z')) {

                    // build temporary word
                    if (time_since_last_update < word_building_delay) { // append
                        const size_t i = strnlen(word, 64);
                        if (i + 1 < 64) {
                            word[i] = static_cast<char>(c);
                            word[i + 1] = '\0';
                        }
                    }
                    else { // reset
                        word[0] = static_cast<char>(c);
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

        if (IsKeyPressed(ImGuiKey_Enter) && keyboard_selected >= 0) {
            *current_item = keyboard_selected;
            keyboard_selected = -1;
            CloseCurrentPopup();
            EndCombo();
            return true;
        }
        if (IsKeyPressed(ImGuiKey_UpArrow) && keyboard_selected > 0) {
            --keyboard_selected;
            keyboard_selected_now = true;
        }
        if (IsKeyPressed(ImGuiKey_DownArrow) && keyboard_selected < items_count - 1) {
            ++keyboard_selected;
            keyboard_selected_now = true;
        }

        // Display items
        bool value_changed = false;
        for (int i = 0; i < items_count; i++) {
            PushID(reinterpret_cast<void*>(i));
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
                SetScrollHereY();
            }
            if (item_keyboard_selected && keyboard_selected_now) {
                SetScrollHereY();
            }
            PopID();
        }

        EndCombo();
        return value_changed;
    }

    ImVec2 CalculateUvCrop(ImTextureID user_texture_id, const ImVec2& size) {
        ImVec2 uv1 = { 1.f,1.f };
        if (user_texture_id) {
            const auto texture = static_cast<IDirect3DTexture9*>(user_texture_id);
            D3DSURFACE_DESC desc;
            const HRESULT res = texture->GetLevelDesc(0, &desc);
            if (!SUCCEEDED(res))
                return uv1; // Don't throw anything into the log here; this function is called every frame by modules that use it!
            const float ratio = size.x / size.y;
            const float image_ratio = static_cast<float>(desc.Width) / static_cast<float>(desc.Height);
            if (image_ratio < ratio) {
                // Image is taller than the required crop; remove bottom of image to fit.
                uv1.y = ratio * image_ratio;
            }
            else if (image_ratio > ratio) {
                // Image is wider than the required crop; remove right of image to fit.
                uv1.x = ratio / image_ratio;
            }
        }
        return uv1;
    }

    void ImageCropped(ImTextureID user_texture_id, const ImVec2& size) {
        ImGui::Image(user_texture_id, size, { 0,0 }, CalculateUvCrop(user_texture_id,size));
    }
    void AddImageCropped(ImTextureID user_texture_id, const ImVec2& top_left, const ImVec2& bottom_right) {
        const ImVec2 size = { bottom_right.x - top_left.x, bottom_right.y - top_left.y };
        ImGui::GetWindowDrawList()->AddImage(user_texture_id, top_left, bottom_right, { 0,0 }, CalculateUvCrop(user_texture_id, size));
    }
    bool ColorPalette(const char* label, size_t* palette_index, const ImVec4* palette, size_t count, size_t max_per_line, ImGuiColorEditFlags flags)
    {
        PushID(label);
        BeginGroup();

        bool value_changed = false;
        for (size_t i = 0; i < count; i++) {
            PushID(i);
            if (ColorButton("", palette[i])) {
                *palette_index = i;
                value_changed = true;
            }
            PopID();
            if (((i + 1) % max_per_line) != 0)
                ImGui::SameLine();
        }

        if (flags & ImGuiColorEditFlags_AlphaPreview) {
            constexpr ImVec4 col;
            PushID(count);
            if (ColorButton("", col, ImGuiColorEditFlags_AlphaPreview)) {
                *palette_index = count;
                value_changed = true;
            }
            PopID();
        }

        EndGroup();
        PopID();
        return value_changed;
    }
}
