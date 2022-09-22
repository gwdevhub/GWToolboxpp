#include "stdafx.h"

#include <GWCA/Constants/Constants.h>

#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/MemoryMgr.h>

#include <Utf8.h>
#include <fonts/fontawesome5.h>
#include <Modules/Resources.h>

#include "GuiUtils.h"

namespace {
    ImFont* font_widget_large = nullptr;
    ImFont* font_widget_small = nullptr;
    ImFont* font_widget_label = nullptr;
    ImFont* font_header1 = nullptr;
    ImFont* font_header2 = nullptr;
    ImFont* font_text = nullptr;

    bool fonts_loading = false;
    bool fonts_loaded = false;

    const char* GetWikiPrefix() {
        /*uint32_t language = GW::UI::GetPreference(GW::UI::Preference_TextLanguage);
        char* wiki_prefix = "https://wiki.guildwars.com/wiki/";
        switch (static_cast<GW::Constants::MapLanguage>(language)) {
        case GW::Constants::MapLanguage::German: // German wiki
            wiki_prefix = "https://www.guildwiki.de/wiki/";
            break;
        case GW::Constants::MapLanguage::French: // French wiki
            wiki_prefix = "https://www.gwiki.fr/w/index.php";
            break;

        }
        return wiki_prefix;*/
        return "https://wiki.guildwars.com/wiki/";
    }

