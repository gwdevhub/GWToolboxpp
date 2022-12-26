#pragma once

#include <Widgets/Minimap/VBuffer.h>

class SymbolsRenderer : public VBuffer {
public:
    SymbolsRenderer() = default;

    void Render(IDirect3DDevice9* device) override;

    void DrawSettings();
    void LoadSettings(ToolboxIni* ini, const char* section);
    void SaveSettings(ToolboxIni* ini, const char* section) const;

private:

    void Initialize(IDirect3DDevice9* device) override;

    Color color_quest = 0;
    Color color_north = 0;
    Color color_modifier = 0;

    const DWORD star_ntriangles = 16;
    DWORD star_offset = 0;

    const DWORD arrow_ntriangles = 2;
    DWORD arrow_offset = 0;

    const DWORD north_ntriangles = 2;
    DWORD north_offset = 0;

    bool initialized = false;
};
