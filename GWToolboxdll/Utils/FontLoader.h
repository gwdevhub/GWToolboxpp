#pragma once

#ifndef DLLAPI
#ifdef GWCA_IMPORT
#define DLLAPI
#else
#define DLLAPI extern "C" __declspec(dllexport)
#endif
#endif

namespace FontLoader {
    enum class FontSize {
        text         = 16,
        header2      = 18,
        header1      = 20,
        widget_label = 24,
        widget_small = 40,
        widget_large = 48
    };

    constexpr float text_size_min = 0.f;
    constexpr float text_size_max = static_cast<float>(FontSize::widget_large);

    constexpr std::array font_sizes = {
        FontSize::text,
        FontSize::header2,
        FontSize::header1,
        FontSize::widget_label,
        FontSize::widget_small,
        FontSize::widget_large
    };
    constexpr std::array font_size_names = {"16", "18", "20", "24", "40", "48"};

    // Cycle through our own fonts, release any valid textures
    bool ReleaseFontTextures();
    // Cycle through our own fonts, create any missing textures
    bool CreateFontTextures();
    bool FontsLoaded();
    void LoadFonts(bool force = false);
    DLLAPI ImFont* GetFont(FontSize size);

    // Given an ideal font size in px, return the best fit font.
    // NOTE: This font will NOT be automatically resized to the desired scale.
    DLLAPI ImFont* GetFontByPx(float size_in_px, bool include_global_font_scale = true);

    void Terminate();
}