    int safe_ispunct(char c) {
        if (c >= -1 && c <= 255)
            return ispunct(c);
        return 0;
    };

}
namespace GuiUtils {
    void FlashWindow() {
        FLASHWINFO flashInfo = { 0 };
        flashInfo.cbSize = sizeof(FLASHWINFO);
        flashInfo.hwnd = GW::MemoryMgr::GetGWWindowHandle();
        if (!flashInfo.hwnd) return;
        flashInfo.dwFlags = FLASHW_TRAY | FLASHW_TIMERNOFG;
        flashInfo.uCount = 5;
        flashInfo.dwTimeout = 0;
        FlashWindowEx(&flashInfo);
    }
    void FocusWindow() {
        HWND hwnd = GW::MemoryMgr::GetGWWindowHandle();
        if (!hwnd) return;
        SetForegroundWindow(hwnd);
        ShowWindow(hwnd, SW_RESTORE);
    }
    std::string WikiUrl(std::string url_path) {
        return WikiUrl(StringToWString(url_path));
    }
    std::string WikiUrl(std::wstring url_path) {

        // @Cleanup: Should really properly url encode the string here, but modern browsers clean up after our mess. Test with Creme Brulees.
        if (!url_path.size())
            return GetWikiPrefix();
        char cmd[256];
        std::string encoded = UrlEncode(WStringToString(RemoveDiacritics(url_path)),'_');
        snprintf(cmd, _countof(cmd), "%s%s", GetWikiPrefix(), encoded.c_str());
        return cmd;
    }
    void SearchWiki(std::wstring term) {
        if (!term.size())
            return;
        char cmd[256];
        std::string encoded = UrlEncode(WStringToString(RemoveDiacritics(term)));
        ASSERT(snprintf(cmd, _countof(cmd), "%s?search=%s", GetWikiPrefix(), encoded.c_str()) != -1);
        GW::UI::SendUIMessage(GW::UI::UIMessage::kOpenWikiUrl, (void*)cmd);
    }
    void OpenWiki(std::wstring url_path) {
        GW::UI::SendUIMessage(GW::UI::UIMessage::kOpenWikiUrl, (void*)WikiUrl(url_path).c_str());
    }
    // Has LoadFonts() finished?
    bool FontsLoaded() {
        return fonts_loaded;
    }
    // Loads fonts asynchronously. CJK font files can by over 20mb in size!
    void LoadFonts() {
        if (fonts_loaded || fonts_loading)
            return;
        fonts_loading = true;

        std::thread t([] {
            printf("Loading fonts\n");

            ImGuiIO& io = ImGui::GetIO();

            std::vector<std::pair<const wchar_t*, const ImWchar*>> fonts_on_disk;
            fonts_on_disk.emplace_back(L"Font.ttf", io.Fonts->GetGlyphRangesDefault());
            fonts_on_disk.emplace_back(L"Font_Japanese.ttf", io.Fonts->GetGlyphRangesJapanese());
            fonts_on_disk.emplace_back(L"Font_Cyrillic.ttf", io.Fonts->GetGlyphRangesCyrillic());
            fonts_on_disk.emplace_back(L"Font_ChineseTraditional.ttf", io.Fonts->GetGlyphRangesChineseFull());
            fonts_on_disk.emplace_back(L"Font_Korean.ttf", io.Fonts->GetGlyphRangesKorean());

            struct FontData
            {
                const ImWchar* glyph_ranges = nullptr;
                size_t data_size = 0;
                void* data = nullptr;
            };
            std::vector<FontData> fonts;
            for (size_t i = 0; i < fonts_on_disk.size(); ++i) {
                const auto& f = fonts_on_disk[i];
                utf8::string utf8path = Resources::GetPathUtf8(f.first);
                size_t size;
                void* data = ImFileLoadToMemory(utf8path.bytes, "rb", &size, 0);
                if (data) {
                    fonts.push_back({ f.second, size, data });
                }
                else if (i == 0) {
                    // first one cannot fail
                    printf("Failed to find Font.ttf file\n");
                    fonts_loaded = true;
                    fonts_loading = false;
                    return;
                }
            }

            // TODO: expose those in UI
            constexpr float size_text = 16.0f;
            constexpr float size_header1 = 18.0f;
            constexpr float size_header2 = 20.0f;
            constexpr float size_widget_label = 24.0f;
            constexpr float size_widget_small = 42.0f;
            constexpr float size_widget_large = 48.0f;
            static constexpr ImWchar fontawesome5_glyph_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };

            ImFontConfig cfg = ImFontConfig();
            cfg.MergeMode = false;
            cfg.PixelSnapH = true;
            cfg.FontDataOwnedByAtlas = true;
            cfg.OversampleH = 2; // OversampleH = 2 for base text size (harder to read if OversampleH < 2)
            cfg.OversampleV = 1;
            for (const auto& font : fonts) {
                io.Fonts->AddFontFromMemoryTTF(font.data, font.data_size, size_text, &cfg, font.glyph_ranges);
                cfg.MergeMode = true; // for all but the first
            }
            io.Fonts->AddFontFromMemoryCompressedTTF(
                fontawesome5_compressed_data, fontawesome5_compressed_size, size_text, &cfg, fontawesome5_glyph_ranges);
            font_text = io.Fonts->Fonts.back();

            // All other fonts re-used the data
            cfg.FontDataOwnedByAtlas = false;

            const auto& base = fonts.front(); // base font

            cfg.OversampleH = 1; // OversampleH = 1 makes the font look a bit more blurry, but halves the size in memory
            cfg.MergeMode = false;
            io.Fonts->AddFontFromMemoryTTF(base.data, base.data_size, size_header1, &cfg, base.glyph_ranges);
            cfg.MergeMode = true;
            io.Fonts->AddFontFromMemoryCompressedTTF(
                fontawesome5_compressed_data, fontawesome5_compressed_size, size_header1, &cfg, fontawesome5_glyph_ranges);
            font_header2 = io.Fonts->Fonts.back();

            cfg.MergeMode = false;
            io.Fonts->AddFontFromMemoryTTF(base.data, base.data_size, size_header2, &cfg, base.glyph_ranges);

            cfg.MergeMode = true;
            io.Fonts->AddFontFromMemoryCompressedTTF(
                fontawesome5_compressed_data, fontawesome5_compressed_size, size_header2, &cfg, fontawesome5_glyph_ranges);
            font_header1 = io.Fonts->Fonts.back();

            cfg.MergeMode = false;

            io.Fonts->AddFontFromMemoryTTF(base.data, base.data_size, size_widget_label, &cfg, base.glyph_ranges);
            font_widget_label = io.Fonts->Fonts.back();

            io.Fonts->AddFontFromMemoryTTF(base.data, base.data_size, size_widget_small, &cfg, base.glyph_ranges);
            font_widget_small = io.Fonts->Fonts.back();

            io.Fonts->AddFontFromMemoryTTF(base.data, base.data_size, size_widget_large, &cfg, base.glyph_ranges);
            font_widget_large = io.Fonts->Fonts.back();

            if (!io.Fonts->IsBuilt())
                io.Fonts->Build();
            // Also create device objects here to avoid blocking the main draw loop when ImGui_ImplDX9_NewFrame() is called.
            // ImGui_ImplDX9_CreateDeviceObjects();

            printf("Fonts loaded\n");
            fonts_loaded = true;
            fonts_loading = false;
            });
        t.detach();
    }
    ImFont* GetFont(FontSize size) {
        ImFont* font = [](FontSize size) -> ImFont* {
            switch (size) {
            case FontSize::widget_large: return font_widget_large;
            case FontSize::widget_small: return font_widget_small;
            case FontSize::widget_label: return font_widget_label;
            case FontSize::header1: return font_header1;
            case FontSize::header2: return font_header2;
            case FontSize::text: return font_text;
            default: return nullptr;
            }
        }(size);

        if (font && font->IsLoaded()) {
            return font;
        }

        return ImGui::GetIO().Fonts->Fonts[0];
    }
    float GetGWScaleMultiplier() {
        const auto interfacesize =
            static_cast<GW::Constants::InterfaceSize>(GW::UI::GetPreference(GW::UI::EnumPreference::InterfaceSize));

        switch (interfacesize) {
        case GW::Constants::InterfaceSize::SMALL: return .9f;
        case GW::Constants::InterfaceSize::LARGE: return 1.166666f;
        case GW::Constants::InterfaceSize::LARGER: return 1.3333333f;
        default: return 1.f;
        }
    }
    ImVec4& ClampRect(ImVec4& rect, const ImVec4& viewport) {
        float correct;
        // X axis
        if (rect.x < viewport.x) {
            correct = viewport.x - rect.x;
            rect.x += correct;
            rect.z += correct;
        }
        if (rect.z > viewport.z) {
            correct = rect.z - viewport.z;
            rect.x -= correct;
            rect.z -= correct;
        }
        // Y axis
        if (rect.y < viewport.y) {
            correct = viewport.y - rect.y;
            rect.y += correct;
            rect.w += correct;
        }
        if (rect.w > viewport.w) {
            correct = rect.w - viewport.w;
            rect.y -= correct;
            rect.w -= correct;
        }
        return rect;
    }
    float GetPartyHealthbarHeight() {
        const auto interfacesize =
            static_cast<GW::Constants::InterfaceSize>(GW::UI::GetPreference(GW::UI::EnumPreference::InterfaceSize));
        switch (interfacesize) {
        case GW::Constants::InterfaceSize::SMALL: return GW::Constants::HealthbarHeight::Small;
        case GW::Constants::InterfaceSize::NORMAL: return GW::Constants::HealthbarHeight::Normal;
        case GW::Constants::InterfaceSize::LARGE: return GW::Constants::HealthbarHeight::Large;
        case GW::Constants::InterfaceSize::LARGER: return GW::Constants::HealthbarHeight::Larger;
        default:
            return GW::Constants::HealthbarHeight::Normal;
        }
    }
    std::string ToSlug(std::string s) {
        s = RemovePunctuation(s);
        std::ranges::transform(s, s.begin(), [](char c) -> char {
            if (c == ' ')
                return '_';
            return static_cast<char>(::tolower(c));
            });
        return s;
    }
    std::wstring ToSlug(std::wstring s) {
        s = RemovePunctuation(s);
        std::ranges::transform(s, s.begin(), [](wchar_t c) -> wchar_t {
            if (c == ' ')
                return '_';
            return static_cast<wchar_t>(::tolower(c));
            });
        return s;
    }
    std::string ToLower(std::string s) {
        std::ranges::transform(s, s.begin(), [](char c) -> char {
            return static_cast<char>(::tolower(c));
            });
        return s;
    }

    std::wstring ToLower(std::wstring s) {
        std::ranges::transform(s, s.begin(), [](wchar_t c) -> wchar_t {
            return static_cast<wchar_t>(::tolower(c));
            });
        return s;
    }
    std::string HtmlEncode(std::string s) {
        if (s.empty())
            return "";
        static char entities[256] = { 0 };
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
        char out[256];
        size_t len = 0;
        const char* in = s.c_str();
        for (size_t i = 0; in[i] && len < 255; i++) {
            if (entities[in[i]]) {
                out[len++] = entities[in[i]];
            }
            else {
                const int written = snprintf(&out[len], 255 - len, "&#%d;", in[i]);
                if (written < 0)
                    return ""; // @Cleanup; how to gracefully fail?
                len += written;
            }
        }
        out[len] = 0;
        return out;
    }
    std::string UrlEncode(const std::string s, const char space_token) {
        if (s.empty())
            return "";
        static char html5[256] = { 0 };
        static bool initialised = false;
        if (!initialised) {
            for (uint8_t i = 0; ; i++) {
                html5[i] = (isalnum(i) || i == '*' || i == '-' || i == '.' || i == '_') ? i : 0;
                if (i == ' ') {
                    html5[i] = space_token;
                }
                if (i == 255) break;
            }
            initialised = true;
        }
        char out[256];
        size_t len = 0;
        const char* in = s.c_str();
        for (size_t i = 0; in[i] && len < 255; i++) {
            if (html5[in[i]]) {
                out[len++] = html5[in[i]];
            }
            else {
                const int written = snprintf(&out[len], 255 - len, "%%%02X", in[i]);
                if (written < 0)
                    return ""; // @Cleanup; how to gracefully fail?
                len += written;
            }
        }
        out[len] = 0;
        return out;
    }

    // Convert a wide Unicode string to an UTF8 string
    std::string WStringToString(const std::wstring& s)
    {
        // @Cleanup: ASSERT used incorrectly here; value passed could be from anywhere!
        if (s.empty()) return "";
        // NB: GW uses code page 0 (CP_ACP)
        int size_needed = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, &s[0], (int)s.size(), NULL, 0, NULL, NULL);
        ASSERT(size_needed != 0);
        std::string strTo(size_needed, 0);
        ASSERT(WideCharToMultiByte(CP_UTF8, 0, &s[0], (int)s.size(), &strTo[0], size_needed, NULL, NULL));
        return strTo;
    }
    // Makes sure the file name doesn't have chars that won't be allowed on disk
    // https://docs.microsoft.com/en-gb/windows/win32/fileio/naming-a-file
    std::string SanitiseFilename(const std::string& str) {
        const char* invalid_chars = "<>:\"/\\|?*";
        size_t len = 0;
        std::string out;
        out.resize(str.length());
        for (size_t i = 0; i < str.length(); i++) {
            if (strchr(invalid_chars, str[i]))
                continue;
            out[len] = str[i];
            len++;
        }
        out.resize(len);
        return out;
    }

    std::wstring RemoveDiacritics(const std::wstring& s) {
        static std::map<wchar_t, wchar_t> charmap;
        if (charmap.empty()) {
            const wchar_t* diacritics[] =
            {
                L"aàáảãạăằắẳẵặâầấẩẫậ",
                L"AÀÁẢÃẠĂẰẮẲẴẶÂẦẤẨẪẬ",
                L"OÒÒÓỎÕỌÔỒỐỔỖỘƠỜỚỞỠỢ",
                L"EÈÉẺẼẸÊỀẾỂỄỆ",
                L"UÙÚỦŨỤƯỪỨỬỮỰ",
                L"IÌÍỈĨỊ",
                L"YỲÝỶỸỴ",
                L"DĐ",
                L"oòóỏõọôồốổỗộơờớởỡợ",
                L"eèéẻẽẹêềếểễệ",
                L"uùúủũụưừứửữựû",
                L"iìíỉĩị",
                L"yỳýỷỹỵ",
                L"dđ"
            };
            for (size_t i = 0; i < _countof(diacritics); i++) {
                for (size_t j = 1; diacritics[i][j]; j++) {
                    charmap[diacritics[i][j]] = diacritics[i][0];
                }
            }
        }
        std::wstring out(s.length(), L'\0');
        std::ranges::transform(s, out.begin(), [&](wchar_t wc) ->wchar_t {
            auto it = charmap.find(wc);
            return it == charmap.end() ? wc : it->second; });
        return out;
    }
    // Convert an UTF8 string to a wide Unicode String
    std::wstring StringToWString(const std::string& str)
    {
        // @Cleanup: ASSERT used incorrectly here; value passed could be from anywhere!
        if (str.empty()) return std::wstring();
        // NB: GW uses code page 0 (CP_ACP)
        int size_needed = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, &str[0], (int)str.size(), NULL, 0);
        ASSERT(size_needed != 0);
        std::wstring wstrTo(static_cast<size_t>(size_needed), 0);
        ASSERT(MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed));
        return wstrTo;
    }
    std::wstring SanitizePlayerName(std::wstring s) {
        // e.g. "Player Name (2)" => "Player Name", for pvp player names
        // e.g. "Player Name [TAG]" = >"Player Name", for alliance message sender name
        wchar_t out[64];
        size_t len = 0;
        wchar_t remove_char_token = 0;
        for (wchar_t& wchar : s) {
            if (remove_char_token) {
                if (wchar == remove_char_token)
                    remove_char_token = 0;
                continue;
            }
            if (wchar == '[') {
                remove_char_token = ']';
                if (len > 0)
                    len--;
                continue;
            }
            if (wchar == '(') {
                remove_char_token = ')';
                if (len > 0)
                    len--;
                continue;
            }
            out[len++] = wchar;
        }
        out[len++] = 0;
        return std::wstring(out);
    }
    std::wstring GetPlayerNameFromEncodedString(const wchar_t* message, const wchar_t** start_pos_out, const wchar_t** end_pos_out)
    {
        const wchar_t* start = wcschr(message, 0x107);
        if (!start) return L"";
        start += 1;
        const wchar_t* end = start ? wcschr(start, 0x1) : nullptr;
        if (!end) return L"";
        if (start_pos_out) *start_pos_out = start;
        if (end_pos_out) *end_pos_out = end;
        std::wstring name(start, end);
        return SanitizePlayerName(name);
    }
    std::string RemovePunctuation(std::string s) {
        std::erase_if(s, &safe_ispunct);
        return s;
    }
    std::wstring RemovePunctuation(std::wstring s) {
        std::erase_if(s, &ispunct);
        return s;
    }
    bool ParseInt(const char* str, int* val, int base) {
        char* end;
        *val = strtol(str, &end, base);
        if (*end != 0 || errno == ERANGE)
            return false;

        return true;
    }
    bool ParseInt(const wchar_t* str, int* val, int base) {
        wchar_t* end;
        *val = wcstol(str, &end, base);
        if (*end != 0 || errno == ERANGE)
            return false;

        return true;
    }
    bool ParseFloat(const char* str, float* val) {
        char* end;
        *val = strtof(str, &end);
        return str != end && errno != ERANGE;
    }
    bool ParseFloat(const wchar_t* str, float* val) {
        wchar_t* end;
        *val = wcstof(str, &end);
        return str != end && errno != ERANGE;
    }
    size_t IniToArray(const std::string& in, std::wstring& out) {
        out.resize((in.size() + 1) / 5);
        size_t offset = 0, pos = 0, converted = 0;
        do {
            if (pos) pos++;
            std::string substring(in.substr(pos, 4));
            if (!ParseUInt(substring.c_str(), &converted, 16))
                return 0;
            if (converted > 0xFFFF)
                return 0;
            out[offset++] = (wchar_t)converted;
        } while ((pos = in.find(' ', pos)) != std::string::npos);
        return offset;
    }
    size_t IniToArray(const std::string& in, std::vector<uint32_t>& out) {
        out.resize((in.size() + 1) / 9);
        out.resize(IniToArray(in, out.data(), out.size()));
        return out.size();
    }
    size_t IniToArray(const std::string& in, uint32_t* out, size_t out_len) {
        if ((in.size() + 1) / 9 > out_len)
            return 0;
        size_t offset = 0, pos = 0, converted = 0;
        do {
            if (pos) pos++;
            std::string substring(in.substr(pos, 8));
            if (!ParseUInt(substring.c_str(), &converted, 16))
                return 0;
            out[offset++] = converted;
        } while ((pos = in.find(' ', pos)) != std::string::npos);
        while (offset < out_len) {
            out[offset++] = 0;
        }
        return offset;
    }
    size_t TimeToString(time_t utc_timestamp, std::string& out) {
        struct tm* timeinfo = localtime(&utc_timestamp);
        if (!timeinfo)
            return 0;
        time_t now = time(NULL);
        struct tm* nowinfo = localtime(&now);
        if (!nowinfo)
            return 0;
        out.resize(64);
        int written = 0;
        if (timeinfo->tm_yday != nowinfo->tm_yday || timeinfo->tm_year != nowinfo->tm_year) {
            const char* months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
            written += snprintf(&out[written], out.capacity() - written,
                "%s %02d", months[timeinfo->tm_mon], timeinfo->tm_mday);
        }
        if (timeinfo->tm_year != nowinfo->tm_year) {
            written += snprintf(&out[written], out.capacity() - written, " %d", timeinfo->tm_year + 1900);
        }
        written += snprintf(&out[written], out.capacity() - written, written > 0 ? ", %02d:%02d" : " %02d:%02d",
            timeinfo->tm_hour, timeinfo->tm_min);
        return out.size();
    }
    size_t TimeToString(uint32_t utc_timestamp, std::string& out) {
        return TimeToString((time_t)utc_timestamp, out);
    }
    size_t TimeToString(FILETIME utc_timestamp, std::string& out) {
        return TimeToString(filetime_to_timet(utc_timestamp), out);
    }
    time_t filetime_to_timet(const FILETIME& ft) {
        ULARGE_INTEGER ull;
        ull.LowPart = ft.dwLowDateTime;
        ull.HighPart = ft.dwHighDateTime;
        return ull.QuadPart / 10000000ULL - 11644473600ULL;
    }

    bool ArrayToIni(const std::wstring& in, std::string* out)
    {
        size_t len = in.size();
        if (!len) {
            out->clear();
            return true;
        }
        out->resize((len * 5) - 1, 0);
        char* data = out->data();
        size_t offset = 0;
        for (size_t i = 0; i < len; i++) {
            if (offset > 0)
                data[offset++] = ' ';
            offset += sprintf(&data[offset], "%04x", in[i]);
        }
        return true;
    }
    bool ArrayToIni(const uint32_t* in, size_t in_len, std::string* out)
    {
        if (!in_len) {
            out->clear();
            return true;
        }
        out->resize((in_len * 9) - 1, 0);
        char* data = out->data();
        size_t offset = 0;
        for (size_t i = 0; i < in_len; i++) {
            if (offset > 0)
                data[offset++] = ' ';
            offset += sprintf(&data[offset], "%08x", in[i]);
        }
        return true;
    }
    bool ParseUInt(const char* str, unsigned int* val, int base) {
        char* end;
        if (!str) return false;
        *val = strtoul(str, &end, base);
        if (str == end || errno == ERANGE)
            return false;
        else
            return true;
    }
    bool ParseUInt(const wchar_t* str, unsigned int* val, int base) {
        wchar_t* end;
        if (!str) return false;
        *val = wcstoul(str, &end, base);
        if (str == end || errno == ERANGE)
            return false;
        else
            return true;
    }
    std::wstring ToWstr(std::string& str) {
        // @Cleanup: No error handling whatsoever
        if (str.empty()) return std::wstring();
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
        std::wstring wstrTo(static_cast<size_t>(size_needed), 0);
        MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
        return wstrTo;
    }
    size_t wcstostr(char* dest, const wchar_t* src, size_t n) {
        size_t i;
        unsigned char* d = (unsigned char*)dest;
        for (i = 0; i < n; i++) {
            if (src[i] & ~0x7f)
                return 0;
            d[i] = src[i] & 0x7fu;
            if (src[i] == 0) break;
        }
        return i;
    }
    char* StrCopy(char* dest, const char* src, size_t dest_size) {
        strncpy(dest, src, dest_size - 1);
        dest[dest_size - 1] = 0;
        return dest;
    }
    void EncString::reset(const uint32_t _enc_string_id, bool sanitise) {
        if (_enc_string_id && encoded_ws.length()) {
            uint32_t this_id = GW::UI::EncStrToUInt32(encoded_ws.c_str());
            if (this_id == _enc_string_id)
                return;
        }
        reset(nullptr, sanitise);
        if (_enc_string_id) {
            wchar_t out[8] = { 0 };
            if (!GW::UI::UInt32ToEncStr(_enc_string_id, out, _countof(out)))
                return;
            encoded_ws = out;
        }
    }
    EncString* EncString::language(GW::Constants::TextLanguage l)
    {
        if (language_id == l)
            return this;
        decoded_ws.clear();
        decoded_s.clear();
        decoding = sanitised = false;
        language_id = l;
        return this;
    }

    void EncString::reset(const wchar_t* _enc_string, bool sanitise)
    {
        if (_enc_string && wcscmp(_enc_string, encoded_ws.c_str()) == 0)
            return;
        encoded_ws.clear();
        decoded_ws.clear();
        decoded_s.clear();
        decoding = false;
        sanitised = !sanitise;
        if (_enc_string)
            encoded_ws = _enc_string;
    }

    std::wstring& EncString::wstring()
    {
        if (!decoding && !encoded_ws.empty()) {
            GW::UI::AsyncDecodeStr(encoded_ws.c_str(), &decoded_ws, (uint32_t)language_id);
            decoding = true;
        }
        sanitise();
        return decoded_ws;
    }

    void EncString::sanitise() {
        if (!sanitised && !decoded_ws.empty()) {
            sanitised = true;
            static const std::wregex sanitiser(L"<[^>]+>");
            decoded_ws = std::regex_replace(decoded_ws, sanitiser, L"");
        }
    }

    std::string& EncString::string()
    {
        wstring();
        if (sanitised && !decoded_ws.empty() && decoded_s.empty()) {
            decoded_s = WStringToString(decoded_ws);
        }
        return decoded_s;
    }
}

















