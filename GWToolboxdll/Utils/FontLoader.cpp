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
        const std::wstring glyph_ranges;
        std::wstring font_name;
        size_t data_size = 0;
        void* data = nullptr;
        bool compressed = false;
    };

    std::vector<FontData> font_data;
    std::wstring all_available_glyph_ranges;
    std::vector<ImWchar*> allocated_glyph_ranges;

    const ImWchar fontawesome5_glyph_ranges[] = {ICON_MIN_FA, ICON_MAX_FA, 0};

    std::unordered_map<GW::Constants::Language, ImFontAtlas*> font_atlas_by_language;

    std::wstring GlyphBuilderToWstring(ImFontGlyphRangesBuilder& builder)
    {
        ImVector<ImWchar> ranges_vec;
        builder.BuildRanges(&ranges_vec);
        return (const wchar_t*)ranges_vec.Data;
    }

    ImFontGlyphRangesBuilder GetGWGlyphRange()
    {
        ImFontGlyphRangesBuilder builder;

        using GetGlyphRanges_pt = uint32_t*(*)();
        GetGlyphRanges_pt GetGlyphRanges_Func = 0;
        uintptr_t address = GW::Scanner::Find("\x50\x8d\x45\xc8\x50\x6a\x0d", "xxxxxxx", -0x5);
        GetGlyphRanges_Func = (GetGlyphRanges_pt)GW::Scanner::FunctionFromNearCall(address);

        if (GetGlyphRanges_Func) {
            const auto res = GetGlyphRanges_Func();
            std::vector<ImWchar> ranges;
            for (int i = 0; res[i]; i++) {
                ranges.push_back(static_cast<ImWchar>(res[i]));
                if (res[i] == 0xffe7) break;
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
        if (device->CreateTexture(width, height, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &new_texture, nullptr) < 0)
            return false;
        D3DLOCKED_RECT tex_locked_rect;
        if (new_texture->LockRect(0, &tex_locked_rect, nullptr, 0) != D3D_OK)
            return false;
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


    // Do dont loading into memory; run on a separate thread.
    void LoadFontsThread()
    {
        Hook_ImGui_ImplDX9_Functions();

        // This is hacky but i cba to use imgui's stupid ranges things; just use a wstring, its all 0 terminated anyway
        static_assert(sizeof(ImWchar) == sizeof(wchar_t), "ImWchar == wchar_t");

        fonts_loading = true;
        fonts_loaded = false;

        // Language decides glyph range
        //const auto current_language = (GW::Constants::Language)GW::UI::GetPreference(GW::UI::NumberPreference::TextLanguage);

        while (!GImGui) {
            Sleep(16);
        }

        printf("Loading fonts\n");

        auto& io = ImGui::GetIO();


        const std::vector<std::pair<const wchar_t*, const ImWchar*>> fonts_on_disk = {
            {L"Font.ttf", io.Fonts->GetGlyphRangesDefault()},
            {L"Font_Japanese.ttf", io.Fonts->GetGlyphRangesJapanese()},
            {L"Font_Cyrillic.ttf", io.Fonts->GetGlyphRangesCyrillic()},
            {L"Font_ChineseTraditional.ttf", io.Fonts->GetGlyphRangesChineseFull()},
            {L"Font_Korean.ttf", io.Fonts->GetGlyphRangesKorean()}
        };

        for (const auto& [font_name, glyph_range] : fonts_on_disk) {
            const auto path = Resources::GetPath(font_name);
            if (!std::filesystem::exists(path))
                continue;
            font_data.push_back({(wchar_t*)glyph_range, path});
        }

        font_data.push_back({(wchar_t*)fontawesome5_glyph_ranges, L"", fontawesome5_compressed_size, (void*)fontawesome5_compressed_data, true});

        auto find_glyph_range_intersection = [](std::vector<wchar_t>& range1, std::vector<wchar_t>& range2) {
            std::vector<ImWchar> intersection;
            if (range1.empty() || range2.empty()) return intersection;
            if (range1.back() != 0) range1.push_back(0);
            if (range2.back() != 0) range2.push_back(0);
            for (size_t i = 0; range1[i]; i += 2) {
                for (size_t j = 0; range2[j]; j += 2) {
                    const wchar_t start = std::max(range1[i], range2[j]);
                    const wchar_t end = std::min(range1[i + 1], range2[j + 1]);
                    if (start <= end) {
                        // Merge with previous range if possible
                        if (!intersection.empty() && start <= intersection.back() + 1) {
                            intersection.back() = std::max(intersection.back(), static_cast<ImWchar>(end));
                        }
                        else {
                            intersection.push_back(start);
                            intersection.push_back(end);
                        }
                    }
                }
            }
            intersection.push_back(0);
            return intersection;
        };

        auto add_font_set = [&find_glyph_range_intersection](ImFontConfig& cfg, ImFontAtlasFlags flags, const float size, [[maybe_unused]] const ImWchar* glyph_ranges_to_find) {
            const auto atlas = IM_NEW(ImFontAtlas);
            atlas->Flags = flags;

            cfg.MergeMode = false;
            for (const auto& font : font_data) {
                void* data = font.data;
                size_t data_size = font.data_size;

                auto glyphs_in_font_vec = std::vector(font.glyph_ranges.begin(), font.glyph_ranges.end());
                auto glyphs_to_find_vec = std::vector(
                    std::wstring_view(reinterpret_cast<const wchar_t*>(glyph_ranges_to_find)).begin(),
                    std::wstring_view(reinterpret_cast<const wchar_t*>(glyph_ranges_to_find)).end()
                );
                auto intersection = find_glyph_range_intersection(glyphs_in_font_vec, glyphs_to_find_vec);
                auto glyph_ranges_to_load_from_this_font = static_cast<ImWchar*>(IM_ALLOC(sizeof(ImWchar) * intersection.size() + 1));
                std::ranges::copy(intersection, glyph_ranges_to_load_from_this_font);
                allocated_glyph_ranges.push_back(glyph_ranges_to_load_from_this_font);

                if (!font.font_name.empty()) {
                    const auto path = Resources::GetPath(font.font_name);
                    data = ImFileLoadToMemory(path.string().c_str(), "rb", &data_size, 0);
                }

                if (font.compressed) {
                    atlas->AddFontFromMemoryCompressedTTF(data, data_size, size, &cfg, glyph_ranges_to_load_from_this_font);
                }
                else {
                    atlas->AddFontFromMemoryTTF(data, data_size, size, &cfg, glyph_ranges_to_load_from_this_font);
                }
                cfg.MergeMode = true; // for all but the first
            }
            atlas->Build();
            return atlas->Fonts.back();
        };

        ImFontGlyphRangesBuilder builder = GetGWGlyphRange();
        builder.AddRanges(fontawesome5_glyph_ranges);

        ImVector<ImWchar> language_specific_glyph_range;
        builder.BuildRanges(&language_specific_glyph_range);

        ImFontAtlasFlags flags = 0;
        flags |= ImFontAtlasFlags_NoPowerOfTwoHeight;
        flags |= ImFontAtlasFlags_NoMouseCursors;
        flags |= ImFontAtlasFlags_NoBakedLines;

        static ImFontConfig cfg;
        cfg.PixelSnapH = true;
        cfg.OversampleH = 2; // OversampleH = 2 for base text size (harder to read if OversampleH < 2)
        cfg.OversampleV = 1;

        while (io.Fonts->Locked) {
            Sleep(16);
        }

        if (font_header2) {
            IM_FREE(font_header2->ContainerAtlas);
        }
        font_header2 = add_font_set(cfg, flags, static_cast<float>(FontLoader::FontSize::header2), language_specific_glyph_range.Data); // 18.f

        if (font_header1)
            IM_FREE(font_header2->ContainerAtlas);
        font_header1 = add_font_set(cfg, flags, static_cast<float>(FontLoader::FontSize::header1), language_specific_glyph_range.Data); // 20.f

        cfg.OversampleH = 1;
        if (font_widget_label)
            IM_FREE(font_widget_label->ContainerAtlas);
        font_widget_label = add_font_set(cfg, flags, static_cast<float>(FontLoader::FontSize::widget_label), language_specific_glyph_range.Data); // 24.f

        if (font_widget_small)
            IM_FREE(font_widget_small->ContainerAtlas);
        font_widget_small = add_font_set(cfg, flags, static_cast<float>(FontLoader::FontSize::widget_small), language_specific_glyph_range.Data); // 40.f

        if (font_widget_large)
            IM_FREE(font_widget_large->ContainerAtlas);
        font_widget_large = add_font_set(cfg, flags, static_cast<float>(FontLoader::FontSize::widget_large), language_specific_glyph_range.Data); // 48.f

        if (font_text)
            IM_FREE(font_text->ContainerAtlas);
        font_text = add_font_set(cfg, flags, static_cast<float>(FontLoader::FontSize::text), language_specific_glyph_range.Data); // 16.f

        ImGui_ImplDX9_InvalidateDeviceObjects();

        io.Fonts = font_text->ContainerAtlas;

        printf("Fonts loaded\n");
        fonts_loaded = true;
        fonts_loading = false;

        // Free allocated glyph ranges
        for (const auto glyph_range : allocated_glyph_ranges) {
            IM_FREE(glyph_range);
        }
        allocated_glyph_ranges.clear();
    }
}

namespace FontLoader {
    bool ReleaseFontTextures()
    {
        if (!GImGui)
            return false;

        for (auto font : GetFonts()) {
            if (!font || !font->ContainerAtlas)
                continue;
            if (font->ContainerAtlas == ImGui::GetIO().Fonts)
                continue; // Fallback function will handle this one
            LPDIRECT3DTEXTURE9 texture = (LPDIRECT3DTEXTURE9)font->ContainerAtlas->TexID;
            if (texture) {
                texture->Release();
                font->ContainerAtlas->SetTexID(0);
            }
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
    void LoadFonts(bool)
    {
        if (fonts_loading) {
            return;
        }
        if (fonts_loaded) {
            return;
        }
        fonts_loading = true;
        fonts_loaded = false;

        std::thread t(LoadFontsThread);
        t.detach();
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
