#pragma once

// ReSharper disable once CppUnusedIncludeDirective
#include <ImGuiAddons.h>
#include <nlohmann/json.hpp>
#include <ToolboxIni.h>

#ifndef DLLAPI
#ifdef GWCA_IMPORT
#define DLLAPI __declspec(dllimport)
#else
#define DLLAPI extern "C" __declspec(dllexport)
#endif
#endif

namespace GW {
    namespace Constants {
        enum class TextLanguage;
    }
}

namespace GuiUtils {
    template <typename T>
    concept map_type = std::same_as<T,
        std::map<typename T::key_type, typename T::mapped_type, typename T::key_compare, typename T::allocator_type>> ||
        std::same_as<T, std::unordered_map<typename T::key_type, typename T::mapped_type, typename T::hasher,
                            typename T::key_equal, typename T::allocator_type>>;

    enum class FontSize {
        text,
        header2,
        header1,
        widget_label,
        widget_small,
        widget_large
    };
    void LoadFonts();
    std::string WikiUrl(std::wstring term);
    std::string WikiUrl(std::string term);
    void OpenWiki(const std::wstring& term);
    void SearchWiki(std::wstring term);
    bool FontsLoaded();
    DLLAPI ImFont* GetFont(FontSize size);

    float GetPartyHealthbarHeight();
    float GetGWScaleMultiplier(bool force = false);

    // Reposition a rect within its container to make sure it isn't overflowing it.
    ImVec4& ClampRect(ImVec4& rect, const ImVec4& viewport);

    std::string ToSlug(std::string s);
    std::wstring ToSlug(std::wstring s);
    std::string ToLower(std::string s);
    std::wstring ToLower(std::wstring s);
    std::string UrlEncode(std::string s, char space_token = '_');
    std::string HtmlEncode(std::string s);
    std::wstring RemovePunctuation(std::wstring s);
    std::string RemovePunctuation(std::string s);
    std::wstring RemoveDiacritics(const std::wstring& s);

    std::string WStringToString(const std::wstring& s);
    std::wstring StringToWString(const std::string& s);
    std::string SanitiseFilename(const std::string& str);
    std::wstring SanitiseFilename(const std::wstring& str);

    std::wstring SanitizePlayerName(std::wstring s);

    // Extract first unencoded substring from gw encoded string. Pass second and third args to know where the player name was found in the original string.
    std::wstring GetPlayerNameFromEncodedString(const wchar_t* message, const wchar_t** start_pos_out = nullptr, const wchar_t** out_pos_out = nullptr);

    bool ParseInt(const char *str, int *val, int base = 0);
    bool ParseInt(const wchar_t *str, int *val, int base = 0);

    bool ParseUInt(const char *str, unsigned int *val, int base = 0);
    bool ParseUInt(const wchar_t *str, unsigned int *val, int base = 0);

    bool ParseFloat(const char *str, float *val);
    bool ParseFloat(const wchar_t *str, float *val);

    time_t filetime_to_timet(const FILETIME& ft);
    size_t TimeToString(uint32_t utc_timestamp, std::string& out);
    size_t TimeToString(time_t utc_timestamp, std::string& out);
    size_t TimeToString(FILETIME utc_timestamp, std::string& out);

    template <map_type T>
    void MapToIni(ToolboxIni* ini, const char* section, const char* name, const T& map) {
        const auto map_json = nlohmann::json(map);
        const auto map_str = map_json.dump();
        ini->SetValue(section, name, map_str.c_str());
    }

    template <map_type T>
    T IniToMap(ToolboxIni* ini, const char* section, const char* name) {
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
    // Convert token separated cstring into array of strings
    size_t IniToArray(const char* in, std::vector<std::string>& out, const char separator = ',');
    size_t IniToArray(const std::string& in, uint32_t* out, size_t out_len);
    size_t IniToArray(const std::string& in, std::vector<uint32_t>& out);
    // Takes a string of hex values separated by spaces, and returns a wstring respresentation
    std::wstring IniToWString(std::string in);

    char *StrCopy(char *dest, const char *src, size_t dest_size);

    size_t wcstostr(char* dest, const wchar_t* src, size_t n);
    std::wstring ToWstr(std::string& str);

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
        virtual void sanitise();
        GW::Constants::TextLanguage language_id = (GW::Constants::TextLanguage)-1;
        static void OnStringDecoded(void* param, wchar_t* decoded);
    public:
        // Set the language for decoding this encoded string. If the language has changed, resets the decoded result. Returns this for chaining.
        EncString* language(GW::Constants::TextLanguage l = (GW::Constants::TextLanguage)-1);
        bool IsDecoding() { return decoding && decoded_ws.empty(); };
        // Recycle this EncString by passing a new encoded string id to decode.
        // Set sanitise to true to automatically remove guild tags etc from the string
        void reset(uint32_t _enc_string_id = 0, bool sanitise = true);
        // Recycle this EncString by passing a new string to decode.
        // Set sanitise to true to automatically remove guild tags etc from the string
        void reset(const wchar_t* _enc_string = nullptr, bool sanitise = true);
        std::wstring& wstring();
        std::string& string();
        const std::wstring& encoded() const {
            return encoded_ws;
        };
        EncString(const wchar_t* _enc_string = nullptr, bool sanitise = true) {
            reset(_enc_string, sanitise);
        }
        EncString(const uint32_t _enc_string, bool sanitise = true) {
            reset(_enc_string, sanitise);
        }
        // Disable object copying; decoded_ws is passed to GW by reference and would be bad to do this. Pass by pointer instead.
        EncString(const EncString& temp_obj) = delete;
        EncString& operator=(const EncString& temp_obj) = delete;
        virtual ~EncString() = default;
    };
};
