#include "stdafx.h"
#include "TextUtils.h"

namespace {
    constexpr auto diacritics = std::to_array<const wchar_t*>({
        L"A\x0041\x0410\x24B6\xFF21\x00C0\x00C1\x00C2\x1EA6\x1EA4\x1EAA\x1EA8\x00C3\x0100\x0102\x1EB0\x1EAE\x1EB4\x1EB2\x0226\x01E0\x00C4\x01DE\x1EA2\x00C5\x01FA\x01CD\x0200\x0202\x1EA0\x1EAC\x1EB6\x1E00\x0104\x023A\x2C6F",
        L"B\x00DF\x0412\x0042\x24B7\xFF22\x1E02\x1E04\x1E06\x0243\x0182\x0181",
        L"C\x0421\x0043\x24B8\xFF23\x0106\x0108\x010A\x010C\x00C7\x1E08\x0187\x023B\xA73E",
        L"D\x0044\x24B9\xFF24\x1E0A\x010E\x1E0C\x1E10\x1E12\x1E0E\x0110\x018B\x018A\x0189\xA779\x00D0",
        L"E\x0401\x0045\x24BA\xFF25\x00C8\x00C9\x00CA\x1EC0\x1EBE\x1EC4\x1EC2\x1EBC\x0112\x1E14\x1E16\x0114\x0116\x00CB\x1EBA\x011A\x0204\x0206\x1EB8\x1EC6\x0228\x1E1C\x0118\x1E18\x1E1A\x0190\x018E",
        L"F\x0046\x24BB\xFF26\x1E1E\x0191\xA77B",
        L"G\u0047\u24BC\uFF27\u01F4\u011C\u1E20\u011E\u0120\u01E6\u0122\u01E4\u0193\uA7A0\uA77D\uA77E",
        L"H\u0048\u24BD\uFF28\u0124\u1E22\u1E26\u021E\u1E24\u1E28\u1E2A\u0126\u2C67\u2C75\uA78D",
        L"I\u0049\u24BE\uFF29\u00CC\u00CD\u00CE\u0128\u012A\u012C\u0130\u00CF\u1E2E\u1EC8\u01CF\u0208\u020A\u1ECA\u012E\u1E2C\u0197",
        L"J\u004A\u24BF\uFF2A\u0134\u0248",
        L"K\u041A\u004B\u24C0\uFF2B\u1E30\u01E8\u1E32\u0136\u1E34\u0198\u2C69\uA740\uA742\uA744\uA7A2",
        L"L\u004C\u24C1\uFF2C\u013F\u0139\u013D\u1E36\u1E38\u013B\u1E3C\u1E3A\u0141\u023D\u2C62\u2C60\uA748\uA746\uA780",
        L"M\u041C\u004D\u24C2\uFF2D\u1E3E\u1E40\u1E42\u2C6E\u019C",
        L"N\u004E\u24C3\uFF2E\u01F8\u0143\u00D1\u1E44\u0147\u1E46\u0145\u1E4A\u1E48\u0220\u019D\uA790\uA7A4",
        L"O\u004F\u24C4\uFF2F\u00D2\u00D3\u00D4\u1ED2\u1ED0\u1ED6\u1ED4\u00D5\u1E4C\u022C\u1E4E\u014C\u1E50\u1E52\u014E\u022E\u0230\u00D6\u022A\u1ECE\u0150\u01D1\u020C\u020E\u01A0\u1EDC\u1EDA\u1EE0\u1EDE\u1EE2\u1ECC\u1ED8\u01EA\u01EC\u00D8\u01FE\u0186\u019F\uA74A\uA74C",
        L"P\u0050\u24C5\uFF30\u1E54\u1E56\u01A4\u2C63\uA750\uA752\uA754",
        L"Q\u0051\u24C6\uFF31\uA756\uA758\u024A",
        L"R\u0052\u24C7\uFF32\u0154\u1E58\u0158\u0210\u0212\u1E5A\u1E5C\u0156\u1E5E\u024C\u2C64\uA75A\uA7A6\uA782",
        L"S\u0053\u24C8\uFF33\u1E9E\u015A\u1E64\u015C\u1E60\u0160\u1E66\u1E62\u1E68\u0218\u015E\u2C7E\uA7A8\uA784",
        L"T\u0054\u0422\u24C9\uFF34\u1E6A\u0164\u1E6C\u021A\u0162\u1E70\u1E6E\u0166\u01AC\u01AE\u023E\uA786",
        L"U\u0055\u24CA\uFF35\u00D9\u00DA\u00DB\u0168\u1E78\u016A\u1E7A\u016C\u00DC\u01DB\u01D7\u01D5\u01D9\u1EE6\u016E\u0170\u01D3\u0214\u0216\u01AF\u1EEA\u1EE8\u1EEE\u1EEC\u1EF0\u1EE4\u1E72\u0172\u1E76\u1E74\u0244",
        L"V\u0056\u24CB\uFF36\u1E7C\u1E7E\u01B2\uA75E\u0245",
        L"W\u0057\u24CC\uFF37\u1E80\u1E82\u0174\u1E86\u1E84\u1E88\u2C72",
        L"X\u0058\u24CD\uFF38\u1E8A\u1E8C",
        L"Y\u0059\u24CE\uFF39\u1EF2\u00DD\u0176\u1EF8\u0232\u1E8E\u0178\u1EF6\u1EF4\u01B3\u024E\u1EFE",
        L"Z\u005A\u24CF\uFF3A\u0179\u1E90\u017B\u017D\u1E92\u1E94\u01B5\u0224\u2C7F\u2C6B\uA762",
        L"a\u0061\u24D0\uFF41\u1E9A\u00E0\u00E1\u00E2\u1EA7\u1EA5\u1EAB\u1EA9\u00E3\u0101\u0103\u1EB1\u1EAF\u1EB5\u1EB3\u0227\u01E1\u00E4\u01DF\u1EA3\u00E5\u01FB\u01CE\u0201\u0203\u1EA1\u1EAD\u1EB7\u1E01\u0105\u2C65\u0250\u03b1",
        L"b\u0062\u24D1\uFF42\u1E03\u1E05\u1E07\u0180\u0183\u0253",
        L"c\u0063\u24D2\uFF43\u0107\u0109\u010B\u010D\u00E7\u1E09\u0188\u023C\uA73F\u2184",
        L"d\u0064\u24D3\uFF44\u1E0B\u010F\u1E0D\u1E11\u1E13\u1E0F\u0111\u018C\u0256\u0257\uA77A",
        L"e\u0065\u24D4\uFF45\u00E8\u00E9\u00EA\u1EC1\u1EBF\u1EC5\u1EC3\u1EBD\u0113\u1E15\u1E17\u0115\u0117\u00EB\u1EBB\u011B\u0205\u0207\u1EB9\u1EC7\u0229\u1E1D\u0119\u1E19\u1E1B\u0247\u025B\u01DD",
        L"f\u0066\u24D5\uFF46\u1E1F\u0192\uA77C",
        L"g\u0067\u24D6\uFF47\u01F5\u011D\u1E21\u011F\u0121\u01E7\u0123\u01E5\u0260\uA7A1\u1D79\uA77F",
        L"h\u0068\u24D7\uFF48\u0125\u1E23\u1E27\u021F\u1E25\u1E29\u1E2B\u1E96\u0127\u2C68\u2C76\u0265",
        L"i\u0069\u24D8\uFF49\u00EC\u00ED\u00EE\u0129\u012B\u012D\u00EF\u1E2F\u1EC9\u01D0\u0209\u020B\u1ECB\u012F\u1E2D\u0268\u0131",
        L"j\u006A\u24D9\uFF4A\u0135\u01F0\u0249",
        L"k\u006B\u24DA\uFF4B\u1E31\u01E9\u1E33\u0137\u1E35\u0199\u2C6A\uA741\uA743\uA745\uA7A3",
        L"l\u006C\u24DB\uFF4C\u0140\u013A\u013E\u1E37\u1E39\u013C\u1E3D\u1E3B\u017F\u0142\u019A\u026B\u2C61\uA749\uA781\uA747",
        L"m\u006D\u24DC\uFF4D\u1E3F\u1E41\u1E43\u0271\u026F\u043C",
        L"n\u006E\u24DD\uFF4E\u01F9\u0144\u00F1\u1E45\u0148\u1E47\u0146\u1E4B\u1E49\u019E\u0272\u0149\uA791\uA7A5",
        L"o\u006F\u24DE\uFF4F\u00F2\u00F3\u00F4\u1ED3\u1ED1\u1ED7\u1ED5\u00F5\u1E4D\u022D\u1E4F\u014D\u1E51\u1E53\u014F\u022F\u0231\u00F6\u022B\u1ECF\u0151\u01D2\u020D\u020F\u01A1\u1EDD\u1EDB\u1EE1\u1EDF\u1EE3\u1ECD\u1ED9\u01EB\u01ED\u00F8\u01FF\u0254\uA74B\uA74D\u0275",
        L"p\u0070\u24DF\uFF50\u1E55\u1E57\u01A5\u1D7D\uA751\uA753\uA755",
        L"q\u0071\u24E0\uFF51\u024B\uA757\uA759",
        L"r\u0072\u24E1\uFF52\u0155\u1E59\u0159\u0211\u0213\u1E5B\u1E5D\u0157\u1E5F\u024D\u027D\uA75B\uA7A7\uA783",
        L"s\u0073\u24E2\uFF53\u015B\u1E65\u015D\u1E61\u0161\u1E67\u1E63\u1E69\u0219\u015F\u023F\uA7A9\uA785\u1E9B",
        L"t\u03C4\u0074\u24E3\uFF54\u1E6B\u1E97\u0165\u1E6D\u021B\u0163\u1E71\u1E6F\u0167\u01AD\u0288\u2C66\uA787",
        L"u\u0075\u24E4\uFF55\u00F9\u00FA\u00FB\u0169\u1E79\u016B\u1E7B\u016D\u00FC\u01DC\u01D8\u01D6\u01DA\u1EE7\u016F\u0171\u01D4\u0215\u0217\u01B0\u1EEB\u1EE9\u1EEF\u1EED\u1EF1\u1EE5\u1E73\u0173\u1E77\u1E75\u0289",
        L"v\u0076\u24E5\uFF56\u1E7D\u1E7F\u028B\uA75F\u028C\u03BD",
        L"w\u0077\u24E6\uFF57\u1E81\u1E83\u0175\u1E87\u1E85\u1E98\u1E89\u2C73\u03C9",
        L"x\u0078\u24E7\uFF58\u1E8B\u1E8D",
        L"y\u0079\u24E8\uFF59\u1EF3\u00FD\u0177\u1EF9\u0233\u1E8F\u00FF\u1EF7\u1E99\u1EF5\u01B4\u024F\u1EFF\u0443",
        L"z\u007A\u24E9\uFF5A\u017A\u1E91\u017C\u017E\u1E93\u1E95\u01B6\u0225\u0240\u2C6C\uA763"
    });
    std::map<wchar_t, wchar_t> diacritics_charmap;

