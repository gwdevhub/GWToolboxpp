#pragma once

#ifndef DLLAPI
#ifdef GWCA_IMPORT
#define DLLAPI __declspec(dllimport)
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
    // Cycle through our own fonts, release any valid textures
    bool ReleaseFontTextures();
    // Cycle through our own fonts, create any missing textures
    bool CreateFontTextures();
    bool FontsLoaded();
    void LoadFonts(bool force = false);
    DLLAPI ImFont* GetFont(FontSize size);

    void Terminate();
}
