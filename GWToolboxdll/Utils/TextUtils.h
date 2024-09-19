#pragma once

namespace TextUtils {
    std::string WStringToString(std::wstring_view str);
    std::wstring StringToWString(std::string_view str);
    std::string UrlEncode(std::string_view s, char space_token = '+');
    std::string HtmlEncode(std::string_view s);
    std::string SanitiseFilename(std::string_view str);
    std::wstring SanitiseFilename(std::wstring_view str);
    std::string PrintFilename(std::string path);
    std::wstring PrintFilename(std::wstring path);

    std::string GuidToString(const GUID* guid);

    std::string RemovePunctuation(std::string s);
    std::wstring RemovePunctuation(std::wstring s);
    std::string ToSlug(std::string s);
    std::wstring ToSlug(std::wstring s);
    std::string ToLower(std::string s);
    std::wstring ToLower(std::wstring s);
    std::wstring RemoveDiacritics(std::wstring_view s);

    std::wstring SanitizePlayerName(std::wstring_view str);
    std::wstring GetPlayerNameFromEncodedString(const wchar_t* message, const wchar_t** start_pos_out = nullptr, const wchar_t** end_pos_out = nullptr);

    bool ParseInt(const char* str, int* val, int base = 10);
    bool ParseInt(const wchar_t* str, int* val, int base = 10);
    bool ParseUInt(const char* str, unsigned int* val, int base = 10);
    bool ParseUInt(const wchar_t* str, unsigned int* val, int base = 10);
    bool ParseFloat(const char* str, float* val);
    bool ParseFloat(const wchar_t* str, float* val);

    size_t TimeToString(time_t utc_timestamp, std::string& out);
    size_t TimeToString(uint32_t utc_timestamp, std::string& out);
    size_t TimeToString(FILETIME utc_timestamp, std::string& out);
}