    time_t filetime_to_timet(const FILETIME& ft)
    {
        const ULARGE_INTEGER ull{ft.dwLowDateTime, ft.dwHighDateTime};
        return ull.QuadPart / 10000000ULL - 11644473600ULL;
    }
}

namespace TextUtils {
    std::string RemovePunctuation(std::string s)
    {
        std::erase_if(s, [](auto c) { return std::ispunct(c, std::locale()); });
        return s;
    }

    std::wstring RemovePunctuation(std::wstring s)
    {
        std::erase_if(s, [](auto c) { return std::ispunct(c, std::locale()); });
        return s;
    }

    std::string ToSlug(std::string s)
    {
        s = RemovePunctuation(s);
        std::ranges::transform(s, s.begin(), [](const char c) {
            if (c == ' ') {
                return '_';
            }
            return std::tolower(c, std::locale());
        });
        return s;
    }

    std::wstring ToSlug(std::wstring s)
    {
        s = RemovePunctuation(s);
        std::ranges::transform(s, s.begin(), [](const wchar_t c) {
            if (c == L' ') {
                return L'_';
            }
            return std::tolower(c, std::locale());
        });
        return s;
    }

    std::string ToLower(std::string s)
    {
        std::ranges::transform(s, s.begin(), [](const char c) {
            return std::tolower(c, std::locale());
        });
        return s;
    }

