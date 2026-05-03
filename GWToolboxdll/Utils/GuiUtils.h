#pragma once

// ReSharper disable once CppUnusedIncludeDirective
#include <ImGuiAddons.h>
#include <nlohmann/json.hpp>
#include <ToolboxIni.h>

// ReSharper disable once CppUnusedIncludeDirective
#include <Utils/EncString.h>

struct RectF {
    ImVec2 top_left;
    ImVec2 bottom_right;

    constexpr RectF(const RECT& rect)
        : top_left({static_cast<float>(rect.left), static_cast<float>(rect.top)}),
          bottom_right({static_cast<float>(rect.right), static_cast<float>(rect.bottom)}) {}

    constexpr RectF(const ImVec2& _top_left, const ImVec2& _bottom_right)
        : top_left(_top_left), bottom_right(_bottom_right) {}

    ImVec2 size() const
    {
        return {width(), height()};
    }

    float width() const
    {
        return bottom_right.x - top_left.x;
    }

    float height() const
    {
        return bottom_right.y - top_left.y;
    }

    void move_to(const ImVec2& new_top_left)
    {
        const auto diff_x = new_top_left.x - top_left.x;
        const auto diff_y = new_top_left.y - top_left.y;
        top_left.x += diff_x;
        top_left.y += diff_y;
        bottom_right.x += diff_x;
        bottom_right.y += diff_y;
    }

    void resize(const ImVec2& new_size)
    {
        const auto old_size = size();
        const auto diff_x = new_size.x - old_size.x;
        const auto diff_y = new_size.y - old_size.y;
        bottom_right.x += diff_x;
        bottom_right.y += diff_y;
    }

    bool contains(const ImVec2& point) const
    {
        return point.x >= top_left.x && point.x <= bottom_right.x &&
               point.y >= top_left.y && point.y <= bottom_right.y;
    }

    RectF() = default;
};


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

    template <map_type T>
    void MapToIni(ToolboxIni* ini, const char* section, const char* name, const T& map)
    {
        const auto map_json = nlohmann::json(map);
        const auto map_str = map_json.dump();
        ini->SetValue(section, name, map_str.c_str());
    }

    template <map_type T>
    T IniToMap(ToolboxIni* ini, const char* section, const char* name)
    {
        std::string map_str = ini->GetValue(section, name, "");
        try {
            const auto map_json = nlohmann::json::parse(map_str);
            return map_json.get<T>();
        } catch (nlohmann::json::exception e) {
            return {};
        }
    }

    template <map_type T>
    T IniToMap(ToolboxIni* ini, const char* section, const char* name, T default_map)
    {
        if (!ini->KeyExists(section, name)) {
            return std::move(default_map);
        }
        return IniToMap<T>(ini, section, name);
    }

    // Takes a wstring and translates into a string of hex values, separated by spaces
    bool ArrayToIni(const std::wstring& in, std::string* out);
    bool ArrayToIni(const uint32_t* in, size_t len, std::string* out);
    size_t IniToArray(const std::string& in, std::wstring& out);
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

    // Same as std::format, but use printf formatting
    std::string format(const char* msg, ...);
    // Same as std::format, but use printf formatting
    std::wstring format(const wchar_t* msg, ...);

    // Create an ImGui representation of the skill bar
    void DrawSkillbar(const char* build_code, bool show_attributes = true);

    int DecimalPlaces(float value, int max_places = 4);
};
