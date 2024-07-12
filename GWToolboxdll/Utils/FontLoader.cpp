#include "stdafx.h"

#include <GWCA/Managers/RenderMgr.h>
#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/Scanner.h>

#include "FontLoader.h"
#include <Modules/Resources.h>

#include "fonts/fontawesome5.h"

namespace {
    ImFont* font_widget_large = nullptr;
    ImFont* font_widget_small = nullptr;
    ImFont* font_widget_label = nullptr;
    ImFont* font_header1 = nullptr;
    ImFont* font_header2 = nullptr;
    ImFont* font_text = nullptr;

    std::vector<ImFont*> GetFonts()
    {
        return {
            font_text,
            font_widget_large,
            font_widget_small,
            font_widget_label,
            font_header1,
            font_header2
        };
    }

    bool fonts_loading = false;
    bool fonts_loaded = false;

    struct FontData {
        std::vector<ImWchar> glyph_ranges;
        std::wstring font_name;
        size_t data_size = 0;
        void* data = nullptr;
        bool compressed = false;
    };

    std::vector<FontData> font_data;

    const std::vector<ImWchar> fontawesome5_glyph_ranges = {ICON_MIN_FA, ICON_MAX_FA, 0};

    [[maybe_unused]] ImFontGlyphRangesBuilder GetGWGlyphRange()
    {
        ImFontGlyphRangesBuilder builder;

        using GetGlyphRanges_pt = uint32_t*(*)();
        GetGlyphRanges_pt GetGlyphRanges_Func = 0;
        uintptr_t address = GW::Scanner::Find("\x50\x8d\x45\xc8\x50\x6a\x0d", "xxxxxxx", -0x5);
        GetGlyphRanges_Func = (GetGlyphRanges_pt)GW::Scanner::FunctionFromNearCall(address);

        uint32_t gw_glyph_range_count = 0xd; // Glyph ranges are in pairs

        if (GetGlyphRanges_Func) {
            const auto res = GetGlyphRanges_Func();
            std::vector<ImWchar> ranges;
            for (size_t i = 0, size = (gw_glyph_range_count * 2); i < size; i++) {
                ranges.push_back(static_cast<ImWchar>(res[i]));
            }
            if (ranges.empty() || ranges.back()) {
                ranges.push_back(0);
            }
            if (ranges[0] == 33) {
                ranges[0] = 32;
            }
            builder.AddRanges(ranges.data());
        }
        return builder;
    }

    bool CreateFontTexture(ImFontAtlas* atlas)
    {
        // Upload texture to graphics system

        auto device = GW::Render::GetDevice();

        unsigned char* pixels;
        int width, height, bytes_per_pixel;
        atlas->GetTexDataAsRGBA32(&pixels, &width, &height, &bytes_per_pixel);

        LPDIRECT3DTEXTURE9 new_texture = 0;
        auto err = D3DERR_INVALIDDEVICE;
        for (size_t i = 0; i < 3 && err != D3D_OK; i++) {
            err = device->CreateTexture(width, height, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &new_texture, nullptr);
        }
        if (err != D3D_OK)
            return false;
        D3DLOCKED_RECT tex_locked_rect = { 0 };
        err = D3DERR_INVALIDDEVICE;
        for (size_t i = 0; i < 3 && err != D3D_OK; i++) {
            err = new_texture->LockRect(0, &tex_locked_rect, nullptr, 0);
        }
        if (err != D3D_OK) {
            new_texture->Release();
            return false;
        }

        for (int y = 0; y < height; y++)
            memcpy((unsigned char*)tex_locked_rect.pBits + (size_t)tex_locked_rect.Pitch * y, pixels + (size_t)width * bytes_per_pixel * y, (size_t)width * bytes_per_pixel);
        new_texture->UnlockRect(0);

        LPDIRECT3DTEXTURE9 old_texture = (LPDIRECT3DTEXTURE9)atlas->TexID;
        if (old_texture) {
            old_texture->Release();
        }
        atlas->TexID = new_texture;
        return true;
    }


    using ImGui_ImplDX9_InvalidateDeviceObjects_pt = void(*)();
    ImGui_ImplDX9_InvalidateDeviceObjects_pt ImGui_ImplDX9_InvalidateDeviceObjects_Ret = nullptr, ImGui_ImplDX9_InvalidateDeviceObjects_Func = nullptr;