    std::wstring ToLower(std::wstring s)
    {
        std::ranges::transform(s, s.begin(), [](const wchar_t c) {
            return std::tolower(c, std::locale());
        });
        return s;
    }


    std::wstring StripTags(std::wstring_view str)
    {
        return ctre_regex_replace<L"<[^>]+>", L"">(str);
    }

    std::string HtmlEncode(const std::string_view s)
    {
        if (s.empty()) {
            return "";
        }
        static char entities[256] = {0};
        static bool initialised = false;
        if (!initialised) {
            for (uint8_t i = 0; i < 255; i++) {
                switch (i) {
                    case '\'':
                    case '"':
                    case '&':
                    case '>':
                    case '<':
                        break;
                    default:
                        entities[i] = i;
                }
            }
            initialised = true;
        }
        char out[256]{};
        size_t len = 0;
        const char* in = s.data();
        for (size_t i = 0; in[i] && len < 255; i++) {
            if (entities[in[i]]) {
                out[len++] = entities[in[i]];
            }
            else {
                const auto written = snprintf(&out[len], 255 - len, "&#%d;", in[i]);
                if (written < 0) {
                    return ""; // @Cleanup; how to gracefully fail?
                }
                len += written;
            }
        }
        out[len] = 0;
        return out;
    }

