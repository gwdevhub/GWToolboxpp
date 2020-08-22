#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/Utilities/Scanner.h>

#include <GWCA/Managers/UIMgr.h>

#include <GuiUtils.h>
#include <Utf8.h>

#include <Modules/Resources.h>

namespace {
    ImFont* font16 = nullptr;
    ImFont* font18 = nullptr;
    ImFont* font20 = nullptr;
    ImFont* font24 = nullptr;
    ImFont* font42 = nullptr;
    ImFont* font48 = nullptr;

    bool fonts_loading = false;
    bool fonts_loaded = false;
}
// Has GuiUtils::LoadFonts() finished?
bool GuiUtils::FontsLoaded() {
    return fonts_loaded;
}
// Loads fonts asynchronously. CJK font files can by over 20mb in size!
void GuiUtils::LoadFonts() {
    if (fonts_loaded || fonts_loading)
        return;
    fonts_loading = true;

    std::thread t([] {
        printf("Loading fonts\n");

        ImGuiIO& io = ImGui::GetIO();

        struct FontData
        {
            wchar_t filename[128] = L"";
            const ImWchar* glyph_ranges = nullptr;
            void* data = nullptr;
            size_t data_size = 0;
        };
        std::vector<FontData> fonts;

        fonts.push_back({L"Font.ttf", io.Fonts->GetGlyphRangesDefault()});
        static const ImWchar fa_ranges[] = {ICON_MIN_FA, ICON_MAX_FA, 0};
        fonts.push_back({L"fa-solid-900.ttf", fa_ranges});
        fonts.push_back({L"Font_Japanese.ttf", io.Fonts->GetGlyphRangesJapanese()});
        fonts.push_back({L"Font_Cyrillic.ttf", io.Fonts->GetGlyphRangesCyrillic()});
        fonts.push_back({L"Font_ChineseTraditional.ttf", io.Fonts->GetGlyphRangesChineseFull()});
        fonts.push_back({L"Font_Korean.ttf", io.Fonts->GetGlyphRangesKorean()});


        // Load font data from disk
        for (auto& font : fonts) {
            utf8::string utf8path = Resources::GetPathUtf8(font.filename);
            font.data = ImFileLoadToMemory(utf8path.bytes, "rb", &font.data_size, 0);
        }

        if (fonts.front().data == nullptr) {
            printf("Failed to find Font.ttf file\n");
            return; 
        }
        
        std::vector<float> font_sizes = {16.f, 18.f, 20.f, 24.f, 42.f, 48.f};
        for (unsigned int i = 0; i < font_sizes.size(); ++i) {
            ImFontConfig cfg;
            sprintf(cfg.Name, "Font%f", font_sizes[i]);
            cfg.MergeMode = false; // for the first
            cfg.SizePixels = font_sizes[i];
            cfg.PixelSnapH = true;
            cfg.FontDataOwnedByAtlas = (i == 0); // only the first font can free the data at the end
            for (const auto& font : fonts) {
                if (font.data) {
                    io.Fonts->AddFontFromMemoryTTF(font.data, font.data_size, font_sizes[i], &cfg, font.glyph_ranges);
                }
                cfg.MergeMode = true;
            }
        }
        font16 = io.Fonts->Fonts[0];
        font18 = io.Fonts->Fonts[1];
        font20 = io.Fonts->Fonts[2];
        font24 = io.Fonts->Fonts[3];
        font42 = io.Fonts->Fonts[4];
        font48 = io.Fonts->Fonts[5];

        printf("Fonts loaded\n");
        fonts_loaded = true;
        fonts_loading = false;
    });
    t.detach();
}
ImFont* GuiUtils::GetFont(GuiUtils::FontSize size) {
    ImFont* font = [](FontSize size) -> ImFont* {
        switch (size) {
            case GuiUtils::FontSize::f16: return font16;
            case GuiUtils::FontSize::f18: return font18;
            case GuiUtils::FontSize::f20: return font20;
            case GuiUtils::FontSize::f24: return font24;
            case GuiUtils::FontSize::f42: return font42;
            case GuiUtils::FontSize::f48: return font48;
        default: return nullptr;
        }
    }(size);
    if (font && font->IsLoaded()) {
        return font;
    } else {
        return ImGui::GetIO().Fonts->Fonts[0];
    }
}

