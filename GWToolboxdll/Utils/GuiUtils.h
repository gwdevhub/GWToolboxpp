#pragma once

// ReSharper disable once CppUnusedIncludeDirective
#include <ImGuiAddons.h>
#include <glaze/glaze.hpp>
#include <ToolboxIni.h>
#include <RectF.h>

// ReSharper disable once CppUnusedIncludeDirective
#include <Utils/EncString.h>
#include <Utils/TextUtils.h>


namespace GuiUtils {
    template <typename T>
    concept map_type =
        std::same_as<
            T,
            std::map<typename T::key_type, typename T::mapped_type, typename T::key_compare, typename T::allocator_type>> ||
        std::same_as<
            T,
            std::unordered_map<typename T::key_type, typename T::mapped_type, typename T::hasher, typename T::key_equal, typename T::allocator_type>>;

    template <map_type T>
    constexpr bool map_has_wstring_key = std::same_as<typename T::key_type, std::wstring>;

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
        // Glaze has no wchar_t serializer; stage wstring keys through UTF-8.
        if constexpr (map_has_wstring_key<T>) {
            std::map<std::string, typename T::mapped_type> staged;
            for (const auto& [k, v] : map) {
                staged.emplace(TextUtils::WStringToString(k), v);
            }
            const auto map_str = glz::write_json(staged).value_or(std::string{});
            ini->SetValue(section, name, map_str.c_str());
        }
        else {
            const auto map_str = glz::write_json(map).value_or(std::string{});
            ini->SetValue(section, name, map_str.c_str());
        }
    }

    template <map_type T>
    T IniToMap(ToolboxIni* ini, const char* section, const char* name)
    {
        std::string map_str = ini->GetValue(section, name, "");
        if constexpr (map_has_wstring_key<T>) {
            std::map<std::string, typename T::mapped_type> staged;
            if (glz::read_json(staged, map_str)) {
                return {};
            }
            T out{};
            for (auto& [k, v] : staged) {
                out.emplace(TextUtils::StringToWString(k), std::move(v));
            }
            return out;
        }
        else {
            T out{};
            if (glz::read_json(out, map_str)) {
                return {};
            }
            return out;
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