    std::string UrlEncode(const std::string_view s, const char space_token)
    {
        if (s.empty()) {
            return "";
        }
        static char html5[256]{};
        static bool initialised = false;
        if (!initialised) {
            for (uint8_t i = 0; ; i++) {
                html5[i] = isalnum(i) || i == '*' || i == '-' || i == '.' || i == '_' ? i : 0;
                if (i == ' ') {
                    html5[i] = space_token;
                }
                if (i == 255) {
                    break;
                }
            }
            initialised = true;
        }
        char out[256]{};
        size_t len = 0;
        const char* in = s.data();
        for (size_t i = 0; in[i] && len < 255; i++) {
            if (html5[in[i]]) {
                out[len++] = html5[in[i]];
            }
            else {
                const auto written = snprintf(&out[len], 255 - len, "%%%02X", in[i]);
                if (written < 0) {
                    return ""; // @Cleanup; how to gracefully fail?
                }
                len += written;
            }
        }
        out[len] = 0;
        return out;
    }

    std::string GuidToString(const GUID* guid)
    {
        ASSERT(guid);
        char guid_string[37]; // 32 hex chars + 4 hyphens + null terminator
        snprintf(
            guid_string, sizeof(guid_string),
            "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
            guid->Data1, guid->Data2, guid->Data3,
            guid->Data4[0], guid->Data4[1], guid->Data4[2],
            guid->Data4[3], guid->Data4[4], guid->Data4[5],
            guid->Data4[6], guid->Data4[7]);
        return guid_string;
    }

    // Convert an UTF8 string to a wide Unicode String
    std::wstring StringToWString(const std::string_view str)
    {
        // @Cleanup: ASSERT used incorrectly here; value passed could be from anywhere!
        if (str.empty()) {
            return {};
        }
        // NB: GW uses code page 0 (CP_ACP)
        constexpr int try_code_pages[] = {CP_UTF8, CP_ACP};
        for (const auto code_page : try_code_pages) {
            const auto size_needed = MultiByteToWideChar(code_page, MB_ERR_INVALID_CHARS, str.data(), static_cast<int>(str.size()), nullptr, 0);
            if (!size_needed)
                continue;
            std::wstring dest(size_needed, 0);
            ASSERT(MultiByteToWideChar(code_page, 0, str.data(), static_cast<int>(str.size()), dest.data(), size_needed));
            return dest;
        }
        ASSERT("Failed to convert" && false);
        return {};
    }

    // Convert a wide Unicode string to an UTF8 string
    std::string WStringToString(const std::wstring_view str)
    {
        // @Cleanup: ASSERT used incorrectly here; value passed could be from anywhere!
        if (str.empty()) {
            return "";
        }
        // NB: GW uses code page 0 (CP_ACP)
        constexpr int try_code_pages[] = {CP_UTF8, CP_ACP};
        for (const auto code_page : try_code_pages) {
            const auto size_needed = WideCharToMultiByte(code_page, WC_ERR_INVALID_CHARS, str.data(), static_cast<int>(str.size()), nullptr, 0, nullptr, nullptr);
            if (!size_needed)
                continue;
            std::string dest(size_needed, 0);
            ASSERT(WideCharToMultiByte(code_page, 0, str.data(), static_cast<int>(str.size()), dest.data(), size_needed, nullptr, nullptr));
            return dest;
        }
        ASSERT("Failed to convert" && false);
        return {};
    }

    // Makes sure the file name doesn't have chars that won't be allowed on disk
    // https://docs.microsoft.com/en-gb/windows/win32/fileio/naming-a-file
    std::string SanitiseFilename(const std::string_view str)
    {
        const auto invalid_chars = "<>:\"/\\|?*";
        size_t len = 0;
        std::string out;
        out.resize(str.length());
        for (const char i : str) {
            if (strchr(invalid_chars, i)) {
                continue;
            }
            out[len] = i;
            len++;
        }
        out.resize(len);
        return out;
    }

