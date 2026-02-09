#pragma once

// ReSharper disable once CppUnusedIncludeDirective
#include <ImGuiAddons.h>
#include <nlohmann/json.hpp>
#include <ToolboxIni.h>

namespace GW::Constants {
    enum class Language;
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

    float GetPartyHealthbarHeight();
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

    class EncString {
    protected:
        std::wstring encoded_ws;
        std::wstring decoded_ws;
        std::string decoded_s;
        bool decoding = false;
        bool decoded = false;
        bool sanitised = false;
        bool release = false;
        virtual void sanitise();
        virtual void decode();
        GW::Constants::Language language_id = static_cast<GW::Constants::Language>(0xff);
        static void OnStringDecoded(void* param, const wchar_t* decoded);

    public:
        // Set the language for decoding this encoded string. If the language has changed, resets the decoded result. Returns this for chaining.
        EncString* language(GW::Constants::Language l);
        bool IsDecoding() const { return decoding && decoded_ws.empty(); };
        // Recycle this EncString by passing a new encoded string id to decode.
        // Set sanitise to true to automatically remove guild tags etc from the string
        EncString* reset(uint32_t _enc_string_id = 0, bool sanitise = true);
        // Recycle this EncString by passing a new string to decode.
        // Set sanitise to true to automatically remove guild tags etc from the string
        EncString* reset(const wchar_t* _enc_string = nullptr, bool sanitise = true);
        std::wstring& wstring();
        std::string& string();

        // Free memory used by this EncString. 
        void Release();

        [[nodiscard]] const std::wstring& encoded() const
        {
            return encoded_ws;
        };

        EncString(const wchar_t* _enc_string = nullptr, const bool sanitise = true)
        {
            reset(_enc_string, sanitise);
        }

        EncString(const uint32_t _enc_string, const bool sanitise = true)
        {
            reset(_enc_string, sanitise);
        }

        // Disable object copying; decoded_ws is passed to GW by reference and would be bad to do this. Pass by pointer instead.
        EncString(const EncString& temp_obj) = delete;
        EncString& operator=(const EncString& temp_obj) = delete;
        ~EncString();
    };

    // Create an ImGui representation of the skill bar
    void DrawSkillbar(const char* build_code, bool show_attributes = true);

    int DecimalPlaces(float value, int max_places = 4);
};
