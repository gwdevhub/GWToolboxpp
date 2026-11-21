#include "stdafx.h"

#include <Utils/TextUtils.h>

// SearchPattern is the one part of TextUtils that catches an exception: std::regex reports a pattern that doesn't
// compile only by throwing. This file is built with exceptions on so the rest of TextUtils can do without them.
namespace TextUtils {
    // "/pattern/flags", the syntax the chat filter has always used; unknown flag letters are ignored.
    template <typename CharT>
    SearchPattern<CharT>::SearchPattern(const std::basic_string_view<CharT> pattern, const Fallback fallback, std::regex_constants::syntax_option_type flags)
        : source(pattern)
    {
        const auto last_slash = pattern.rfind(static_cast<CharT>('/'));
        const bool slash_wrapped = pattern.starts_with(static_cast<CharT>('/')) && last_slash != 0 && last_slash != std::basic_string_view<CharT>::npos;
        if (!slash_wrapped && fallback != Fallback::Regex) {
            lowered = ToLower(std::basic_string<CharT>(pattern));
            exact = fallback == Fallback::Exact;
            return;
        }
        auto expression = pattern;
        if (slash_wrapped) {
            expression = pattern.substr(1, last_slash - 1);
            for (const auto chr : pattern.substr(last_slash + 1)) {
                switch (chr) {
                    case 'i': flags |= std::regex_constants::icase; break;
                    case 'I': flags &= ~std::regex_constants::icase; break;
                    case 'c': flags |= std::regex_constants::collate; break;
                    case 'n': flags |= std::regex_constants::nosubs; break;
                    case 's': flags |= std::regex_constants::ECMAScript; break;
                    case 'b': flags |= std::regex_constants::basic; break;
                    case 'x': flags |= std::regex_constants::extended; break;
                    case 'a': flags |= std::regex_constants::awk; break;
                    case 'g': flags |= std::regex_constants::grep; break;
                    case 'e': flags |= std::regex_constants::egrep; break;
                    default: break;
                }
            }
        }
        try {
            regex.emplace(expression.begin(), expression.end(), flags);
        } catch (const std::regex_error&) {
            valid = false;
        }
    }

    template <typename CharT>
    bool SearchPattern<CharT>::Matches(const std::basic_string_view<CharT> subject) const
    {
        if (!valid) return false;
        if (regex) return std::regex_search(subject.begin(), subject.end(), *regex);
        if (lowered.empty()) return false;
        static constexpr auto fold_table = [] {
            std::array<CharT, 256> table{};
            for (size_t i = 0; i < 256; i++) {
                const auto c = static_cast<CharT>(i);
                table[i] = c >= static_cast<CharT>('A') && c <= static_cast<CharT>('Z') ? static_cast<CharT>(c | static_cast<CharT>(0x20)) : c;
            }
            return table;
        }();
        // `lowered` is already lowercased, so only the subject needs folding as we go.
        const auto folded_equals = [](const CharT a, const CharT b) {
            return (a >= 0 && static_cast<size_t>(a) < 256 ? fold_table[a] : a) == b;
        };
        if (exact) return std::ranges::equal(subject, lowered, folded_equals);
        return !std::ranges::search(subject, lowered, folded_equals).empty();
    }

    template <typename CharT>
    std::vector<SearchPattern<CharT>> ParsePatterns(const std::basic_string_view<CharT> text, const typename SearchPattern<CharT>::Fallback fallback, const std::regex_constants::syntax_option_type flags)
    {
        std::vector<SearchPattern<CharT>> patterns;
        std::basic_istringstream<CharT> stream{std::basic_string<CharT>(text)};
        std::basic_string<CharT> line;
        while (std::getline(stream, line)) {
            if (line.empty()) {
                continue;
            }
            patterns.emplace_back(line, fallback, flags);
        }
        return patterns;
    }

    template class SearchPattern<char>;
    template class SearchPattern<wchar_t>;
    template std::vector<SearchPattern<char>> ParsePatterns(std::basic_string_view<char>, SearchPattern<char>::Fallback, std::regex_constants::syntax_option_type);
    template std::vector<SearchPattern<wchar_t>> ParsePatterns(std::basic_string_view<wchar_t>, SearchPattern<wchar_t>::Fallback, std::regex_constants::syntax_option_type);
}