    std::wstring SanitiseFilename(const std::wstring_view str)
    {
        const auto invalid_chars = L"<>:\"/\\|?*";
        size_t len = 0;
        std::wstring out;
        out.resize(str.length());
        for (const wchar_t i : str) {
            if (wcschr(invalid_chars, i)) {
                continue;
            }
            out[len] = i;
            len++;
        }
        out.resize(len);
        return out;
    }

    std::string PrintFilename(std::string path)
    {
        for (auto& c : path) {
            if (c == '\\') c = '/';
        }
        return path;
    }

    std::wstring PrintFilename(std::wstring path)
    {
        for (auto& c : path) {
            if (c == L'\\') c = L'/';
        }
        return path;
    }

    std::string Base64Decode(std::string_view encoded)
    {
        const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string decoded;
        std::vector<int> T(256, -1);

        for (int i = 0; i < 64; i++)
            T[chars[i]] = i;

        int val = 0, valb = -8;
        for (unsigned char c : encoded) {
            if (T[c] == -1) break;
            val = (val << 6) + T[c];
            valb += 6;
            if (valb >= 0) {
                decoded.push_back(char((val >> valb) & 0xFF));
                valb -= 8;
            }
        }
        return decoded;
    }

    std::wstring RemoveDiacritics(const std::wstring_view s)
    {
        if (diacritics_charmap.empty()) {
            // Build static diacritics map if not already done so
            for (size_t i = 0; i < diacritics.size(); i++) {
                for (size_t j = 1; diacritics[i][j]; j++) {
                    diacritics_charmap[diacritics[i][j]] = diacritics[i][0];
                }
            }
        }
        std::wstring out(s.length(), L'\0');
        std::ranges::transform(s, out.begin(), [&](const wchar_t wc) -> wchar_t {
            if (wc < 0x7f) {
                return wc;
            }
            const auto it = diacritics_charmap.find(wc);
            return it == diacritics_charmap.end() ? wc : it->second;
        });
        return out;
    }

    std::string SanitizePlayerName(const std::string_view str)
    {
        return WStringToString(SanitizePlayerName(StringToWString(str)));
    }

    std::wstring SanitizePlayerName(const std::wstring_view str)
    {
        std::wstring result;
        wchar_t remove_char_token = 0;

        for (const auto& wchar : str) {
            if (remove_char_token) {
                if (wchar == remove_char_token) {
                    remove_char_token = 0; // End removal mode if closing character is found
                }
                continue; // Skip characters inside the brackets/parentheses
            }
            if (wchar == L'[') {
                remove_char_token = L']'; // Set to skip until the closing bracket
                if (!result.empty()) {
                    result.pop_back(); // Remove the space before the opening bracket if needed
                }
                continue;
            }
            if (wchar == L'(') {
                remove_char_token = L')'; // Set to skip until the closing parenthesis
                if (!result.empty()) {
                    result.pop_back();
                }
                continue;
            }
            result.push_back(wchar); // Add valid character to result
        }

        return result;
    }

    // Extract first unencoded substring from gw encoded string. Pass second and third args to know where the player name was found in the original string.
    std::wstring GetPlayerNameFromEncodedString(const wchar_t* message, const wchar_t** start_pos_out, const wchar_t** end_pos_out)
    {
        const wchar_t* start = wcschr(message, 0x107);
        if (!start) {
            return L"";
        }
        start += 1;
        const wchar_t* end = start ? wcschr(start, 0x1) : nullptr;
        if (!end) {
            return L"";
        }
        if (start_pos_out) {
            *start_pos_out = start;
        }
        if (end_pos_out) {
            *end_pos_out = end;
        }
        const std::wstring name(start, end);
        return SanitizePlayerName(name);
    }

    bool ParseInt(const char* str, int* val, const int base)
    {
        char* end;
        *val = strtol(str, &end, base);
        if (*end != 0 || errno == ERANGE) {
            return false;
        }

        return true;
    }

    bool ParseInt(const wchar_t* str, int* val, const int base)
    {
        wchar_t* end;
        *val = wcstol(str, &end, base);
        if (*end != 0 || errno == ERANGE) {
            return false;
        }

        return true;
    }

    bool ParseUInt(const char* str, unsigned int* val, const int base)
    {
        char* end;
        if (!str) {
            return false;
        }
        *val = strtoul(str, &end, base);
        if (str == end || errno == ERANGE) {
            return false;
        }
        return true;
    }

