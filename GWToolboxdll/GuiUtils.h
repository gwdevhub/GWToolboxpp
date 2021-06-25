#pragma once

#include <ImGuiAddons.h>

namespace GuiUtils {
    enum class FontSize {
        text,
        header2,
        header1,
        widget_label,
        widget_small,
        widget_large
    };
    void LoadFonts();
    void OpenWiki(std::wstring term);
    bool FontsLoaded();
    ImFont* GetFont(FontSize size);

    float GetPartyHealthbarHeight();
    float GetGWScaleMultiplier();

    // Reposition a rect within its container to make sure it isn't overflowing it.
    ImVec4& ClampRect(ImVec4& rect, ImVec4& viewport);

    std::string ToLower(std::string s);
    std::wstring ToLower(std::wstring s);
    std::string UrlEncode(std::string str);
    std::wstring RemovePunctuation(std::wstring s);
    std::string RemovePunctuation(std::string s);

    std::string WStringToString(const std::wstring& s);
    std::wstring StringToWString(const std::string& s);

    std::wstring SanitizePlayerName(std::wstring in);

    bool ParseInt(const char *str, int *val, int base = 0);
    bool ParseInt(const wchar_t *str, int *val, int base = 0);

    bool ParseUInt(const char *str, unsigned int *val, int base = 0);
    bool ParseUInt(const wchar_t *str, unsigned int *val, int base = 0);

    bool ParseFloat(const char *str, float *val);
    bool ParseFloat(const wchar_t *str, float *val);

    // Takes a wstring and translates into a string of hex values, separated by spaces
    bool ArrayToIni(const wchar_t* in, std::string* out);
    bool ArrayToIni(const uint32_t* in, size_t len, std::string* out);
    bool IniToArray(const std::string& in, wchar_t* out, size_t out_len);
    bool IniToArray(const std::string& in, uint32_t* out, size_t out_len);
    // Takes a string of hex values separated by spaces, and returns a wstring respresentation
    std::wstring IniToWString(std::string in);

    char *StrCopy(char *dest, const char *src, size_t dest_size);

    size_t wcstostr(char* dest, const wchar_t* src, size_t n);
    std::wstring ToWstr(std::string& str);

    class EncString {
    private:
        std::wstring encoded_ws;
        std::wstring decoded_ws;
        std::string decoded_s;
        bool decoding = false;
        bool sanitised = false;
    public:
        void reset(const uint32_t _enc_string_id = 0);
        void reset(const wchar_t* _enc_string = nullptr);
        const std::wstring& wstring();
        const std::string& string();
        const std::wstring& encoded() {
            return encoded_ws;
        };
        EncString(const wchar_t* _enc_string = nullptr) {
            reset(_enc_string);
        }
        EncString(const uint32_t _enc_string) {
            reset(_enc_string);
        }
    };
};
