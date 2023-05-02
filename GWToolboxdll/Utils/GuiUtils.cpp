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

    const wchar_t* diacritics[] =
    {
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
        L"a\u0061\u24D0\uFF41\u1E9A\u00E0\u00E1\u00E2\u1EA7\u1EA5\u1EAB\u1EA9\u00E3\u0101\u0103\u1EB1\u1EAF\u1EB5\u1EB3\u0227\u01E1\u00E4\u01DF\u1EA3\u00E5\u01FB\u01CE\u0201\u0203\u1EA1\u1EAD\u1EB7\u1E01\u0105\u2C65\u0250",
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
        L"m\u006D\u24DC\uFF4D\u1E3F\u1E41\u1E43\u0271\u026F",
        L"n\u006E\u24DD\uFF4E\u01F9\u0144\u00F1\u1E45\u0148\u1E47\u0146\u1E4B\u1E49\u019E\u0272\u0149\uA791\uA7A5",
        L"o\u006F\u24DE\uFF4F\u00F2\u00F3\u00F4\u1ED3\u1ED1\u1ED7\u1ED5\u00F5\u1E4D\u022D\u1E4F\u014D\u1E51\u1E53\u014F\u022F\u0231\u00F6\u022B\u1ECF\u0151\u01D2\u020D\u020F\u01A1\u1EDD\u1EDB\u1EE1\u1EDF\u1EE3\u1ECD\u1ED9\u01EB\u01ED\u00F8\u01FF\u0254\uA74B\uA74D\u0275",
        L"p\u0070\u24DF\uFF50\u1E55\u1E57\u01A5\u1D7D\uA751\uA753\uA755",
        L"q\u0071\u24E0\uFF51\u024B\uA757\uA759",
        L"r\u0072\u24E1\uFF52\u0155\u1E59\u0159\u0211\u0213\u1E5B\u1E5D\u0157\u1E5F\u024D\u027D\uA75B\uA7A7\uA783",
        L"s\u0073\u24E2\uFF53\u015B\u1E65\u015D\u1E61\u0161\u1E67\u1E63\u1E69\u0219\u015F\u023F\uA7A9\uA785\u1E9B",
        L"t\u03C4\u0074\u24E3\uFF54\u1E6B\u1E97\u0165\u1E6D\u021B\u0163\u1E71\u1E6F\u0167\u01AD\u0288\u2C66\uA787",
        L"u\u0075\u24E4\uFF55\u00F9\u00FA\u00FB\u0169\u1E79\u016B\u1E7B\u016D\u00FC\u01DC\u01D8\u01D6\u01DA\u1EE7\u016F\u0171\u01D4\u0215\u0217\u01B0\u1EEB\u1EE9\u1EEF\u1EED\u1EF1\u1EE5\u1E73\u0173\u1E77\u1E75\u0289",
        L"v\u0076\u24E5\uFF56\u1E7D\u1E7F\u028B\uA75F\u028C\u03BD",
        L"w\u0077\u24E6\uFF57\u1E81\u1E83\u0175\u1E87\u1E85\u1E98\u1E89\u2C73",
        L"x\u0078\u24E7\uFF58\u1E8B\u1E8D",
        L"y\u0079\u24E8\uFF59\u1EF3\u00FD\u0177\u1EF9\u0233\u1E8F\u00FF\u1EF7\u1E99\u1EF5\u01B4\u024F\u1EFF",
        L"z\u007A\u24E9\uFF5A\u017A\u1E91\u017C\u017E\u1E93\u1E95\u01B6\u0225\u0240\u2C6C\uA763"
    };

    std::map<wchar_t, wchar_t> diacritics_charmap;

}
namespace GuiUtils {
    void FlashWindow(bool force) {
        FLASHWINFO flashInfo = { 0 };
        flashInfo.cbSize = sizeof(FLASHWINFO);
        flashInfo.hwnd = GW::MemoryMgr::GetGWWindowHandle();
        if (!force && GetActiveWindow() == flashInfo.hwnd)
            return; // Already in focus
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
        if (url_path.empty())
            return GetWikiPrefix();
        char cmd[256];
        std::string encoded = UrlEncode(WStringToString(RemoveDiacritics(url_path)),'_');
        snprintf(cmd, _countof(cmd), "%s%s", GetWikiPrefix(), encoded.c_str());
        return cmd;
    }
    void SearchWiki(std::wstring term) {
        if (term.empty())
            return;
        char cmd[256];
        std::string encoded = UrlEncode(WStringToString(RemoveDiacritics(term)));
        ASSERT(snprintf(cmd, _countof(cmd), "%s?search=%s", GetWikiPrefix(), encoded.c_str()) != -1);
        GW::UI::SendUIMessage(GW::UI::UIMessage::kOpenWikiUrl, cmd);
    }
    void OpenWiki(const std::wstring& url_path) {
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
    float GetGWScaleMultiplier(bool force) {
        return Resources::GetGWScaleMultiplier(force);
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
        char out[256]{};
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
        char out[256]{};
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
        return std::move(strTo);
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
    std::wstring SanitiseFilename(const std::wstring& str) {
        const wchar_t* invalid_chars = L"<>:\"/\\|?*";
        size_t len = 0;
        std::wstring out;
        out.resize(str.length());
        for (size_t i = 0; i < str.length(); i++) {
            if (wcschr(invalid_chars, str[i]))
                continue;
            out[len] = str[i];
            len++;
        }
        out.resize(len);
        return out;
    }

    std::wstring RemoveDiacritics(const std::wstring& s) {
        if (diacritics_charmap.empty()) {
            // Build static diacritics map if not already done so
            for (size_t i = 0; i < _countof(diacritics); i++) {
                for (size_t j = 1; diacritics[i][j]; j++) {
                    diacritics_charmap[diacritics[i][j]] = diacritics[i][0];
                }
            }
        }
        std::wstring out(s.length(), L'\0');
        std::ranges::transform(s, out.begin(), [&](wchar_t wc) ->wchar_t {
            if (wc < 0x7f)
                return wc;
            const auto it = diacritics_charmap.find(wc);
            return it == diacritics_charmap.end() ? wc : it->second; });
        return out;
    }
    // Convert an UTF8 string to a wide Unicode String
    std::wstring StringToWString(const std::string& str)
    {
        // @Cleanup: ASSERT used incorrectly here; value passed could be from anywhere!
        if (str.empty()) return {};
        // NB: GW uses code page 0 (CP_ACP)
        const int size_needed = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str.data(), (int)str.size(), NULL, 0);
        ASSERT(size_needed != 0);
        std::wstring wstrTo(size_needed, 0);
        ASSERT(MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), wstrTo.data(), size_needed));
        return std::move(wstrTo);
    }
    std::wstring SanitizePlayerName(std::wstring s) {
        // e.g. "Player Name (2)" => "Player Name", for pvp player names
        // e.g. "Player Name [TAG]" = >"Player Name", for alliance message sender name
        wchar_t out[64]{};
        size_t len = 0;
        wchar_t remove_char_token = 0;
        for (const auto& wchar : s) {
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
        return {out};
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
    size_t IniToArray(const char* in, std::vector<std::string>& out, const char separator) {
        const char* found = in;
        out.clear();
        while(true) {
            if (!(found && *found))
                break;
            auto next_token = (char*)strchr(found, separator);
            if (next_token)
                *next_token = 0;
            out.push_back(found);
            found = 0;
            if (next_token) {
                found = next_token + 1;
            }
        }
        return out.size();
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
        tm* timeinfo = localtime(&utc_timestamp);
        if (!timeinfo)
            return 0;
        time_t now = time(NULL);
        tm* nowinfo = localtime(&now);
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
        const ULARGE_INTEGER ull{ft.dwLowDateTime, ft.dwHighDateTime};
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
        if (str.empty()) return {};
        const int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), NULL, 0);
        std::wstring wstrTo(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), wstrTo.data(), size_needed);
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
        language_id = l;
        reset(encoded_ws.c_str());
        return this;
    }

    void EncString::reset(const wchar_t* _enc_string, bool sanitise)
    {
        if (_enc_string && wcscmp(_enc_string, encoded_ws.c_str()) == 0)
            return;
        encoded_ws.clear();
        decoded_ws.clear();
        decoded_s.clear();
        decoding = decoded = false;
        sanitised = !sanitise;
        if (_enc_string)
            encoded_ws = _enc_string;
    }

    std::wstring& EncString::wstring()
    {
        if (!decoded && !decoding && !encoded_ws.empty()) {
            decoding = true;
            GW::UI::AsyncDecodeStr(encoded_ws.c_str(), OnStringDecoded, (void*)this, (uint32_t)language_id);
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
    void EncString::OnStringDecoded(void* param, wchar_t* decoded) {
        EncString* context = (EncString*)param;
        if (!(context && context->decoding && !context->decoded))
            return; // Not expecting a decoded string; may have been reset() before response was received.
        if(decoded && decoded[0])
            context->decoded_ws = decoded;
        context->decoded = true;
        context->decoding = false;
    }

    std::string format(const char* msg, ...) {
        std::string out;
        va_list args;
        va_start(args, msg);
        const auto size = vsnprintf(NULL, 0, msg,args);
        out.resize(size + 1);
        ASSERT(vsnprintf(out.data(), out.size(), msg, args) <= size);
        va_end(args);
        return std::move(out);
    }
    std::wstring format(const wchar_t* msg, ...) {
        std::wstring out;
        va_list args;
        va_start(args, msg);
        const auto size = _vsnwprintf(NULL, 0, msg, args);
        out.resize(size + 1);
        ASSERT(_vsnwprintf(out.data(), out.size(), msg, args) <= size);
        va_end(args);
        return std::move(out);
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
