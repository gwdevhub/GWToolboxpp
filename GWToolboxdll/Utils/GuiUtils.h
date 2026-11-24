#pragma once

// ReSharper disable once CppUnusedIncludeDirective
#include <optional>

#include <ImGuiAddons.h>
#include <Utils/Json.h>
#include <ToolboxIni.h>
#include <RectF.h>

// ReSharper disable once CppUnusedIncludeDirective
#include <Utils/EncString.h>
#include <Utils/TextUtils.h>

namespace GW {
    namespace SkillbarMgr {
        struct SkillTemplate;
    }
}

namespace GuiUtils {
    template <typename T>
    concept map_type =
        std::same_as<
            T,
            std::map<typename T::key_type, typename T::mapped_type, typename T::key_compare, typename T::allocator_type>> ||
        std::same_as<
            T,
            std::unordered_map<typename T::key_type, typename T::mapped_type, typename T::hasher, typename T::key_equal, typename T::allocator_type>>;

    std::string WikiUrl(const std::wstring& term);
    std::string WikiUrl(const std::string& term);
    std::string WikiTemplateUrlFromTitle(const std::string& title);
    std::string WikiTemplateUrlFromTitle(const std::wstring& title);
    void OpenWiki(const std::wstring& term);
    void SearchWiki(const std::wstring& term);
    std::string SanitizeWikiUrl(std::string s);

    float GetGWScaleMultiplier(bool force = false);

    // Reposition a rect within its container to make sure it isn't overflowing it.
    ImVec4& ClampRect(ImVec4& rect, const ImVec4& viewport);

    // Takes a wstring and translates into a string of hex values, separated by spaces.
    // Lossless: handles all wchar_t values including lone Unicode surrogates present in GW encoded strings.
    bool ArrayToIni(const std::wstring& in, std::string* out);
    bool ArrayToIni(const uint32_t* in, size_t len, std::string* out);
    size_t IniToArray(const std::string& in, std::wstring& out);

    template <map_type T>
    T IniToMap(ToolboxIni* ini, const char* section, const char* name)
    {
        const std::string map_str = ini->GetValue(section, name, "");
        if (map_str.empty()) {
            return {};
        }

        using key_type = typename T::key_type;
        using staged_key = std::conditional_t<std::is_same_v<key_type, std::wstring>, std::string, key_type>;
        using staged_map = std::map<staged_key, typename T::mapped_type>;

        staged_map staged;
        if (glz::read<SizeOptimisedJson{}>(staged, map_str)) {
            staged.clear();
            std::vector<std::tuple<staged_key, typename T::mapped_type>> legacy;
            if (glz::read<SizeOptimisedJson{}>(legacy, map_str)) {
                return {};
            }
            for (auto& [k, v] : legacy) {
                staged.emplace(std::move(k), std::move(v));
            }
        }

        T out;
        for (auto& [k, v] : staged) {
            if constexpr (std::is_same_v<key_type, std::wstring>) {
                std::wstring wkey;
                if (IniToArray(k, wkey) == 0) {
                    // Backward compat: key was written in old UTF-8 format before hex encoding was introduced
                    wkey = TextUtils::StringToWString(k);
                }
                out.emplace(std::move(wkey), std::move(v));
            } else {
                out.emplace(std::move(k), std::move(v));
            }
        }
        return out;
    }

    template <map_type T>
    T IniToMap(ToolboxIni* ini, const char* section, const char* name, T default_map)
    {
        if (!ini->KeyExists(section, name)) {
            return std::move(default_map);
        }
        return IniToMap<T>(ini, section, name);
    }
    void BitsetToIni(const std::bitset<256>& key_combo, std::string& out_str);
    void IniToBitset(const std::string& str, std::bitset<256>& key_combo);
    // Convert token separated cstring into array of strings
    size_t IniToArray(const char* in, std::vector<std::string>& out, char separator = ',');
    size_t IniToArray(const std::string& in, uint32_t* out, size_t out_len);
    size_t IniToArray(const std::string& in, std::vector<uint32_t>& out);

    char* StrCopy(char* dest, const char* src, size_t dest_size);

    size_t wcstostr(char* dest, const wchar_t* src, size_t n);
    std::wstring ToWstr(const std::string& str);

    // Will flash GW Window if not already in focus. Pass force = true to flash regardless of focus.
    void FlashWindow(bool force = false);
    void FocusWindow();

    void DrawSkillbar(const char* build_code, bool show_attributes = true);

    void DrawSkillbar(const GW::SkillbarMgr::SkillTemplate* skill_template_pt, bool show_attributes = true);

    int DecimalPlaces(float value, int max_places = 4);

    enum class GwButtonIcon { Save, LoadFromTemplate, SaveToTemplate, ManageTemplates, TemplateCode, ChatIcon };
    bool IconButton(const char* label, GwButtonIcon icon, const ImVec2& size = ImVec2(0, 0), const ImGuiButtonFlags flags = ImGuiButtonFlags_None);
    bool IconButtonConfirm(const char* label, GwButtonIcon icon, const ImVec2& size = ImVec2(0, 0), const ImGuiButtonFlags flags = ImGuiButtonFlags_None);

    template <typename Items>
    bool SwapWithNeighbour(Items& items, const size_t index, const bool up)
    {
        if (up ? index == 0 : index + 1 >= items.size()) return false;
        std::swap(items[index], items[up ? index - 1 : index + 1]);
        return true;
    }

    template <typename Items>
    bool ReorderDragDrop(Items& items, const size_t index, const char* payload_type)
    {
        bool moved = false;
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
            ImGui::SetDragDropPayload(payload_type, &index, sizeof(index));
            ImGui::TextUnformatted("Move");
            ImGui::EndDragDropSource();
        }
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(payload_type)) {
                const size_t from = *static_cast<const size_t*>(payload->Data);
                if (from != index && from < items.size()) {
                    std::swap(items[from], items[index]);
                    moved = true;
                }
            }
            ImGui::EndDragDropTarget();
        }
        return moved;
    }

    template <typename Items>
    bool ReorderButtons(Items& items, const size_t index, const std::optional<size_t> up, const std::optional<size_t> down, const float button_width, const float spacing, const char* payload_type)
    {
        bool moved = false;
        if (up) {
            const bool move_up = ImGui::Button(ICON_FA_ARROW_UP, ImVec2(button_width, 0));
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Move up (or drag)");
            }
            if (move_up) {
                std::swap(items[index], items[*up]);
                moved = true;
            }
            moved |= ReorderDragDrop(items, index, payload_type);
            ImGui::SameLine(0.f, spacing);
        }
        else {
            ImGui::SameLine(0.f, button_width + spacing * 2);
        }

        if (down) {
            const bool move_down = ImGui::Button(ICON_FA_ARROW_DOWN, ImVec2(button_width, 0));
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Move down (or drag)");
            }
            if (move_down) {
                std::swap(items[index], items[*down]);
                moved = true;
            }
            moved |= ReorderDragDrop(items, index, payload_type);
            ImGui::SameLine(0.f, spacing);
        }
        else {
            ImGui::SameLine(0.f, button_width + spacing * 2);
        }
        return moved;
    }

    template <typename Items>
    bool ReorderButtons(Items& items, const size_t index, const size_t count, const float button_width, const float spacing, const char* payload_type)
    {
        const std::optional<size_t> up = index > 0 ? std::optional<size_t>(index - 1) : std::nullopt;
        const std::optional<size_t> down = index + 1 < count ? std::optional<size_t>(index + 1) : std::nullopt;
        return ReorderButtons(items, index, up, down, button_width, spacing, payload_type);
    }
};