    using ImGui_ImplDX9_CreateFontsTexture_pt = bool(*)();
    ImGui_ImplDX9_CreateFontsTexture_pt ImGui_ImplDX9_CreateFontsTexture_Ret = nullptr, ImGui_ImplDX9_CreateFontsTexture_Func = nullptr;

    // Also create any missing fonts from our own array
    bool OnImGui_ImplDX9_CreateFontsTexture()
    {
        GW::Hook::EnterHook();
        bool ret = FontLoader::CreateFontTextures() && ImGui_ImplDX9_CreateFontsTexture_Ret();
        GW::Hook::LeaveHook();
        return ret;
    }

    // Also release any fonts from our own array
    void OnImGui_ImplDX9_InvalidateDeviceObjects()
    {
        GW::Hook::EnterHook();
        FontLoader::ReleaseFontTextures();
        ImGui_ImplDX9_InvalidateDeviceObjects_Ret();
        GW::Hook::LeaveHook();
    }

    void Hook_ImGui_ImplDX9_Functions()
    {
        if (!ImGui_ImplDX9_InvalidateDeviceObjects_Ret) {
            ImGui_ImplDX9_InvalidateDeviceObjects_Func = ImGui_ImplDX9_InvalidateDeviceObjects;
            GW::Hook::CreateHook((void**)&ImGui_ImplDX9_InvalidateDeviceObjects_Func, OnImGui_ImplDX9_InvalidateDeviceObjects, (void**)&ImGui_ImplDX9_InvalidateDeviceObjects_Ret);
            GW::Hook::EnableHooks(ImGui_ImplDX9_InvalidateDeviceObjects_Func);
        }
        if (!ImGui_ImplDX9_CreateFontsTexture_Ret) {
            ImGui_ImplDX9_CreateFontsTexture_Func = ImGui_ImplDX9_CreateFontsTexture;
            GW::Hook::CreateHook((void**)&ImGui_ImplDX9_CreateFontsTexture_Func, OnImGui_ImplDX9_CreateFontsTexture, (void**)&ImGui_ImplDX9_CreateFontsTexture_Ret);
            GW::Hook::EnableHooks(ImGui_ImplDX9_CreateFontsTexture_Func);
        }
    }

    constexpr std::vector<ImWchar> ConstGetGlyphRangesLatin()
    {
        return {
            0x0020, 0x00FF, // Basic Latin + Latin Supplement
        };
    }

    constexpr std::vector<ImWchar> ConstGetGlyphRangesJapanese()
    {
        return  {
            0x0020, 0x00FF, // Basic Latin + Latin Supplement
            0x3000, 0x30FF, // CJK Symbols and Punctuations, Hiragana, Katakana
            0x31F0, 0x31FF, // Katakana Phonetic Extensions
            0xFF00, 0xFFEF, // Half-width characters
            0xFFFD, 0xFFFD  // Invalid
        };
    }

    constexpr std::vector<ImWchar> ConstGetGlyphRangesCyrillic()
    {
        return {
            0x0020, 0x00FF, // Basic Latin + Latin Supplement
            0x0400, 0x052F, // Cyrillic + Cyrillic Supplement
            0x2DE0, 0x2DFF, // Cyrillic Extended-A
            0xA640, 0xA69F, // Cyrillic Extended-B
        };
    }

    constexpr std::vector<ImWchar> ConstGetGlyphRangesChinese()
    {
        return {
            0x0020, 0x00FF, // Basic Latin + Latin Supplement
            0x2000, 0x206F, // General Punctuation
            0x3000, 0x30FF, // CJK Symbols and Punctuations, Hiragana, Katakana
            0x31F0, 0x31FF, // Katakana Phonetic Extensions
            0xFF00, 0xFFEF, // Half-width characters
            0xFFFD, 0xFFFD, // Invalid
            0x4e00, 0x9FAF, // CJK Ideograms
        };
    }

    constexpr std::vector<ImWchar> ConstGetGlyphRangesKorean()
    {
        return {
            0x0020, 0x00FF, // Basic Latin + Latin Supplement
            0x3131, 0x3163, // Korean alphabets
            0xAC00, 0xD7A3, // Korean characters
            0xFFFD, 0xFFFD, // Invalid
        };
    }

    constexpr std::vector<ImWchar> ConstGetGWGlyphRange()
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
            0xff01, 0xffe7
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

