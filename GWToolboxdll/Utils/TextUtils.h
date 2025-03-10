#pragma once

#define __forceinline
#include <ctre.hpp>
#undef __forceinline

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

    template <typename CharT, ctll::fixed_string Pattern, typename... Modifiers>
    constexpr std::basic_string<CharT> ctre_simple_regex_replace(const std::basic_string_view<CharT> input, const std::basic_string_view<CharT> replacement)
    {
        auto r = ctre::split<Pattern, Modifiers...>(input);
        return r | std::views::join_with(replacement) | std::ranges::to<std::basic_string<CharT>>();
    }

    template <ctll::fixed_string Pattern, ctll::fixed_string Replacement, typename... Modifiers>
    constexpr std::string ctre_regex_replace(const std::string_view subject)
    {
        // this is actually SLOWER than the stringstream version, so don't use it.
        static constexpr ctll::fixed_string special_tokens = R"(\$(\d+|'|&|`|\$))";
        if constexpr (!ctre::search<special_tokens>(Replacement) && false) {
            return ctre_simple_regex_replace<char, Pattern, Modifiers...>(subject, Replacement);
        }

        std::string result;
        result.reserve(subject.size() * 2);
        auto search_start = subject.begin();
#pragma warning(push)
#pragma warning(disable: 4244)
        const auto replacement = Replacement | std::ranges::to<std::string>();
#pragma warning(pop)

        for (auto match : ctre::search_all<Pattern, Modifiers...>(subject)) {
            result.append(&*search_start, match.begin() - search_start);
            std::string replaced_match(replacement);
        struct Pair {
            std::string key;
            std::string value;
        };
        std::vector<Pair> replacements;

        constexpr auto cnt = decltype(match)::count();
        static_assert(cnt < 10, "Only up to 9 capture groups are supported");
        // Add replacements to vector with emplace_back
        constexpr auto has_escaped_dollar = ctre::search<R"(\$\$)">(Replacement);
        if constexpr (has_escaped_dollar)
            replacements.emplace_back("$$", "~$~");
        if constexpr (ctre::search<R"(\$&)">(Replacement))
            replacements.emplace_back("$&", match.to_string());
        if constexpr (ctre::search<R"(\$')">(Replacement))
            replacements.emplace_back("$'", std::string(match.end(), subject.end()));
        if constexpr (ctre::search<R"(\$`)">(Replacement))
            replacements.emplace_back("$`", std::string(subject.begin(), match.begin()));
        // if constexpr (ctre::search<R"(\$0)">(Replacement))
        //     replacements.emplace_back("$0", match.to_string());
        if constexpr (ctre::search<R"(\$1)">(Replacement) && cnt > 1)
            replacements.emplace_back("$1", match.template get<1>().to_string());
        if constexpr (ctre::search<R"(\$2)">(Replacement) && cnt > 2)
            replacements.emplace_back("$2", match.template get<2>().to_string());
        if constexpr (ctre::search<R"(\$3)">(Replacement) && cnt > 3)
            replacements.emplace_back("$3", match.template get<3>().to_string());
        if constexpr (ctre::search<R"(\$4)">(Replacement) && cnt > 4)
            replacements.emplace_back("$4", match.template get<4>().to_string());
        if constexpr (ctre::search<R"(\$5)">(Replacement) && cnt > 5)
            replacements.emplace_back("$5", match.template get<5>().to_string());
        if constexpr (ctre::search<R"(\$6)">(Replacement) && cnt > 6)
            replacements.emplace_back("$6", match.template get<6>().to_string());
        if constexpr (ctre::search<R"(\$7)">(Replacement) && cnt > 7)
            replacements.emplace_back("$7", match.template get<7>().to_string());
        if constexpr (ctre::search<R"(\$8)">(Replacement) && cnt > 8)
            replacements.emplace_back("$8", match.template get<8>().to_string());
        if constexpr (ctre::search<R"(\$9)">(Replacement) && cnt > 9)
            replacements.emplace_back("$9", match.template get<9>().to_string());
        if constexpr (has_escaped_dollar)
            replacements.emplace_back("~$~", "$");

            for (const auto& [key, value] : replacements) {
                size_t pos = 0;
                while ((pos = replaced_match.find(key, pos)) != std::string::npos) {
                    replaced_match.replace(pos, key.length(), value);
                    pos += value.length();
                }
            }

            result.append(replaced_match);
            search_start = match.end();
        }

        if (search_start != subject.end()) {
            result.append(&*search_start, std::distance(search_start, subject.end()));
        }
        return result;
    }

    template <ctll::fixed_string Pattern, ctll::fixed_string Replacement, typename... Modifiers>
    constexpr std::wstring ctre_regex_replace(const std::wstring_view subject)
    {
        // this is actually SLOWER than the stringstream version, so don't use it.
        static constexpr ctll::fixed_string special_tokens = LR"(\$(\d+|'|&|`|\$))";
        if constexpr (!ctre::search<special_tokens>(Replacement) && false) {
            return ctre_simple_regex_replace<char, Pattern, Modifiers...>(subject, Replacement);
        }

        std::wstring result;
        result.reserve(subject.size() * 2);
        auto search_start = subject.begin();
#pragma warning(push)
#pragma warning(disable: 4244)
        const auto replacement = Replacement | std::ranges::to<std::wstring>();
#pragma warning(pop)

        for (auto match : ctre::search_all<Pattern, Modifiers...>(subject)) {
            result.append(&*search_start, match.begin() - search_start);
            std::wstring replaced_match(replacement);
        struct Pair {
            std::wstring key;
            std::wstring value;
        };
        std::vector<Pair> replacements;

        constexpr auto cnt = decltype(match)::count();
        static_assert(cnt < 10, "Only up to 9 capture groups are supported");
        // Add replacements to vector with emplace_back
        constexpr auto has_escaped_dollar = ctre::search<LR"(\$\$)">(Replacement);
        if constexpr (has_escaped_dollar)
            replacements.emplace_back(L"$$", L"~$~");
        if constexpr (ctre::search<LR"(\$&)">(Replacement))
            replacements.emplace_back(L"$&", match.to_string());
        if constexpr (ctre::search<LR"(\$')">(Replacement))
            replacements.emplace_back(L"$'", std::string(match.end(), subject.end()));
        if constexpr (ctre::search<LR"(\$`)">(Replacement))
            replacements.emplace_back(L"$`", std::string(subject.begin(), match.begin()));
        // if constexpr (ctre::search<LR"(\$0)">(Replacement))
        //     replacements.emplace_back(L"$0", match.to_string());
        if constexpr (ctre::search<LR"(\$1)">(Replacement) && cnt > 1)
            replacements.emplace_back(L"$1", match.template get<1>().to_string());
        if constexpr (ctre::search<LR"(\$2)">(Replacement) && cnt > 2)
            replacements.emplace_back(L"$2", match.template get<2>().to_string());
        if constexpr (ctre::search<LR"(\$3)">(Replacement) && cnt > 3)
            replacements.emplace_back(L"$3", match.template get<3>().to_string());
        if constexpr (ctre::search<LR"(\$4)">(Replacement) && cnt > 4)
            replacements.emplace_back(L"$4", match.template get<4>().to_string());
        if constexpr (ctre::search<LR"(\$5)">(Replacement) && cnt > 5)
            replacements.emplace_back(L"$5", match.template get<5>().to_string());
        if constexpr (ctre::search<LR"(\$6)">(Replacement) && cnt > 6)
            replacements.emplace_back(L"$6", match.template get<6>().to_string());
        if constexpr (ctre::search<LR"(\$7)">(Replacement) && cnt > 7)
            replacements.emplace_back(L"$7", match.template get<7>().to_string());
        if constexpr (ctre::search<LR"(\$8)">(Replacement) && cnt > 8)
            replacements.emplace_back(L"$8", match.template get<8>().to_string());
        if constexpr (ctre::search<LR"(\$9)">(Replacement) && cnt > 9)
            replacements.emplace_back(L"$9", match.template get<9>().to_string());
        if constexpr (has_escaped_dollar)
            replacements.emplace_back(L"~$~", L"$");

            for (const auto& [key, value] : replacements) {
                size_t pos = 0;
                while ((pos = replaced_match.find(key, pos)) != std::wstring::npos) {
                    replaced_match.replace(pos, key.length(), value);
                    pos += value.length();
                }
            }

            result.append(replaced_match);
            search_start = match.end();
        }

        if (search_start != subject.end()) {
            result.append(&*search_start, std::distance(search_start, subject.end()));
        }
        return result;
    }

    template <ctll::fixed_string Pattern, typename Formatter, typename... Modifiers>
    std::wstring ctre_regex_replace_with_formatter(std::wstring_view input, Formatter formatter)
    {
        std::wstring result;
        result.reserve(input.size());
        size_t last_pos = 0;

        // Iterate through all matches
        for (auto match : ctre::search_all<Pattern, Modifiers...>(input)) {
            // Get the matched substring and its position
            auto matched = match.template get<0>().to_view();
            auto pos = std::distance(input.begin(), match.template get<0>().begin());

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

    inline std::string str_replace_all(std::string subject, const std::string_view needle, const std::string_view str)
    {
        size_t pos = 0;
        while ((pos = subject.find(needle, pos)) != std::string::npos) {
            subject.replace(pos, needle.length(), str);
            pos += str.length();
        }
        return subject;
    }

    inline std::wstring str_replace_all(std::wstring haystack, const std::wstring_view needle, const std::wstring_view str)
    {
        size_t pos = 0;
        while ((pos = haystack.find(needle, pos)) != std::wstring::npos) {
            haystack.replace(pos, needle.length(), str);
            pos += str.length();
        }
        return haystack;
    }
}
