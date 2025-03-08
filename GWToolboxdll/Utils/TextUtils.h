#pragma once

#include "ctre.hpp"

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
    std::string SanitizePlayerName(std::string_view str);
    std::wstring GetPlayerNameFromEncodedString(const wchar_t* message, const wchar_t** start_pos_out = nullptr, const wchar_t** end_pos_out = nullptr);

    bool ParseInt(const char* str, int* val, int base = 10);
    bool ParseInt(const wchar_t* str, int* val, int base = 10);
    bool ParseUInt(const char* str, unsigned int* val, int base = 10);
    bool ParseUInt(const wchar_t* str, unsigned int* val, int base = 10);
    bool ParseFloat(const char* str, float* val);
    bool ParseFloat(const wchar_t* str, float* val);

    std::string TimeToString(time_t utc_timestamp = 0, bool include_seconds = false);
    std::string TimeToString(uint32_t utc_timestamp, bool include_seconds = false);
    std::string TimeToString(FILETIME utc_timestamp, bool include_seconds = false);
    std::vector<std::string> Split(const std::string& in, const std::string& token);
    // Turn and array of strings into a single string
    std::string Join(const std::vector<std::string>& parts, const std::string& token);
    // Capitalise the first letter of each word. Replaces original.
    std::string UcWords(std::string_view input);


    template <ctll::fixed_string Pattern>
    std::string ctre_regex_replace(std::string_view input, const std::string_view replacement) {
        std::string result;
        // Pre-allocate memory to avoid frequent reallocations
        result.reserve(input.size());

        size_t last_pos = 0;

        // Iterate through all matches
        for (const auto match : ctre::search_all<Pattern>(input)) {
            // Get the matched substring and its position
            const auto matched_view = match.get<0>().to_view();
            const auto pos = std::distance(input.begin(), match.get<0>().begin());

            // Append text before the match
            result.append(input.substr(last_pos, pos - last_pos));
            // Append the replacement
            result.append(replacement);
            // Update position
            last_pos = pos + matched_view.size();
        }

        // Append the remaining text
        if (last_pos < input.size()) {
            result.append(input.substr(last_pos));
        }

        return result;
    }

    template <ctll::fixed_string Pattern>
    std::wstring ctre_regex_replace(const std::wstring_view input, const std::wstring_view replacement) {
        std::wstring result;
        // Pre-allocate memory to avoid frequent reallocations
        result.reserve(input.size());

        size_t last_pos = 0;

        // Iterate through all matches
        for (const auto match : ctre::search_all<Pattern>(input)) {
            // Get the matched substring and its position
            const auto matched_view = match.get<0>().to_view();
            const auto pos = std::distance(input.begin(), match.get<0>().begin());

            // Append text before the match
            result.append(input.substr(last_pos, pos - last_pos));
            // Append the replacement
            result.append(replacement);
            // Update position
            last_pos = pos + matched_view.size();
        }

        // Append the remaining text
        if (last_pos < input.size()) {
            result.append(input.substr(last_pos));
        }

        return result;
    }

    template <ctll::fixed_string Pattern, typename Formatter>
    std::wstring ctre_regex_replace_with_formatter(std::wstring_view input, Formatter formatter) {
        std::wstring result;
        result.reserve(input.size());
        size_t last_pos = 0;

        // Iterate through all matches
        for (auto match : ctre::search_all<Pattern>(input)) {
            // Get the matched substring and its position
            auto matched = match.get<0>().to_view();
            auto pos = std::distance(input.begin(), match.get<0>().begin());

            // Append text before the match
            result.append(input.substr(last_pos, pos - last_pos));
            // Apply custom formatter and append
            result.append(formatter(match));
            // Update position
            last_pos = pos + matched.length();
        }

        // Append the remaining text
        if (last_pos < input.size()) {
            result.append(input.substr(last_pos));
        }

        return result;
    }
}