    void ReleaseFontTexture(ImFont* font) {
        if (!font || !font->ContainerAtlas)
            return;
        LPDIRECT3DTEXTURE9 texture = (LPDIRECT3DTEXTURE9)font->ContainerAtlas->TexID;
        if (texture) {
            texture->Release();
            font->ContainerAtlas->SetTexID(0);
        }
    }

    void ReleaseFont(ImFont* font) {
        if (!font) return;
        if (font->ContainerAtlas && font->ContainerAtlas == ImGui::GetIO().Fonts)
            return;
        ReleaseFontTexture(font);
        if (font->ContainerAtlas) {
            IM_DELETE(font->ContainerAtlas);
        }
        else {
            IM_DELETE(font);
        }
    }

    std::vector<FontData>& GetFontData() {
        if (!font_data.empty())
            return font_data;
            const auto fonts_on_disk = std::to_array<std::pair<std::wstring_view, std::vector<ImWchar>>>({
            {L"Font.ttf", find_glyph_range_intersection(ConstGetGlyphRangesLatin(), ConstGetGWGlyphRange())},
            {L"Font_Japanese.ttf", find_glyph_range_intersection(ConstGetGlyphRangesJapanese(), ConstGetGWGlyphRange())},
            {L"Font_Cyrillic.ttf", find_glyph_range_intersection(ConstGetGlyphRangesCyrillic(), ConstGetGWGlyphRange())},
            {L"Font_ChineseTraditional.ttf", find_glyph_range_intersection(ConstGetGlyphRangesChinese(), ConstGetGWGlyphRange())},
            {L"Font_Korean.ttf", find_glyph_range_intersection(ConstGetGlyphRangesKorean(), ConstGetGWGlyphRange())}
            });

        for (const auto& [font_name, glyph_range] : fonts_on_disk) {
            const auto path = Resources::GetPath(font_name);
            if (!std::filesystem::exists(path))
                continue;
            font_data.emplace_back(glyph_range, path);
        }

        font_data.emplace_back(fontawesome5_glyph_ranges, L"", fontawesome5_compressed_size, (void*)fontawesome5_compressed_data, true);
        return font_data;
    }


    ImFont* BuildFont(const float size, bool first_only = false) {
        const auto atlas = IM_NEW(ImFontAtlas);

        ImFontAtlasFlags flags = 0;
        flags |= ImFontAtlasFlags_NoPowerOfTwoHeight;
        flags |= ImFontAtlasFlags_NoMouseCursors;
        flags |= ImFontAtlasFlags_NoBakedLines;

        atlas->Flags = flags;

        static ImFontConfig cfg;
        cfg.PixelSnapH = true;
        cfg.OversampleH = 1; // OversampleH = 2 for base text size (harder to read if OversampleH < 2)
        cfg.OversampleV = 1;

        cfg.MergeMode = false;
        for (const auto& font : GetFontData()) {
            void* data = font.data;
            if (size > 20.f && fontawesome5_compressed_data == data)
                continue; // Don't load FA over 20px
            size_t data_size = font.data_size;

            if (!font.font_name.empty()) {
                const auto path = Resources::GetPath(font.font_name);
                data = ImFileLoadToMemory(path.string().c_str(), "rb", &data_size, 0);
            }

            if (font.compressed) {
                atlas->AddFontFromMemoryCompressedTTF(data, data_size, size, &cfg, font.glyph_ranges.data());
            }
            else {
                atlas->AddFontFromMemoryTTF(data, data_size, size, &cfg, font.glyph_ranges.data());
            }
            if (first_only)
                break;
            cfg.MergeMode = true; // for all but the first
        }
        atlas->Build();
        unsigned char* unused = nullptr;
        // Preload the data for this font
        atlas->GetTexDataAsRGBA32(&unused, nullptr,nullptr,nullptr);
        return atlas->Fonts.back();
        };