    bool ParseUInt(const wchar_t* str, unsigned int* val, const int base)
    {
        wchar_t* end;
        if (!str) {
            return false;
        }
        *val = wcstoul(str, &end, base);
        if (str == end || errno == ERANGE) {
            return false;
        }
        return true;
    }

    bool ParseFloat(const char* str, float* val)
    {
        char* end;
        *val = strtof(str, &end);
        return str != end && errno != ERANGE;
    }

    bool ParseFloat(const wchar_t* str, float* val)
    {
        wchar_t* end;
        *val = wcstof(str, &end);
        return str != end && errno != ERANGE;
    }

    std::string TimeToString(time_t utc_timestamp, bool include_seconds)
    {
        const time_t now = time(nullptr);
        if (!utc_timestamp) {
            utc_timestamp = now;
        }
        const tm* timeinfo = localtime(&utc_timestamp);
        const tm* nowinfo = localtime(&now);

        if (!timeinfo || !nowinfo) return "Invalid time";

        std::string out;
        if (timeinfo->tm_yday != nowinfo->tm_yday || timeinfo->tm_year != nowinfo->tm_year) {
            static constexpr const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
            out += std::format("{} {:02d} ", months[timeinfo->tm_mon], timeinfo->tm_mday);
        }
        if (timeinfo->tm_year != nowinfo->tm_year) {
            out += std::format("{} ", timeinfo->tm_year + 1900);
        }
        out += std::format("{:02}:{:02}", timeinfo->tm_hour, timeinfo->tm_min);
        if (include_seconds) {
            out += std::format(":{:02}", timeinfo->tm_sec);
        }
        return out;
    }

    std::string TimeToString(const uint32_t utc_timestamp, bool include_seconds)
    {
        return TimeToString(static_cast<time_t>(utc_timestamp), include_seconds);
    }

    std::string TimeToString(const FILETIME utc_timestamp, bool include_seconds)
    {
        return TimeToString(filetime_to_timet(utc_timestamp), include_seconds);
    }

    std::vector<std::string> Split(const std::string& in, const std::string& token)
    {
        std::vector<std::string> result;
        size_t start = 0;
        size_t pos = 0;

        while ((pos = in.find(token, start)) != std::string::npos) {
            std::string part = in.substr(start, pos - start);
            if (!part.empty()) {
                // Skip empty substrings
                result.push_back(part);
            }
            start = pos + token.length();
        }

        // Add the last remaining part if it's not empty
        std::string lastPart = in.substr(start);
        if (!lastPart.empty()) {
            result.push_back(lastPart);
        }

        return result;
    }

    std::string Join(const std::vector<std::string>& parts, const std::string& token)
    {
        std::string result;
        bool first = true; // Track if it's the first valid part

        for (const auto& part : parts) {
            if (!part.empty()) {
                if (!first) {
                    result += token; // Add the delimiter before non-first parts
                }
                result += part;
                first = false; // Switch after adding the first valid part
            }
        }

        return result;
    }

    std::string UcWords(const std::string_view input)
    {
#pragma warning(push)
#pragma warning(disable : 4244)
        std::string result(input); // Create a copy of the input
        bool capitalizeNext = true;

        for (size_t i = 0; i < result.length(); ++i) {
            if (std::isspace(static_cast<unsigned char>(result[i]))) {
                capitalizeNext = true; // Set flag to capitalize next letter after space
            }
            else if (capitalizeNext && std::isalpha(static_cast<unsigned char>(result[i]))) {
                result[i] = std::toupper(static_cast<unsigned char>(result[i]));
                capitalizeNext = false; // Reset flag after capitalization
            }
            else {
                result[i] = std::tolower(static_cast<unsigned char>(result[i])); // Ensure other characters are lowercase
            }
        }
#pragma warning(pop)
        return result; // Return the modified string
    }

    std::string rtrim(const std::string& s, const char* t)
    {
        auto cpy = s;
        const auto found = s.find_last_not_of(t);
        if (found == std::string::npos) {
            cpy.clear();
        }
        else {
            cpy.erase(found + 1);
        }
        return cpy;
    }

    std::string ltrim(const std::string& s, const char* t)
    {
        auto cpy = s;
        const auto found = s.find_first_not_of(t);
        if (found == std::string::npos) {
            cpy.clear();
        }
        else {
            cpy.erase(0, found);
        }

        return cpy;
    }

    std::string trim(const std::string& s, const char* t)
    {
        return ltrim(rtrim(s, t), t);
    }
}
