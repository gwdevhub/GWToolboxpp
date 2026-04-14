#include "stdafx.h"

#include <GWCA/Managers/RenderMgr.h>
#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/Scanner.h>

#include "FontLoader.h"
#include <Modules/Resources.h>
#include <Utils/TextUtils.h>
#include "toolbox_default_font.h"

#include "fonts/fontawesome5.h"

namespace {
    ImFont* loaded_font = nullptr;

    bool fonts_loading = false;
    bool fonts_loaded = false;

    struct FontData {
        std::vector<ImWchar> glyph_ranges;
        std::wstring font_name;
    };

    const std::vector<ImWchar> fontawesome5_glyph_ranges = {ICON_MIN_FA, ICON_MAX_FA, 0};

    constexpr std::vector<ImWchar> ConstGetGlyphRangesLatin()
    {
        return {
            0x0020, 0x00FF, 0 // Basic Latin + Latin Supplement
        };
    }

    constexpr std::vector<ImWchar> ConstGetGlyphRangesJapanese()
    {
        return {
            0x0020, 0x00FF,   // Basic Latin + Latin Supplement
            0x3000, 0x30FF,   // CJK Symbols and Punctuations, Hiragana, Katakana
            0x31F0, 0x31FF,   // Katakana Phonetic Extensions
            0xFF00, 0xFFEF,   // Half-width characters
            0xFFFD, 0xFFFD, 0 // Invalid
        };
    }

    constexpr std::vector<ImWchar> ConstGetGlyphRangesCyrillic()
    {
        return {
            0x0020, 0x00FF,   // Basic Latin + Latin Supplement
            0x0400, 0x052F,   // Cyrillic + Cyrillic Supplement
            0x2DE0, 0x2DFF,   // Cyrillic Extended-A
            0xA640, 0xA69F, 0 // Cyrillic Extended-B
        };
    }

    constexpr std::vector<ImWchar> ConstGetGlyphRangesChinese()
    {
        return {
            0x0020, 0x00FF,   // Basic Latin + Latin Supplement
            0x2000, 0x206F,   // General Punctuation
            0x3000, 0x30FF,   // CJK Symbols and Punctuations, Hiragana, Katakana
            0x31F0, 0x31FF,   // Katakana Phonetic Extensions
            0xFF00, 0xFFEF,   // Half-width characters
            0xFFFD, 0xFFFD,   // Invalid
            0x4e00, 0x9FAF, 0 // CJK Ideograms
        };
    }

    constexpr std::vector<ImWchar> ConstGetGlyphRangesKorean()
    {
        return {
            0x0020, 0x00FF,   // Basic Latin + Latin Supplement
            0x3131, 0x3163,   // Korean alphabets
            0xAC00, 0xD7A3,   // Korean characters
            0xFFFD, 0xFFFD, 0 // Invalid
        };
    }

    constexpr std::vector<ImWchar> ConstGetGlyphRangesGW()
    {
        return {
            0x20, 0x7f,
            0xa1, 0xff,
            0x100, 0x180,
            0x0391, 0x0460,
            0x2010, 0x266b,
            0x3000, 0x3020,
            0x3041, 0x3100,
            0x3105, 0x312a,
            0x3131, 0x318f,
            0xac00, 0xd7a4,
            0x4e00, 0x9fa6,
            0xf900, 0xfa6b,
            0xff01, 0xffe7, 0
        };
    }

    constexpr std::vector<ImWchar> find_glyph_range_intersection(const std::vector<ImWchar>& range1, const std::vector<ImWchar>& range2)
    {
        if (range1.empty() || range2.empty()) return {};

        auto in_range = [](const wchar_t c, const std::vector<ImWchar>& range) {
            for (size_t i = 0; i < range.size() - 1; i += 2) {
                if (c >= range[i] && c <= range[i + 1]) return true;
            }
            return false;
        };

        wchar_t start = 0;
        bool in_intersection = false;

        std::vector<ImWchar> intersection;
        for (ImWchar c = 0; c <= 0xffe7; ++c) {
            const bool in_both = in_range(c, range1) && in_range(c, range2);

            if (in_both && !in_intersection) {
                start = c;
                in_intersection = true;
            }
            else if (!in_both && in_intersection) {
                intersection.push_back(start);
                intersection.push_back(c - 1);
                in_intersection = false;
            }
        }

        if (in_intersection) {
            intersection.push_back(start);
            intersection.push_back(0xffe7);
        }

        intersection.push_back(0); // Null-terminate the range
        return intersection;
    }