    // Do dont loading into memory; run on a separate thread.
    void LoadFontsThread()
    {
        Hook_ImGui_ImplDX9_Functions();

        fonts_loading = true;
        fonts_loaded = false;

        // Language decides glyph range
        //const auto current_language = (GW::Constants::Language)GW::UI::GetPreference(GW::UI::NumberPreference::TextLanguage);

        while (!GImGui) {
            Sleep(16);
        }

        printf("Loading fonts\n");

        struct FontPending {
            ImFont** dst_font;
            FontLoader::FontSize font_size;
            ImFont* src_font = nullptr;
            FontPending(ImFont** dst_font, FontLoader::FontSize font_size) : dst_font(dst_font), font_size(font_size) {};
            void build(bool first_font_only = false) {
                src_font = BuildFont(static_cast<float>(font_size), first_font_only);
            }
        };

        auto first_pass = new std::vector<FontPending>({
            {&font_header2, FontLoader::FontSize::header2},
            {&font_header1, FontLoader::FontSize::header1},
            {&font_widget_label, FontLoader::FontSize::widget_label},
            { &font_widget_small, FontLoader::FontSize::widget_small },
            { &font_widget_large, FontLoader::FontSize::widget_large },
            { &font_text, FontLoader::FontSize::text }
        });

        // First pass; only build the first font.
        for (auto& pending : *first_pass) {
            pending.build(true);
        }
        Resources::EnqueueDxTask([first_pass](IDirect3DDevice9*) {
            for (auto& pending : *first_pass) {
                ReleaseFont(*pending.dst_font);
                *pending.dst_font = pending.src_font;
            }
            delete first_pass;
            ImGui::GetIO().Fonts = font_text->ContainerAtlas;
            ImGui_ImplDX9_InvalidateDeviceObjects();

            printf("Fonts loaded\n");
            fonts_loaded = true;
            fonts_loading = false;
            });

        // First pass; build full glyph ranges
        auto second_pass = new std::vector<FontPending>(*first_pass);
        for (auto& pending : *second_pass) {
            pending.build();
        }

        Resources::EnqueueDxTask([second_pass](IDirect3DDevice9*) {
            for (auto& pending : *second_pass) {
                ReleaseFont(*pending.dst_font);
                *pending.dst_font = pending.src_font;
            }
            delete second_pass;
            ImGui::GetIO().Fonts = font_text->ContainerAtlas;
            ImGui_ImplDX9_InvalidateDeviceObjects();
            });
    }
}

namespace FontLoader {

    bool ReleaseFontTextures()
    {
        if (!GImGui)
            return false;

        for (auto font : GetFonts()) {
            if (font && font->ContainerAtlas == ImGui::GetIO().Fonts)
                continue;
            ReleaseFontTexture(font);
        }
        return true;
    }

    bool CreateFontTextures()
    {
        if (!GImGui)
            return false;
        if (fonts_loading)
            return false;

        for (auto font : GetFonts()) {
            if (!(font && font->ContainerAtlas))
                continue;
            if (font->ContainerAtlas == ImGui::GetIO().Fonts)
                continue; // Fallback function will handle this one
            if (font->ContainerAtlas->TexID)
                continue; // Already generated
            if (!CreateFontTexture(font->ContainerAtlas))
                return false;
        }
        return true;
    }

    // Has LoadFonts() finished?
    bool FontsLoaded()
    {
        return fonts_loaded;
    }

    // Loads fonts asynchronously. CJK font files can by over 20mb in size!
    void LoadFonts([[maybe_unused]] const bool force) // todo: reload fonts when this is true
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

        Resources::EnqueueWorkerTask(LoadFontsThread);
    }

    void Terminate() {

        ImFont** fonts[] = {
            &font_widget_large,
            &font_widget_small,
            &font_widget_label,
            &font_header1,
            &font_header2,
            &font_text
        };


        for (auto font : fonts) {
            if (*font && (*font)->ContainerAtlas == ImGui::GetIO().Fonts)
                continue;
            ReleaseFont(*font);
            *font = nullptr;
        }
    }

    ImFont* GetFont(const FontSize size)
    {
        ImFont* font = [](const FontSize size) -> ImFont* {
            switch (size) {
                case FontSize::widget_large:
                    return font_widget_large;
                case FontSize::widget_small:
                    return font_widget_small;
                case FontSize::widget_label:
                    return font_widget_label;
                case FontSize::header1:
                    return font_header1;
                case FontSize::header2:
                    return font_header2;
                case FontSize::text:
                    return font_text;
                default:
                    return nullptr;
            }
        }(size);

        if (font && font->IsLoaded()) {
            return font;
        }

        const auto& io = ImGui::GetIO();
        return io.Fonts->Fonts[0];
    }
}