size_t GuiUtils::GetPartyHealthbarHeight() {
    GW::Constants::InterfaceSize interfacesize =
        static_cast<GW::Constants::InterfaceSize>(GW::UI::GetPreference(GW::UI::Preference_InterfaceSize));

    switch (interfacesize) {
    case GW::Constants::InterfaceSize::SMALL: return GW::Constants::HealthbarHeight::Small;
    case GW::Constants::InterfaceSize::NORMAL: return GW::Constants::HealthbarHeight::Normal;
    case GW::Constants::InterfaceSize::LARGE: return GW::Constants::HealthbarHeight::Large;
    case GW::Constants::InterfaceSize::LARGER: return GW::Constants::HealthbarHeight::Larger;
    default:
        return GW::Constants::HealthbarHeight::Normal;
    }
}

std::string GuiUtils::ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](char c) -> char {
        return static_cast<char>(::tolower(c));
    });
    return s;
}

std::wstring GuiUtils::ToLower(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) -> wchar_t {
        return static_cast<wchar_t>(::tolower(c));
    });
    return s;
}
// Convert a wide Unicode string to an UTF8 string
std::string GuiUtils::WStringToString(const std::wstring& wstr)
{
    // @Cleanup: No error handling whatsoever
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(static_cast<size_t>(size_needed), 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

// Convert an UTF8 string to a wide Unicode String
std::wstring GuiUtils::StringToWString(const std::string& str)
{
    // @Cleanup: No error handling whatsoever
    if (str.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(static_cast<size_t>(size_needed), 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}
std::wstring GuiUtils::SanitizePlayerName(std::wstring s) {
    // @Cleanup:
    // What exactly needs to be done to sanitize a player name?
    // Add this information as a comment, because nobody want to read the regex.
    if (s.empty()) return L"";
    static std::wregex remove(L" ?[\\(\\[]\\w+[\\)\\]]");
    s = std::regex_replace(s, remove, L""); 
    return s;
}
std::string GuiUtils::RemovePunctuation(std::string s) {
    s.erase(std::remove_if(s.begin(), s.end(), &ispunct), s.end());
    return s;
}
std::wstring GuiUtils::RemovePunctuation(std::wstring s) {
    s.erase(std::remove_if(s.begin(), s.end(), &ispunct), s.end());
    return s;
}
bool GuiUtils::ParseInt(const char *str, int *val, int base) {
    char *end;
    *val = strtol(str, &end, base);
    if (*end != 0 || errno == ERANGE)
        return false;
    else
        return true;
}
bool GuiUtils::ParseInt(const wchar_t *str, int *val, int base) {
    wchar_t *end;
    *val = wcstol(str, &end, base);
    if (*end != 0 || errno == ERANGE)
        return false;
    else
        return true;
}
bool GuiUtils::ParseFloat(const char *str, float *val) {
    char *end;
    *val = strtof(str, &end);
    return str != end && errno != ERANGE;
}
bool GuiUtils::ParseFloat(const wchar_t *str, float *val) {
    wchar_t *end;
    *val = wcstof(str, &end);
    return str != end && errno != ERANGE;
}
bool GuiUtils::ParseUInt(const char *str, unsigned int *val, int base) {
    char *end;
    *val = strtoul(str, &end, base);
    if (str == end || errno == ERANGE)
        return false;
    else
        return true;
}
bool GuiUtils::ParseUInt(const wchar_t *str, unsigned int *val, int base) {
    wchar_t *end;
    *val = wcstoul(str, &end, base);
    if (str == end || errno == ERANGE)
        return false;
    else
        return true;
}
std::wstring GuiUtils::ToWstr(std::string &str) {
    // @Cleanup: No error handling whatsoever
    if (str.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(static_cast<size_t>(size_needed), 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}
size_t GuiUtils::wcstostr(char *dest, const wchar_t *src, size_t n) {
    size_t i;
    unsigned char *d = (unsigned char *)dest;
    for (i = 0; i < n; i++) {
        if (src[i] & ~0x7f)
            return 0;
        d[i] = src[i] & 0x7fu;
        if (src[i] == 0) break;
    }
    return i;
}
char *GuiUtils::StrCopy(char *dest, const char *src, size_t dest_size) {
    strncpy(dest, src, dest_size - 1);
    dest[dest_size - 1] = 0;
    return dest;
}