    std::vector<FontData>& GetFontData()
    {
        static std::vector<FontData> font_data;
        if (!font_data.empty())
            return font_data;

        const auto fonts_on_disk = std::to_array<std::pair<std::wstring_view, std::vector<ImWchar>>>({
            {L"Font.ttf", ConstGetGlyphRangesLatin()},
            {L"Font_Japanese.ttf", ConstGetGlyphRangesJapanese()},
            {L"Font_Cyrillic.ttf", ConstGetGlyphRangesCyrillic()},
            {L"Font_ChineseTraditional.ttf", ConstGetGlyphRangesChinese()},
            {L"Font_Korean.ttf", ConstGetGlyphRangesKorean()}
        });

        for (const auto& [font_name, glyph_range] : fonts_on_disk) {
            const auto path = Resources::GetPath(font_name);
            if (!std::filesystem::exists(path))
                continue;
            const auto font_glyphs = find_glyph_range_intersection(glyph_range, ConstGetGlyphRangesGW());
            font_data.emplace_back(font_glyphs, path);
        }

        return font_data;
    }

    // Build a single font by merging all available font files.
    ImFont* BuildFont(const float size, const bool default_only = false)
    {
        ImFontAtlas* atlas = ImGui::GetIO().Fonts;

        ImFontConfig cfg;
        cfg.PixelSnapH = true;
        cfg.OversampleH = 1;
        cfg.OversampleV = 1;
        cfg.MergeMode = false;

        if (default_only) {
            auto* font = atlas->AddFontFromMemoryCompressedTTF(toolbox_default_font_compressed_data, toolbox_default_font_compressed_size, size, &cfg, toolbox_default_font_glyph_ranges);
            cfg.MergeMode = true;
            atlas->AddFontFromMemoryCompressedTTF(fontawesome5_compressed_data, fontawesome5_compressed_size, size, &cfg, fontawesome5_glyph_ranges.data());
            cfg.MergeMode = false;
            return font;
        }

        ImFont* font = nullptr;
        // Load fonts from disk, merging glyph ranges
        for (const auto& [glyph_ranges, font_name] : GetFontData()) {
            size_t data_size;

            ASSERT(!font_name.empty() && "Font name is empty, this shouldn't happen. Contact the developers.");
            const auto font_name_str = TextUtils::WStringToString(font_name);
            void* data = ImFileLoadToMemory(font_name_str.c_str(), "rb", &data_size, 0);

            if (!data)
                continue; // Failed to load data from disk
            font = atlas->AddFontFromMemoryTTF(data, data_size, size, &cfg, glyph_ranges.data());
            cfg.MergeMode = true;
        }

        // Merge fontawesome icons
        cfg.MergeMode = true;
        atlas->AddFontFromMemoryCompressedTTF(fontawesome5_compressed_data, fontawesome5_compressed_size, size, &cfg, fontawesome5_glyph_ranges.data());

        return font;
    }

}

namespace FontLoader {
    // Has LoadFonts() finished?
    bool FontsLoaded()
    {
        return fonts_loaded;
    }

    // Loads fonts asynchronously. CJK font files can be over 20mb in size!
    void LoadFonts([[maybe_unused]] const bool force)
    {
        if (fonts_loading) {
            return;
        }
        if (fonts_loaded) {
            return;
        }
        if (!GImGui)
            return;
        fonts_loading = true;
        fonts_loaded = false;

        constexpr float base_size = static_cast<float>(FontSize::text);

        Resources::EnqueueDxTask([base_size](IDirect3DDevice9*) {
            loaded_font = BuildFont(base_size, true);
            fonts_loaded = true;
            fonts_loading = false;
            printf("Loaded default font\n");
        });

        Resources::EnqueueDxTask([base_size](IDirect3DDevice9*) {
            auto* font = BuildFont(base_size);
            if (font) {
                loaded_font = font;
            }
            printf("Loaded all fonts\n");
        });
    }

    void Terminate()
    {
        loaded_font = nullptr;
    }

    ImFont* GetFont()
    {
        if (loaded_font && loaded_font->IsLoaded()) {
            return loaded_font;
        }

        const auto& io = ImGui::GetIO();
        return io.Fonts->Fonts[0];
    }
}
